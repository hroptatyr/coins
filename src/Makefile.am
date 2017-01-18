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
libws_a_SOURCES += boobs.h
libws_a_SOURCES += nifty.h
ws_LIBS = -lssl -lcrypto

noinst_LIBRARIES += libcoins.a
libcoins_a_SOURCES =
libcoins_a_SOURCES += wssnarf.c
libcoins_a_LIBADD = libws.a

bin_PROGRAMS += coin
coin_SOURCES = coin.c coin.yuck
coin_SOURCES += version.c version.h
coin_CPPFLAGS = $(AM_CPPFLAGS)
coin_CPPFLAGS += $(ev_CFLAGS)
coin_LDFLAGS = $(AM_LDFLAGS)
coin_LDFLAGS += $(ev_LIBS)
coin_LDFLAGS += $(ws_LIBS)
coin_LDFLAGS += -lcurl
coin_LDADD = libcoins.a libws.a
BUILT_SOURCES += coin.yucc

bin_PROGRAMS += coin2b
coin2b_SOURCES = coin2b.c coin2b.yuck
coin2b_SOURCES += jsmn.c jsmn.h
coin2b_SOURCES += hash.c hash.h
coin2b_SOURCES += version.c version.h
BUILT_SOURCES += coin2b.yucc

bin_PROGRAMS += hitwss
hitwss_SOURCES = hitwss.c hitwss.yuck
hitwss_SOURCES += version.c version.h
hitwss_CPPFLAGS = $(AM_CPPFLAGS)
hitwss_CPPFLAGS += $(ev_CFLAGS)
hitwss_LDFLAGS = $(AM_LDFLAGS)
hitwss_LDFLAGS += $(ev_LIBS)
hitwss_LDFLAGS += $(ws_LIBS)
hitwss_LDADD = libcoins.a libws.a
BUILT_SOURCES += hitwss.yucc

bin_PROGRAMS += hit2b
hit2b_SOURCES = hit2b.c hit2b.yuck
hit2b_SOURCES += jsmn.c jsmn.h
hit2b_SOURCES += hash.c hash.h
hit2b_SOURCES += version.c version.h
BUILT_SOURCES += hit2b.yucc

bin_PROGRAMS += cexwss
cexwss_SOURCES = cexwss.c cexwss.yuck
cexwss_SOURCES += version.c version.h
cexwss_SOURCES += ../cex_cred.h
cexwss_CPPFLAGS = $(AM_CPPFLAGS)
cexwss_CPPFLAGS += $(ev_CFLAGS)
cexwss_LDFLAGS = $(AM_LDFLAGS)
cexwss_LDFLAGS += $(ev_LIBS)
cexwss_LDFLAGS += $(ws_LIBS)
cexwss_LDADD = libcoins.a libws.a
BUILT_SOURCES += cexwss.yucc

bin_PROGRAMS += cex2b
cex2b_SOURCES = cex2b.c cex2b.yuck
cex2b_SOURCES += jsmn.c jsmn.h
cex2b_SOURCES += hash.c hash.h
cex2b_SOURCES += version.c version.h
BUILT_SOURCES += cex2b.yucc

bin_PROGRAMS += plxwss
plxwss_SOURCES = plxwss.c plxwss.yuck
plxwss_SOURCES += version.c version.h
plxwss_CPPFLAGS = $(AM_CPPFLAGS)
plxwss_CPPFLAGS += $(ev_CFLAGS)
plxwss_LDFLAGS = $(AM_LDFLAGS)
plxwss_LDFLAGS += $(ev_LIBS)
plxwss_LDFLAGS += $(ws_LIBS)
plxwss_LDFLAGS += -lcurl
plxwss_LDADD = libcoins.a libws.a
BUILT_SOURCES += plxwss.yucc

bin_PROGRAMS += plx2b
plx2b_SOURCES = plx2b.c plx2b.yuck
plx2b_SOURCES += jsmn.c jsmn.h
plx2b_SOURCES += hash.c hash.h
plx2b_SOURCES += version.c version.h
BUILT_SOURCES += plx2b.yucc

bin_PROGRAMS += btsp
btsp_SOURCES = btsp.c btsp.yuck
btsp_SOURCES += version.c version.h
btsp_CPPFLAGS = $(AM_CPPFLAGS)
btsp_CPPFLAGS += $(ev_CFLAGS)
btsp_LDFLAGS = $(AM_LDFLAGS)
btsp_LDFLAGS += $(ev_LIBS)
btsp_LDFLAGS += $(ws_LIBS)
btsp_LDADD = libcoins.a libws.a
BUILT_SOURCES += btsp.yucc

bin_PROGRAMS += btsp2b
btsp2b_SOURCES = btsp2b.c btsp2b.yuck
btsp2b_SOURCES += jsmn.c jsmn.h
btsp2b_SOURCES += hash.c hash.h
btsp2b_SOURCES += version.c version.h
BUILT_SOURCES += btsp2b.yucc

bin_PROGRAMS += bitfin
bitfin_SOURCES = bitfin.c bitfin.yuck
bitfin_SOURCES += version.c version.h
bitfin_CPPFLAGS = $(AM_CPPFLAGS)
bitfin_CPPFLAGS += $(ev_CFLAGS)
bitfin_LDFLAGS = $(AM_LDFLAGS)
bitfin_LDFLAGS += $(ev_LIBS)
bitfin_LDFLAGS += $(ws_LIBS)
bitfin_LDADD = libcoins.a libws.a
BUILT_SOURCES += bitfin.yucc

bin_PROGRAMS += bitfin2b
bitfin2b_SOURCES = bitfin2b.c bitfin2b.yuck
bitfin2b_SOURCES += version.c version.h
BUILT_SOURCES += bitfin2b.yucc

bin_PROGRAMS += okfix
okfix_SOURCES = okfix.c okfix.yuck
okfix_SOURCES += version.c version.h
okfix_SOURCES += ../okcoin_cred.h
okfix_SOURCES += fix.c fix.h
okfix_SOURCES += tls.c tls.h
okfix_CPPFLAGS = $(AM_CPPFLAGS)
okfix_CPPFLAGS += $(ev_CFLAGS)
okfix_LDFLAGS = $(AM_LDFLAGS)
okfix_LDFLAGS += $(ev_LIBS)
okfix_LDADD = -lssl -lcrypto
BUILT_SOURCES += okfix.yucc

bin_PROGRAMS += ok2b
ok2b_SOURCES = ok2b.c ok2b.yuck
ok2b_SOURCES += version.c version.h
ok2b_SOURCES += fix.c fix.h
ok2b_CPPFLAGS = $(AM_CPPFLAGS)
ok2b_CPPFLAGS += -DSOH='"|"'
ok2b_LDFLAGS = $(AM_LDFLAGS)
BUILT_SOURCES += ok2b.yucc

bin_PROGRAMS += btcc
btcc_SOURCES = btcc.c btcc.yuck
btcc_SOURCES += version.c version.h
btcc_SOURCES += fix.c fix.h
btcc_SOURCES += tls.c tls.h
btcc_CPPFLAGS = $(AM_CPPFLAGS)
btcc_CPPFLAGS += $(ev_CFLAGS)
btcc_LDFLAGS = $(AM_LDFLAGS)
btcc_LDFLAGS += $(ev_LIBS)
btcc_LDADD = -lssl -lcrypto
BUILT_SOURCES += btcc.yucc

bin_PROGRAMS += btcc2b
btcc2b_SOURCES = btcc2b.c btcc2b.yuck
btcc2b_SOURCES += version.c version.h
btcc2b_SOURCES += fix.c fix.h
btcc2b_CPPFLAGS = $(AM_CPPFLAGS)
btcc2b_CPPFLAGS += -DSOH='"|"'
btcc2b_LDFLAGS = $(AM_LDFLAGS)
BUILT_SOURCES += btcc2b.yucc

bin_PROGRAMS += bitmex
bitmex_SOURCES = bitmex.c bitmex.yuck
bitmex_SOURCES += version.c version.h
bitmex_CPPFLAGS = $(AM_CPPFLAGS)
bitmex_CPPFLAGS += $(ev_CFLAGS)
bitmex_LDFLAGS = $(AM_LDFLAGS)
bitmex_LDFLAGS += $(ev_LIBS)
bitmex_LDFLAGS += $(ws_LIBS)
bitmex_LDADD = libcoins.a libws.a
BUILT_SOURCES += bitmex.yucc

bin_PROGRAMS += deribit
deribit_SOURCES = deribit.c deribit.yuck
deribit_SOURCES += version.c version.h
deribit_CPPFLAGS = $(AM_CPPFLAGS)
deribit_CPPFLAGS += $(ev_CFLAGS)
deribit_LDFLAGS = $(AM_LDFLAGS)
deribit_LDFLAGS += $(ev_LIBS)
deribit_LDFLAGS += $(ws_LIBS)
deribit_LDADD = libcoins.a libws.a
BUILT_SOURCES += deribit.yucc

bin_PROGRAMS += derifix
derifix_SOURCES = derifix.c derifix.yuck
derifix_SOURCES += fix.c fix.h
derifix_SOURCES += version.c version.h
derifix_CPPFLAGS = $(AM_CPPFLAGS)
derifix_CPPFLAGS += $(ev_CFLAGS)
derifix_LDFLAGS = $(AM_LDFLAGS)
derifix_LDFLAGS += $(ev_LIBS)
derifix_LDADD = -lcrypto
BUILT_SOURCES += deribit.yucc

bin_PROGRAMS += trtwss
trtwss_SOURCES = trtwss.c trtwss.yuck
trtwss_SOURCES += version.c version.h
trtwss_CPPFLAGS = $(AM_CPPFLAGS)
trtwss_CPPFLAGS += $(ev_CFLAGS)
trtwss_LDFLAGS = $(AM_LDFLAGS)
trtwss_LDFLAGS += $(ev_LIBS)
trtwss_LDFLAGS += $(ws_LIBS)
trtwss_LDADD = libcoins.a libws.a
BUILT_SOURCES += trtwss.yucc

bin_PROGRAMS += gemini
gemini_SOURCES = gemini.c gemini.yuck
gemini_SOURCES += version.c version.h
gemini_CPPFLAGS = $(AM_CPPFLAGS)
gemini_CPPFLAGS += $(ev_CFLAGS)
gemini_LDFLAGS = $(AM_LDFLAGS)
gemini_LDFLAGS += $(ev_LIBS)
gemini_LDFLAGS += $(ws_LIBS)
gemini_LDADD = libcoins.a libws.a
BUILT_SOURCES += gemini.yucc

bin_PROGRAMS += coinfl
coinfl_SOURCES = coinfl.c coinfl.yuck
coinfl_SOURCES += version.c version.h
coinfl_SOURCES += ../coinfl_cred.h
coinfl_CPPFLAGS = $(AM_CPPFLAGS)
coinfl_CPPFLAGS += $(ev_CFLAGS)
coinfl_LDFLAGS = $(AM_LDFLAGS)
coinfl_LDFLAGS += $(ev_LIBS)
coinfl_LDFLAGS += $(ws_LIBS)
coinfl_LDADD = libcoins.a libws.a
BUILT_SOURCES += coinfl.yucc

bin_PROGRAMS += fast
fast_SOURCES = fast.c fast.yuck
fast_SOURCES += version.c version.h
fast_CPPFLAGS = $(AM_CPPFLAGS)
fast_CPPFLAGS += $(ev_CFLAGS)
fast_LDFLAGS = $(AM_LDFLAGS)
fast_LDFLAGS += $(ev_LIBS)
fast_LDFLAGS += $(ws_LIBS)
fast_LDADD = libcoins.a libws.a
BUILT_SOURCES += fast.yucc

bin_PROGRAMS += ff
ff_SOURCES = ff.c ff.yuck
ff_SOURCES += version.c version.h
ff_CPPFLAGS = $(AM_CPPFLAGS)
ff_CPPFLAGS += $(ev_CFLAGS)
ff_LDFLAGS = $(AM_LDFLAGS)
ff_LDFLAGS += $(ev_LIBS)
ff_LDFLAGS += $(ws_LIBS)
ff_LDADD = libcoins.a libws.a
BUILT_SOURCES += ff.yucc

bin_PROGRAMS += kaiko
kaiko_SOURCES = kaiko.c kaiko.yuck
kaiko_SOURCES += version.c version.h
kaiko_CPPFLAGS = $(AM_CPPFLAGS)
kaiko_CPPFLAGS += $(ev_CFLAGS)
kaiko_LDFLAGS = $(AM_LDFLAGS)
kaiko_LDFLAGS += $(ev_LIBS)
kaiko_LDFLAGS += $(ws_LIBS)
kaiko_LDADD = libws.a libcoins.a
BUILT_SOURCES += kaiko.yucc

bin_PROGRAMS += bitso
bitso_SOURCES = bitso.c bitso.yuck
bitso_SOURCES += version.c version.h
bitso_CPPFLAGS = $(AM_CPPFLAGS)
bitso_CPPFLAGS += $(ev_CFLAGS)
bitso_LDFLAGS = $(AM_LDFLAGS)
bitso_LDFLAGS += $(ev_LIBS)
bitso_LDFLAGS += $(ws_LIBS)
bitso_LDADD = libws.a libcoins.a
BUILT_SOURCES += bitso.yucc

bin_PROGRAMS += huobi
huobi_SOURCES = huobi.c huobi.yuck
huobi_SOURCES += version.c version.h
huobi_CPPFLAGS = $(AM_CPPFLAGS)
huobi_CPPFLAGS += $(ev_CFLAGS)
huobi_LDFLAGS = $(AM_LDFLAGS)
huobi_LDFLAGS += $(ev_LIBS)
huobi_LDFLAGS += $(ws_LIBS)
huobi_LDFLAGS += -lcurl
huobi_LDADD = libws.a libcoins.a
BUILT_SOURCES += huobi.yucc


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
