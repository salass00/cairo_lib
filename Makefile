CAIRODIR := cairo-1.14.10

CC    := ppc-amigaos-gcc
STRIP := ppc-amigaos-strip

CFLAGS  := -O2 -g -Wall -Wwrite-strings -Werror -I/SDK/local/newlib/include/cairo
LDFLAGS := -static
LIBS    := -lcairo -lfreetype -lpng -lbz2 -lz -lpixman-1 -lauto

BUILDSYS := $(shell uname -s)

# Only use host argument if cross-compiling
ifneq ($(BUILDSYS),AmigaOS)
	HOSTARG := --host=ppc-amigaos
else
	HOSTARG := 
endif

TESTS := tests/rectangles tests/lines tests/text

.PHONY: all
all: build-cairo

cairo-build/Makefile: $(CAIRODIR)/configure
	mkdir -p cairo-build
	rm -rf cairo-build/*
	cd cairo-build && ../$(CAIRODIR)/configure --prefix=/SDK/local/newlib $(HOSTARG) --disable-shared --enable-amigaos --enable-amigaos-font LIBS=-lauto

.PHONY: build-cairo
build-cairo: cairo-build/Makefile
	$(MAKE) -C cairo-build

.PHONY: build-tests
build-tests: $(TESTS)

tests/rectangles: tests/rectangles.c
	$(CC) $(LDFLAGS) -o $@.debug $(CFLAGS) $^ $(LIBS)
	$(STRIP) -R.comment -o $@ $@.debug

tests/lines: tests/lines.c
	$(CC) $(LDFLAGS) -o $@.debug $(CFLAGS) $^ $(LIBS)
	$(STRIP) -R.comment -o $@ $@.debug

tests/text: tests/text.c
	$(CC) $(LDFLAGS) -o $@.debug $(CFLAGS) $^ $(LIBS)
	$(STRIP) -R.comment -o $@ $@.debug

.PHONY: clean
clean:
	rm -rf cairo-build
	rm -f $(TESTS) tests/*.debug

.PHONY: install
install: cairo-build/Makefile
	$(MAKE) -C cairo-build install

