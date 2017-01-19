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
#include <curl/curl.h>
#include "wssnarf.h"
#include "nifty.h"

#define API_URL	"ws://wsapi.spacebtc.com/socket.io/?EIO=3&transport=polling"
static char api_url[256U] = API_URL;


static char*
xmemmem(const char *hay, const size_t hayz, const char *ndl, const size_t ndlz)
{
/* the one that points NDLZ behind HAY */
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
		return deconst(hay + ndlz);
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
			return deconst(cand + ndlz);
		}
	}
	return NULL;
}


int
auth_coin(ws_t ws)
{
	ws_send(ws, "2probe", 6U, 0);
	return 0;
}

int
heartbeat(ws_t ws)
{
	ws_send(ws, "5", 1U, 0);
	return 0;
}

int
join_coin(ws_t ws)
{
	ws_send(ws, "5", 1U, 0);
	return 0;
}

size_t
massage(char *restrict buf, size_t bsz)
{
	const char *const eob = buf + bsz;
	const char *x;
	size_t t;

	/* get first extent */
	if ((x = xmemmem(buf, bsz, ",\"", 2U)) == NULL) {
		return bsz;
	}
	/* initialise target pointer */
	t = --x - buf;

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
		if ((y = xmemmem(x, eob - x, ",\"", 2U)) == NULL) {
			/* no more hits */
			break;
		}
		/* and copy in between X and Y */
		t += memnmove(buf + t, x, --y - x);
	}
	/* and copy in between X and Y */
	t += memnmove(buf + t, x, eob - x);
	return t;
}


static ssize_t
sid_cb(const void *data, const size_t nz, const size_t nm, void *UNUSED(clo))
{
	const char *sid, *eos;
	size_t z;

	if ((sid = xmemmem(data, nz * nm, "\"sid\"", 5U)) == NULL) {
		return -1;
	}
	/* fast forward */
	while (*sid++ != ':');
	while (*sid++ != '"');
	for (eos = sid; *eos != '"'; eos++);
	/* otherwise upgrade to websocket and copy sid */
	z = strlenof(API_URL) - 7U;
	z += memncpy(api_url + z, "websocket", 9U);
	z += memncpy(api_url + z, "&sid=", 5U);
	z += memncpy(api_url + z, sid, eos - sid);
	api_url[z] = '\0';	

	puts(api_url);
	return nz * nm;
}


#include "spacebtc.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	wssnarf_t wss;
	int rc = EXIT_SUCCESS;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

	/* obtain a session handle first *sigh* */
	curl_global_init(CURL_GLOBAL_ALL);
	with (CURL *hndl = curl_easy_init()) {
		curl_easy_setopt(hndl, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(hndl, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(hndl, CURLOPT_WRITEFUNCTION, sid_cb);
		curl_easy_setopt(hndl, CURLOPT_USERAGENT,
				 "Dark Secret Ninja/1.0");

		curl_easy_setopt(hndl, CURLOPT_URL, api_url + 5U/*ws:...*/);
		curl_easy_perform(hndl);
		curl_easy_cleanup(hndl);
	}

	/* obtain a loop */
	wss = make_wssnarf("prices");
	add_wssnarf(wss, (wssnarf_param_t){api_url, 6.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return rc;
}

/* spacebtc.c ends here */
