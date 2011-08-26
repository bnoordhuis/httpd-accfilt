CC	= gcc
CFLAGS	= -I. -Iev -Wall -Wextra -Wno-unused-parameter
LDFLAGS	= -lm

REL_CFLAGS	= -O2 -Os
DBG_CFLAGS	= -O0 -g

OBJS	= http_parser.o
LIBEV	= ev/.libs/libev.a

.PHONY:	all ev clean

all:	release

release:	CFLAGS += $(REL_CFLAGS)
release:	build

debug:	CFLAGS += $(DBG_CFLAGS)
debug:	build

build:	httpd-accfilt httpd-nofilt

httpd-accfilt:	$(OBJS) httpd-accfilt.o $(LIBEV)
	$(CC) -o $@ $^ $(LDFLAGS)

httpd-nofilt:	$(OBJS) httpd-nofilt.o $(LIBEV)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) httpd-accfilt.o httpd-nofilt.o httpd-accfilt httpd-nofilt

distclean:	clean
	cd ev && $(MAKE) distclean

$(LIBEV):	ev/Makefile
	cd ev && $(MAKE)

ev/Makefile:	ev/configure
	cd ev && ./configure --disable-shared

ev/configure:
	cd ev && sh autogen.sh

httpd-accfilt.o:	httpd.c
	$(CC) $(CFLAGS) -DHAVE_ACCFILT -o $@ -c $<

httpd-nofilt.o:	httpd.c
	$(CC) $(CFLAGS) -UHAVE_ACCFILT -o $@ -c $<
