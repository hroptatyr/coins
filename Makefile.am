include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =
EXTRA_DIST = $(DISTCLEANFILES) $(doc_DATA)
DISTCLEANFILES =
CLEANFILES =
doc_DATA =
BUILT_SOURCES =

SUBDIRS += build-aux
SUBDIRS += src

DISTCLEANFILES += version.mk
DISTCLEANFILES += .version
EXTRA_DIST += version.mk.in

## make sure .version is read-only in the dist
dist-hook:
	chmod ugo-w $(distdir)/.version
	cp $(top_srcdir)/dummy_cred.h $(distdir)/cex_cred.h
	cp $(top_srcdir)/dummy_cred.h $(distdir)/okcoin_cred.h
	cp $(top_srcdir)/dummy_cred.h $(distdir)/deribit_cred.h

BUILT_SOURCES += cex_cred.h
BUILT_SOURCES += okcoin_cred.h
EXTRA_DIST += dummy_cred.h
cex_cred.h okcoin_cred.h deribit_cred.h: dummy_cred.h
	cp $< $@

## Makefile.am ends here
