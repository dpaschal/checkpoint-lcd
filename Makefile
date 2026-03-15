# cpanel — Check Point P-210/12200 LCD Panel Driver
# Works on Linux (gmake) and FreeBSD (bmake/make)

CC      ?= cc
CFLAGS  ?= -O2 -Wall -std=c11 -D_DEFAULT_SOURCE

.PHONY: all clean install

all: cpanel

cpanel.o: cpanel.c cpanel.h
	$(CC) $(CFLAGS) -c cpanel.c -o cpanel.o

cpanel-cli.o: cpanel-cli.c cpanel.h
	$(CC) $(CFLAGS) -c cpanel-cli.c -o cpanel-cli.o

cpanel: cpanel-cli.o cpanel.o
	$(CC) -o cpanel cpanel-cli.o cpanel.o

clean:
	rm -f *.o cpanel

install: cpanel
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 cpanel $(DESTDIR)/usr/local/bin/
