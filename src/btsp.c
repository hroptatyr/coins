#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "wssnarf.h"
#include "nifty.h"

#define API_URL		"wss://ws.pusherapp.com/app/de504dc5763aeef9ff52?protocol=7"


static char*
xmemmem(const char *hay, const size_t hayz, const char *ndl, const size_t ndlz)
{
	const char *const eoh = hay + hayz;
	const char *const eon = ndl + ndlz;
	const char *hp;
	const char *np;
	const char *cand;
	unsigned int hsum;
	unsigned int nsum;
	unsigned int eqp;

	/* trivial checks first
         * a 0-sized needle is defined to be found anywhere in haystack
         * then run strchr() to find a candidate in HAYSTACK (i.e. a portion
         * that happens to begin with *NEEDLE) */
	if (ndlz == 0UL) {
		return deconst(hay);
	} else if ((hay = memchr(hay, *ndl, hayz)) == NULL) {
		/* trivial */
		return NULL;
	}

	/* First characters of haystack and needle are the same now. Both are
	 * guaranteed to be at least one character long.  Now computes the sum
	 * of characters values of needle together with the sum of the first
	 * needle_len characters of haystack. */
	for (hp = hay + 1U, np = ndl + 1U, hsum = *hay, nsum = *hay, eqp = 1U;
	     hp < eoh && np < eon;
	     hsum ^= *hp, nsum ^= *np, eqp &= *hp == *np, hp++, np++);

	/* HP now references the (NZ + 1)-th character. */
	if (np < eon) {
		/* haystack is smaller than needle, :O */
		return NULL;
	} else if (eqp) {
		/* found a match */
		return deconst(hay);
	}

	/* now loop through the rest of haystack,
	 * updating the sum iteratively */
	for (cand = hay; hp < eoh; hp++) {
		hsum ^= *cand++;
		hsum ^= *hp;

		/* Since the sum of the characters is already known to be
		 * equal at that point, it is enough to check just NZ - 1
		 * characters for equality,
		 * also CAND is by design < HP, so no need for range checks */
		if (hsum == nsum && memcmp(cand, ndl, ndlz - 1U) == 0) {
			return deconst(cand);
		}
	}
	return NULL;
}

static char*
next_data(const char *buf, size_t bsz)
{
       static const char tok[] = "\"data\"";
       const char *const eob = buf + bsz;
       const char *bp = buf;

       for (size_t bz;
            (bp = xmemmem(bp, eob - bp, tok, strlenof(tok)));
            bp += bz) {
               /* by default skip strlenof(tok) */
               bz = strlenof(tok);

               /* overread whitespace */
               for (; (unsigned char)bp[bz] <= ' '; bz++);
               /* must be : */
               if (bp[bz++] != ':') {
                       continue;
               }
               /* overread whitespace again */
               for (; (unsigned char)bp[bz] <= ' '; bz++);
               /* must be " */
               if (bp[bz++] != '"') {
                       continue;
               }
               return deconst(bp + bz - 1U);
       }
       return NULL;
}

size_t
massage(char *restrict buf, size_t bsz)
{
	const char *const eob = buf + bsz;
	const char *x;
	size_t t;

	/* get first extent */
	if ((x = next_data(buf, bsz)) == NULL) {
		return bsz;
	}
	/* initialise target pointer */
	t = x - buf;

	for (const char *y;; x = y) {
		/* shrink buffer, de-escaping things */
		for (;; t++) {
			if ((buf[t] = *++x) == '\\') {
				/* copy next one */
				buf[t] = *++x;
			} else if (buf[t] == '"') {
				/* we'done */
				x++;
				break;
			}
		}
		/* try and find the next occurrence */
		if ((y = next_data(x, eob - x)) == NULL) {
			/* no more hits */
			break;
		}
		/* and copy in between X and Y */
		t += memnmove(buf + t, x, y - x);
	}
	/* and copy in between X and Y */
	t += memnmove(buf + t, x, eob - x);
	return t;
}


int
join_coin(ws_t ws)
{
	static const char *subs[] = {"{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_trades\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_trades_btceur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_trades_eurusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_trades_xrpusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_trades_xrpeur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_orders\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_orders_btceur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_orders_eurusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_orders_xrpusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_orders_xrpeur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"diff_order_book\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"diff_order_book_btceur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"diff_order_book_eurusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"diff_order_book_xrpusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"diff_order_book_xrpeur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"order_book\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"order_book_btceur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"order_book_eurusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"order_book_xrpusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"order_book_xrpeur\"}\
}"
	};

	for (size_t i = 0U; i < countof(subs); i++) {
		const size_t z = strlen(subs[i]);
		ws_send(ws, subs[i], z, 0);
	}
	return 1;
}


#include "btsp.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	wssnarf_t wss;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

	/* obtain a loop */
	wss = make_wssnarf((wssnarf_param_t){API_URL, "prices"});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* btsp.c ends here */
