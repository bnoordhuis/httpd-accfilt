/*
 * Copyright (c) 2011, Ben Noordhuis <info@bnoordhuis.nl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

/* prefer speed over correctness, this is a benchmark after all */
#define HTTP_PARSER_STRICT 0

#include "http_parser.h"
#include "ev.h"

#define container_of(ptr, type, member) \
  ((type *) (((char *) ptr) - offsetof(type, member)))

#define assert_errno()          \
  do {                          \
    if (errno != 0) {           \
      fprintf(stderr,           \
              "%s:%d: %s\n",    \
              __FILE__,         \
              __LINE__,         \
              strerror(errno)); \
      abort();                  \
    }                           \
  }                             \
  while (0);

#define E(expr)     \
  do {              \
    errno = 0;      \
    (expr);         \
    assert_errno(); \
  }                 \
  while (0)

#define CRLF "\r\n"

typedef struct {
  const char *data;
  size_t size;
  size_t out;
} response_t;

typedef struct {
  http_parser parser;
  response_t res;
  ev_io watcher;
} client_t;


static http_parser_settings parser_settings;

static const char response[] =
  "HTTP/1.1 200 OK" CRLF
  "Server: quickie/0.1" CRLF
  "Date: Fri, 26 Aug 2011 00:31:53 GMT" CRLF
  "Content-Type: text/plain" CRLF
  "Content-Length: 4" CRLF
  CRLF
  "OK" CRLF
  ;


static void setnonblock(int sockfd) {
  int flags;
  E(flags = fcntl(sockfd, F_GETFL));
  E(fcntl(sockfd, F_SETFL, O_NONBLOCK | flags));
}


static void close_conn(client_t *cl) {
  ev_io_stop(EV_DEFAULT_UC_ &cl->watcher);
  E(close(cl->watcher.fd)); /* maybe handle EINTR here? */
  free(cl);
}


static void on_read(EV_P_ ev_io* w, int revents) {
  char buf[16384];
  ssize_t nread;
  client_t *cl;

  assert(!(revents & ~EV_READ));

  cl = container_of(w, client_t, watcher);

  while (1) {
    do {
      nread = read(cl->watcher.fd, buf, sizeof buf);
    }
    while (nread == -1 && errno == EINTR);

    if (nread == -1) {
      if (!(errno == EAGAIN || errno == EWOULDBLOCK))
        close_conn(cl);
      break;
    }

    if (nread == 0) {
      close_conn(cl);
      break;
    }

    if (http_parser_execute(&cl->parser,
                            &parser_settings,
                            buf,
                            (size_t) nread) != (size_t) nread) {
      close_conn(cl);
      break;
    }
  }
}


static void on_write(EV_P_ ev_io* w, int revents) {
  ssize_t nwritten;
  client_t *cl;

  assert(!(revents & ~EV_WRITE));

  cl = container_of(w, client_t, watcher);

  do {
    do {
      nwritten = write(cl->watcher.fd,
                       cl->res.data + cl->res.out,
                       cl->res.size - cl->res.out);
    }
    while (nwritten == -1 && errno == EINTR);

    if (nwritten == -1) {
      if (!(errno == EAGAIN || errno == EWOULDBLOCK))
        close_conn(cl);
      break;
    }

    cl->res.out += nwritten;
  }
  while (cl->res.out < cl->res.size);

  if (cl->res.out == cl->res.size)
    close_conn(cl);
}


static void on_accept(EV_P_ ev_io* w, int revents) {
  struct sockaddr_in addr;
  socklen_t addrlen;
  client_t *cl;
  int peerfd;

  assert(!(revents & ~EV_READ));

  addrlen = sizeof addr;
  E(peerfd = accept(w->fd, (struct sockaddr *) &addr, &addrlen));
  setnonblock(peerfd);

  E(cl = calloc(1, sizeof *cl));
  http_parser_init(&cl->parser, HTTP_REQUEST);

  ev_io_init(&cl->watcher, on_read, peerfd, EV_READ);
  ev_io_start(EV_DEFAULT_UC_ &cl->watcher);
}


static int on_message_complete(http_parser *parser) {
  client_t *cl;

  cl = container_of(parser, client_t, parser);

  cl->res.data = response;
  cl->res.size = sizeof(response) - 1;
  cl->res.out = 0;

  ev_io_stop(EV_DEFAULT_UC_ &cl->watcher);
  ev_io_init(&cl->watcher, on_write, cl->watcher.fd, EV_WRITE);
  ev_io_start(EV_DEFAULT_UC_ &cl->watcher);

  return 0;
}


#if 0
static int on_message_begin(http_parser *parser) {
  puts(__func__);
  return 0;
}
static int on_url(http_parser *parser, const char *at, size_t length) {
  puts(__func__);
  return 0;
}
static int on_header_field(http_parser *parser, const char *at, size_t length) {
  puts(__func__);
  return 0;
}
static int on_header_value(http_parser *parser, const char *at, size_t length) {
  puts(__func__);
  return 0;
}
static int on_headers_complete(http_parser *parser) {
  puts(__func__);
  return 0;
}
static int on_body(http_parser *parser, const char *at, size_t length) {
  puts(__func__);
  return 0;
}
#endif


int main(int argc, char **argv) {
  int sockfd;
  ev_io w;

  memset(&parser_settings, 0, sizeof parser_settings);
#if 0
  parser_settings.on_message_begin = on_message_begin;
  parser_settings.on_url = on_url;
  parser_settings.on_header_field = on_header_field;
  parser_settings.on_header_value = on_header_value;
  parser_settings.on_headers_complete = on_headers_complete;
  parser_settings.on_body = on_body;
#endif
  parser_settings.on_message_complete = on_message_complete;

  if (!ev_default_loop(0))
    abort();

  E(sockfd = socket(AF_INET, SOCK_STREAM, 0));
  setnonblock(sockfd);

  {
    int yes = 1;
    E(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes));
  }
  {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8000);
    addr.sin_addr.s_addr = INADDR_ANY;
    E(bind(sockfd, (struct sockaddr *) &addr, sizeof addr));
  }

  E(listen(sockfd, SOMAXCONN));

  printf("Listening on http://0.0.0.0:8000/\n");

  ev_io_init(&w, on_accept, sockfd, EV_READ);
  ev_io_start(EV_DEFAULT_UC_ &w);

  ev_run(EV_DEFAULT_UC_ 0);

  return 0;
}
