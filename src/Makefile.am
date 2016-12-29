### Makefile.am
include $(top_builddir)/version.mk

AM_CFLAGS = $(EXTRA_CFLAGS)
AM_CPPFLAGS = -D_POSIX_C_SOURCE=201001L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
AM_CPPFLAGS += -DHAVE_VERSION_H
AM_LDFLAGS = $(XCCLDFLAGS)

bin_PROGRAMS =
noinst_PROGRAMS =
noinst_LTLIBRARIES =
BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)
CLEANFILES =
SUFFIXES =

bin_PROGRAMS += hitwss
hitwss_SOURCES = hitwss.c hitwss.yuck
hitwss_SOURCES += nifty.h
hitwss_SOURCES += version.c version.h
hitwss_CPPFLAGS = $(AM_CPPFLAGS)
hitwss_CPPFLAGS += $(ev_CFLAGS)
hitwss_LDFLAGS = $(AM_LDFLAGS)
hitwss_LDFLAGS += $(ev_LIBS)
BUILT_SOURCES += hitwss.yucc

bin_PROGRAMS += hit2b
hit2b_SOURCES = hit2b.c hit2b.yuck
hit2b_SOURCES += jsmn.c jsmn.h
hit2b_SOURCES += hash.c hash.h
hit2b_SOURCES += version.c version.h
hit2b_SOURCES += boobs.h
hit2b_SOURCES += nifty.h
BUILT_SOURCES += hit2b.yucc

bin_PROGRAMS += cexwss
cexwss_SOURCES = cexwss.c cexwss.yuck
cexwss_SOURCES += nifty.h
cexwss_SOURCES += tls.c tls.h
cexwss_SOURCES += version.c version.h
cexwss_SOURCES += ../cex_cred.h
cexwss_CPPFLAGS = $(AM_CPPFLAGS)
cexwss_CPPFLAGS += $(ev_CFLAGS)
cexwss_LDFLAGS = $(AM_LDFLAGS)
cexwss_LDFLAGS += $(ev_LIBS)
cexwss_LDADD = -lssl -lcrypto
BUILT_SOURCES += cexwss.yucc

bin_PROGRAMS += cex2b
cex2b_SOURCES = cex2b.c cex2b.yuck
cex2b_SOURCES += jsmn.c jsmn.h
cex2b_SOURCES += hash.c hash.h
cex2b_SOURCES += version.c version.h
cex2b_SOURCES += boobs.h
cex2b_SOURCES += nifty.h
BUILT_SOURCES += cex2b.yucc

bin_PROGRAMS += plxwss
plxwss_SOURCES = plxwss.c plxwss.yuck
plxwss_SOURCES += nifty.h
plxwss_SOURCES += tls.c tls.h
plxwss_SOURCES += version.c version.h
plxwss_CPPFLAGS = $(AM_CPPFLAGS)
plxwss_CPPFLAGS += $(ev_CFLAGS)
plxwss_LDFLAGS = $(AM_LDFLAGS)
plxwss_LDFLAGS += $(ev_LIBS)
plxwss_LDADD = -lssl -lcrypto
BUILT_SOURCES += plxwss.yucc

bin_PROGRAMS += plx2b
plx2b_SOURCES = plx2b.c plx2b.yuck
plx2b_SOURCES += jsmn.c jsmn.h
plx2b_SOURCES += hash.c hash.h
plx2b_SOURCES += version.c version.h
plx2b_SOURCES += boobs.h
plx2b_SOURCES += nifty.h
BUILT_SOURCES += plx2b.yucc

bin_PROGRAMS += btsp
btsp_SOURCES = btsp.c btsp.yuck
btsp_SOURCES += tls.c tls.h
btsp_SOURCES += version.c version.h
btsp_SOURCES += boobs.h
btsp_SOURCES += nifty.h
btsp_CPPFLAGS = $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
btsp_LDFLAGS = $(AM_LDFLAGS)  $(LIBEV_LDFLAGS)
btsp_LDADD = $(LIBEV_LIBS) -lssl -lcrypto
BUILT_SOURCES += btsp.yucc

bin_PROGRAMS += btsp2b
btsp2b_SOURCES = btsp2b.c btsp2b.yuck
btsp2b_SOURCES += jsmn.c jsmn.h
btsp2b_SOURCES += hash.c hash.h
btsp2b_SOURCES += version.c version.h
btsp2b_SOURCES += boobs.h
btsp2b_SOURCES += nifty.h
BUILT_SOURCES += btsp2b.yucc

bin_PROGRAMS += bitfin
bitfin_SOURCES = bitfin.c bitfin.yuck
bitfin_SOURCES += nifty.h
bitfin_SOURCES += tls.c tls.h
bitfin_SOURCES += version.c version.h
bitfin_CPPFLAGS = $(AM_CPPFLAGS)
bitfin_CPPFLAGS += $(ev_CFLAGS)
bitfin_LDFLAGS = $(AM_LDFLAGS)
bitfin_LDFLAGS += $(ev_LIBS)
bitfin_LDADD = $(LIBEV_LIBS) -lssl -lcrypto
BUILT_SOURCES += bitfin.yucc

bin_PROGRAMS += bitfin2b
bitfin2b_SOURCES = bitfin2b.c bitfin2b.yuck
bitfin2b_SOURCES += version.c version.h
bitfin2b_SOURCES += boobs.h
bitfin2b_SOURCES += nifty.h
BUILT_SOURCES += bitfin2b.yucc


CLEANFILES += version.c
EXTRA_DIST += version.c.in


## version rules
version.c: version.c.in $(top_builddir)/.version
	$(AM_V_GEN) PATH="$(top_builddir)/build-aux:$${PATH}" \
		yuck$(EXEEXT) scmver --ignore-noscm --force -o $@ \
			--use-reference --reference $(top_builddir)/.version $<

## yuck rule
SUFFIXES += .yuck
SUFFIXES += .yucc
.yuck.yucc:
	$(AM_V_GEN) PATH=$(top_builddir)/build-aux:"$${PATH}" \
		yuck$(EXEEXT) gen -o $@ $<

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<

## Makefile.am ends here
