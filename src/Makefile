#
# This Makefile creates an executable binary (called test_clickatell_sms) that is linked with the Clickatell
# SMS module library file (lib/clickatell_sms.a)
# test_clickatell_sms can be run without parameters. It will run a sequence of HTTP
# and REST tests demonstrating how to use these APIs.
# Before compiling test_clickatell_sms, please first edit the config settings in file test_clickatell_sms.c, so
# that the correct login credentials are applied according to your Clickatell user account and Clickatell
# api ID (be that REST or HTTP).
#
SHELL = /bin/sh
RANLIB = ranlib

includedir = ./

CC=gcc
LIBS=-lrt -lresolv -lnsl -lm -lpthread -ldl -L/usr/lib64 -lcurl -L/usr/lib -lxml2
CFLAGS=-D_REENTRANT=1 -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -ggdb -O2 -I. -I$(includedir)
LDFLAGS= -rdynamic

progsrcs = test_clickatell_sms.c
progobjs = $(progsrcs:.c=.o)
progs = $(progsrcs:.c=)

psrcs = $(wildcard *.c)
pobjs = $(filter-out $(progobjs), $(psrcs:.c=.o))

libs = $(pobjs) ./clickatell_sms/lib/libclickatell_sms.a

cleanfiles = $(progobjs) $(progs) $(pobjs)

srcdirs = .

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

all: $(progs)

clean:
	rm -f $(cleanfiles)

$(progs): $(libs) $(progobjs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(@:=).o $(libs) $(LIBS)
