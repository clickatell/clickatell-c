#
# This Makefile creates a static library file:  lib/libclickatell_sms.a
# The static library can me linked into your application.
#
SHELL = /bin/sh
RANLIB = ranlib

includedir = ./

CC=gcc
LIBS=-lrt -lresolv -lnsl -lm -L/usr/lib64
CFLAGS=-D_REENTRANT=1 -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -static -ggdb -O2 -I. -I$(includedir)
LDFLAGS= -rdynamic

MKDEPEND=$(CC) $(CFLAGS) -MM

progsrcs = clickatell_sms.c clickatell_debug.c clickatell_string.c
progobjs = $(progsrcs:.c=.o)
progs = $(progsrcs:.c=)

staticlib = lib/libclickatell_sms.a

libsrcs = $(wildcard *.c)
libobjs = $(libsrcs:.c=.o)
libsobj = $(libsrcs:.c=.so)

cleanfiles = $(progobjs) $(progs) $(staticlib)

srcdirs = .

.SUFFIXES: .c .o .so

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

.c.so:
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

all: $(staticlib) $(libsobj)

clean:
	rm -f $(cleanfiles)

# this archives the object files into our library
$(staticlib): $(libobjs)
	$(AR) rc $(staticlib) $(libobjs)
	$(RANLIB) $(staticlib)
