CC	= gcc
CFLAGS	= -I. -Iev -Wall -Wextra -O0 -g
LDFLAGS	=

OBJS	= \
	http_parser.o \
	httpd.o

LIBEV	= ev/.libs/libev.a

.PHONY:	all ev clean

all:	$(LIBEV) $(OBJS)
	$(CC) -DHAVE_ACCFILT -o httpd-accfilt $^ $(LDFLAGS)
	$(CC) -UHAVE_ACCFILT -o httpd-nofilt $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) httpd-accfilt httpd-nofilt

distclean:	clean

$(LIBEV):	ev/Makefile
	cd ev && $(MAKE)

ev/Makefile:	ev/configure
	cd ev && ./configure --disable-shared

ev/configure:
	cd ev && sh autogen.sh
