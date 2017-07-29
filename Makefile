CAIRODIR := cairo-1.14.10

.PHONY: all
all: cairo-build/src/.libs/libcairo.a

cairo-build:
	mkdir cairo-build

cairo-build/src/.libs/libcairo.a: cairo-build
	cd cairo-build && ../$(CAIRODIR)/configure --prefix=/SDK/local/newlib --host=ppc-amigaos --disable-shared
	$(MAKE) -C cairo-build

.PHONY: clean
clean:
	rm -rf cairo-build

