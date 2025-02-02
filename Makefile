CFLAGS := -Wall -Wextra -Wvla -Wsign-conversion -pedantic -std=c99
APPNAME := lpcli

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin

PLATFORM := posix
ifeq ($(HAS_XCLIP), 1)
    CFLAGS += -DUSE_XCLIP
else
    CFLAGS += -lX11
endif

DEPS := lpcli.h lp.h pbkdf2_sha256.h
CODE := lpcli_$(PLATFORM).c lpcli.c

.PHONY: all clean install uninstall

all: $(APPNAME)

$(APPNAME): $(DEPS) $(CODE)
	$(CC) $(CFLAGS) -o $@ $(CODE)

# Installation targets
install: $(APPNAME)
	install -D -m755 $(APPNAME) $(DESTDIR)$(BINDIR)/$(APPNAME)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(APPNAME)

clean:
	rm -f $(APPNAME)
