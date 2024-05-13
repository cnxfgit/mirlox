all:
	cd mir && $(MAKE)
	cd src && $(MAKE) $@

clean:
	cd mir && $(MAKE) $@
	cd src && $(MAKE) $@