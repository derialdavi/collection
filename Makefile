CC=gcc
CFLAGS=-Wall -Wextra -Werror -O2 -std=c2x -shared -fPIC
DEBFLAGS=-g

SODIR=./so
SRCDIR=./src
LIBDIR=$(shell gcc -print-file-name=libc.so | xargs dirname)

LIBNAME=collection

SRCS=$(wildcard $(SRCDIR)/*.c)
HEADERS=$(wildcard $(SRCDIR)/*.h)
COLLECTION_SO=lib$(LIBNAME).so
COLLECTION_DEBUG_SO=$(SODIR)/lib$(LIBNAME)_debug.so

.PHONY: all debug install uninstall clean

all: $(COLLECTION_SO)

debug: $(COLLECTION_DEBUG_SO)

install: $(COLLECTION_SO)
	install -m 755 $(SODIR)/$(COLLECTION_SO) $(LIBDIR)
	install -m 644 $(SRCDIR)/*.h /usr/include

uninstall:
	rm -f $(LIBDIR)/$(COLLECTION_SO) /usr/include/$(HEADERS)

$(COLLECTION_SO): $(SRCS)
	@mkdir -p $(SODIR)
	$(CC) $(CFLAGS) $(SRCS) -o $(SODIR)/$@

$(COLLECTION_DEBUG_SO): $(SRCS)
	@mkdir -p $(SODIR)
	$(CC) $(CFLAGS) $(DEBFLAGS) $(SRCS) -o $@

clean:
	rm -rf $(SODIR)/* $(BINDIR)/*
