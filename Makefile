BUILDSYS := $(shell uname -s)

# Only use host argument if cross-compiling
ifneq ($(BUILDSYS),AmigaOS)
	HOSTARG := --host=ppc-amigaos
else
	HOSTARG := 
endif

CAIRODIR := cairo-1.14.10

.PHONY: all
all: build-cairo

cairo-build/Makefile: $(CAIRODIR)/configure
	mkdir -p cairo-build
	rm -rf cairo-build/*
	cd cairo-build && ../$(CAIRODIR)/configure --prefix=/SDK/local/newlib $(HOSTARG) --disable-shared --enable-amigaos LIBS=-lauto

.PHONY: build-cairo
build-cairo: cairo-build/Makefile
	$(MAKE) -C cairo-build

.PHONY: clean
clean:
	rm -rf cairo-build

