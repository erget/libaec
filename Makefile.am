ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src tests
EXTRA_DIST = doc/license.txt doc/logo.svg CMakeLists.txt
bench: all
	cd tests && $(MAKE) $(AM_MAKEFLAGS) bench
benc: all
	cd tests && $(MAKE) $(AM_MAKEFLAGS) benc
bdec: all
	cd tests && $(MAKE) $(AM_MAKEFLAGS) bdec
.PHONY: bench benc bdec
