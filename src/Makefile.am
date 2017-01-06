### Makefile.am
include $(top_builddir)/version.mk

AM_CFLAGS = $(EXTRA_CFLAGS)
AM_CPPFLAGS = -D_POSIX_C_SOURCE=201001L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
AM_CPPFLAGS += -DHAVE_VERSION_H
AM_LDFLAGS = $(XCCLDFLAGS)

bin_PROGRAMS =
noinst_PROGRAMS =
noinst_LIBRARIES =
BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)
CLEANFILES =
SUFFIXES =

noinst_LIBRARIES += libws.a
libws_a_SOURCES =
libws_a_SOURCES += ws.c ws.h
libws_a_SOURCES += tls.c tls.h
ws_LIBS = -lssl -lcrypto

bin_PROGRAMS += coin
coin_SOURCES = coin.c coin.yuck
coin_SOURCES += tls.c tls.h
coin_SOURCES += nifty.h
coin_SOURCES += version.c version.h
coin_CPPFLAGS = $(AM_CPPFLAGS)
coin_CPPFLAGS += $(ev_CFLAGS)
coin_LDFLAGS = $(AM_LDFLAGS)
coin_LDFLAGS += $(ev_LIBS)
coin_LDFLAGS += $(ws_LIBS)
coin_LDADD = libws.a
BUILT_SOURCES += coin.yucc

bin_PROGRAMS += coin2b
coin2b_SOURCES = coin2b.c coin2b.yuck
coin2b_SOURCES += jsmn.c jsmn.h
coin2b_SOURCES += hash.c hash.h
coin2b_SOURCES += version.c version.h
BUILT_SOURCES += coin2b.yucc

bin_PROGRAMS += hitwss
hitwss_SOURCES = hitwss.c hitwss.yuck
hitwss_SOURCES += nifty.h
hitwss_SOURCES += version.c version.h
hitwss_CPPFLAGS = $(AM_CPPFLAGS)
hitwss_CPPFLAGS += $(ev_CFLAGS)
hitwss_LDFLAGS = $(AM_LDFLAGS)
hitwss_LDFLAGS += $(ev_LIBS)
hitwss_LDFLAGS += $(ws_LIBS)
hitwss_LDADD = libws.a
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
plxwss_LDFLAGS += $(ws_LIBS)
plxwss_LDADD = libws.a
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
btsp_CPPFLAGS = $(AM_CPPFLAGS)
btsp_CPPFLAGS += $(ev_CFLAGS)
btsp_LDFLAGS = $(AM_LDFLAGS)
btsp_LDFLAGS += $(ev_LIBS)
btsp_LDFLAGS += $(ws_LIBS)
btsp_LDADD = libws.a
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

bin_PROGRAMS += okfix
okfix_SOURCES = okfix.c okfix.yuck
okfix_SOURCES += nifty.h
okfix_SOURCES += version.c version.h
okfix_SOURCES += ../okcoin_cred.h
okfix_SOURCES += fix.c fix.h
okfix_SOURCES += tls.c tls.h
okfix_CPPFLAGS = $(AM_CPPFLAGS)
okfix_CPPFLAGS += $(ev_CFLAGS)
okfix_LDFLAGS = $(AM_LDFLAGS)
okfix_LDFLAGS += $(ev_LIBS)
okfix_LDADD = $(LIBEV_LIBS) -lssl -lcrypto
BUILT_SOURCES += okfix.yucc

bin_PROGRAMS += ok2b
ok2b_SOURCES = ok2b.c ok2b.yuck
ok2b_SOURCES += nifty.h
ok2b_SOURCES += version.c version.h
ok2b_SOURCES += fix.c fix.h
ok2b_CPPFLAGS = $(AM_CPPFLAGS)
ok2b_CPPFLAGS += -DSOH='"|"'
ok2b_LDFLAGS = $(AM_LDFLAGS)
BUILT_SOURCES += ok2b.yucc

bin_PROGRAMS += btcc
btcc_SOURCES = btcc.c btcc.yuck
btcc_SOURCES += nifty.h
btcc_SOURCES += version.c version.h
btcc_SOURCES += fix.c fix.h
btcc_SOURCES += tls.c tls.h
btcc_CPPFLAGS = $(AM_CPPFLAGS)
btcc_CPPFLAGS += $(ev_CFLAGS)
btcc_LDFLAGS = $(AM_LDFLAGS)
btcc_LDFLAGS += $(ev_LIBS)
btcc_LDADD = $(LIBEV_LIBS) -lssl -lcrypto
BUILT_SOURCES += btcc.yucc

bin_PROGRAMS += btcc2b
btcc2b_SOURCES = btcc2b.c btcc2b.yuck
btcc2b_SOURCES += nifty.h
btcc2b_SOURCES += version.c version.h
btcc2b_SOURCES += fix.c fix.h
btcc2b_CPPFLAGS = $(AM_CPPFLAGS)
btcc2b_CPPFLAGS += -DSOH='"|"'
btcc2b_LDFLAGS = $(AM_LDFLAGS)
BUILT_SOURCES += btcc2b.yucc

bin_PROGRAMS += bitmex
bitmex_SOURCES = bitmex.c bitmex.yuck
bitmex_SOURCES += ws.c ws.h
bitmex_SOURCES += nifty.h
bitmex_SOURCES += tls.c tls.h
bitmex_SOURCES += version.c version.h
bitmex_CPPFLAGS = $(AM_CPPFLAGS)
bitmex_CPPFLAGS += $(ev_CFLAGS)
bitmex_LDFLAGS = $(AM_LDFLAGS)
bitmex_LDFLAGS += $(ev_LIBS)
bitmex_LDADD = $(LIBEV_LIBS) -lssl -lcrypto
BUILT_SOURCES += bitmex.yucc

bin_PROGRAMS += deribit
deribit_SOURCES = deribit.c deribit.yuck
deribit_SOURCES += ws.c ws.h
deribit_SOURCES += nifty.h
deribit_SOURCES += tls.c tls.h
deribit_SOURCES += version.c version.h
deribit_SOURCES += ../cex_cred.h
deribit_CPPFLAGS = $(AM_CPPFLAGS)
deribit_CPPFLAGS += $(ev_CFLAGS)
deribit_LDFLAGS = $(AM_LDFLAGS)
deribit_LDFLAGS += $(ev_LIBS)
deribit_LDADD = $(LIBEV_LIBS) -lssl -lcrypto
BUILT_SOURCES += deribit.yucc

bin_PROGRAMS += derifix
derifix_SOURCES = derifix.c derifix.yuck
derifix_SOURCES += nifty.h
derifix_SOURCES += fix.c fix.h
derifix_SOURCES += version.c version.h
derifix_SOURCES += ../cex_cred.h
derifix_CPPFLAGS = $(AM_CPPFLAGS)
derifix_CPPFLAGS += $(ev_CFLAGS)
derifix_LDFLAGS = $(AM_LDFLAGS)
derifix_LDFLAGS += $(ev_LIBS)
derifix_LDADD = $(LIBEV_LIBS) -lcrypto
BUILT_SOURCES += deribit.yucc

bin_PROGRAMS += trtwss
trtwss_SOURCES = trtwss.c trtwss.yuck
trtwss_SOURCES += ws.c ws.h
trtwss_SOURCES += tls.c tls.h
trtwss_SOURCES += version.c version.h
trtwss_SOURCES += boobs.h
trtwss_SOURCES += nifty.h
trtwss_CPPFLAGS = $(AM_CPPFLAGS) $(LIBEV_CPPFLAGS)
trtwss_LDFLAGS = $(AM_LDFLAGS)  $(LIBEV_LDFLAGS)
trtwss_LDADD = $(LIBEV_LIBS) -lssl -lcrypto
BUILT_SOURCES += trtwss.yucc

bin_PROGRAMS += gemini
gemini_SOURCES = gemini.c gemini.yuck
gemini_SOURCES += nifty.h
gemini_SOURCES += ws.c ws.h
gemini_SOURCES += tls.c tls.h
gemini_SOURCES += version.c version.h
gemini_CPPFLAGS = $(AM_CPPFLAGS)
gemini_CPPFLAGS += $(ev_CFLAGS)
gemini_LDFLAGS = $(AM_LDFLAGS)
gemini_LDFLAGS += $(ev_LIBS)
gemini_LDADD = $(LIBEV_LIBS) -lssl -lcrypto
BUILT_SOURCES += gemini.yucc


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
