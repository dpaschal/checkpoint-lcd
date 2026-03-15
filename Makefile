# EZIO-G500 LCD Driver for Check Point P-210/12200
#
# Build: make              (native, Linux or FreeBSD)
# Cross: make CC=cc CFLAGS="-O2" TARGET=freebsd   (for OPNsense)
# Test:  make test         (runs on Linux with /dev/ttyS1)

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
LDFLAGS ?=

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin

# Source files
LIB_SRC  = ezio.c
CLI_SRC  = ezio-cli.c
LIB_OBJ  = $(LIB_SRC:.c=.o)
CLI_OBJ  = $(CLI_SRC:.c=.o)

# Targets
LIB      = libezio.a
CLI      = ezio-cli

.PHONY: all clean install

all: $(LIB) $(CLI)

$(LIB): $(LIB_OBJ)
	$(AR) rcs $@ $^

$(CLI): $(CLI_OBJ) $(LIB)
	$(CC) $(LDFLAGS) -o $@ $(CLI_OBJ) -L. -lezio -lm

%.o: %.c ezio.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(LIB) $(CLI)

install: $(CLI)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(CLI) $(DESTDIR)$(BINDIR)/
