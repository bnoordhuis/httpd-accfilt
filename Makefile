CC	= gcc
CFLAGS	= -I. -Iev -Wall -Wextra -Wno-unused-parameter
LDFLAGS	= -lm

REL_CFLAGS	= -O2 -Os
DBG_CFLAGS	= -O0 -g

OBJS	= httpd.o http_parser.o
LIBEV	= ev/.libs/libev.a

.PHONY:	all ev clean

all:	release

release:	CFLAGS += $(REL_CFLAGS)
release:	build

debug:	CFLAGS += $(DBG_CFLAGS)
debug:	build

build:	$(OBJS)	$(LIBEV)
	$(CC) -DHAVE_ACCFILT -o httpd-accfilt $^ $(LDFLAGS)
	$(CC) -UHAVE_ACCFILT -o httpd-nofilt $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) httpd-accfilt httpd-nofilt

distclean:	clean
	cd ev && $(MAKE) distclean

$(LIBEV):	ev/Makefile
	cd ev && $(MAKE)

ev/Makefile:	ev/configure
	cd ev && ./configure --disable-shared

ev/configure:
	cd ev && sh autogen.sh
