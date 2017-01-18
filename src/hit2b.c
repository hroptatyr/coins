#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "jsmn.h"
#include "hash.h"
#include "nifty.h"

/* bitstamp needs a full order book as orders can change in price and
 * quantity and are referenced by order id only
 * apart from that bitstamp suffers badly from ill-formatted doubles */

#define NSECS	(1000000000)
#define MSECS	(1000)

typedef long unsigned int tv_t;
#define NOT_A_TIME	((tv_t)-1ULL)

typedef enum {
	SIDE_UNK,
	SIDE_BID,
	SIDE_ASK,
	SIDE_TRA,
} hit_side_t;


static tv_t
strtotv(const char *ln, char **endptr)
{
	char *on;
	tv_t r;

	/* time value up first */
	with (long unsigned int s, x) {
		if (UNLIKELY((s = strtoul(ln, &on, 10), on == NULL))) {
			r = NOT_A_TIME;
			goto out;
		} else if (*on == '.') {
			char *moron;

			x = strtoul(++on, &moron, 10);
			if (UNLIKELY(moron - on > 9U)) {
				return NOT_A_TIME;
			} else if ((moron - on) % 3U) {
				/* huh? */
				return NOT_A_TIME;
			}
			switch (moron - on) {
			case 0U:
				x *= MSECS;
			case 3U:
				x *= MSECS;
			case 6U:
				x *= MSECS;
			case 9U:
			default:
				break;
			}
			on = moron;
		} else {
			x = 0U;
		}
		r = s * NSECS + x;
	}
out:
	if (LIKELY(endptr != NULL)) {
		*endptr = on;
	}
	return r;
}

static tv_t
smstotv(const char *ln, char **endptr)
{
/* treat LN as string in milliseconds since Epoch */
	tv_t r = strtoul(ln, endptr, 10);
	r *= MSECS;
	r *= MSECS;
	return r;
}

static ssize_t
tvtostr(char *restrict buf, size_t bsz, tv_t t)
{
	return snprintf(buf, bsz, "%lu.%09lu", t / NSECS, t % NSECS);
}


/* processor */
static hx_t hx_full, hx_incr;
static hx_t hx_sym, hx_bid, hx_ask, hx_tra;

static void
init(void)
{
	static const char full[] = "MarketDataSnapshotFullRefresh";
	static const char incr[] = "MarketDataIncrementalRefresh";

	hx_full = hash(full, strlenof(full));
	hx_incr = hash(incr, strlenof(incr));

	hx_sym = hash("symbol", 3U);
	hx_bid = hash("bid", 3U);
	hx_ask = hash("ask", 3U);
	hx_tra = hash("trade", 3U);
	return;
}

static void
fini(void)
{
	return;
}

static int
procln(const char *line, size_t llen)
{
/* process one line */
	const char *const eol = line + llen;
	jsmntok_t tok[4096U];
	jsmn_parser p;
	ssize_t r;
	char *on;
	tv_t xt;
	tv_t rt;
	char buf[256U];
	size_t ini = 0U;

	if (UNLIKELY((rt = strtotv(line, &on)) == NOT_A_TIME)) {
		/* just skip him */
		return -1;
	} else if (UNLIKELY(*on++ != '\t')) {
		/* naw */
		return -1;
	}
	/* now comes the json bit, or maybe not? */
	if (UNLIKELY(*on != '{')) {
		return -1;
	}

	jsmn_init(&p);
	r = jsmn_parse(&p, on, eol - on, tok, countof(tok));
	if (UNLIKELY(r < 0)) {
		/* didn't work */
		return -1;
	}

	/* top-level element should be an object */
	if (UNLIKELY(!r || tok->type != JSMN_OBJECT)) {
		return -1;
	}
	/* make sure we're dealing with market data */
	if (UNLIKELY(tok[1U].type != JSMN_STRING)) {
		return -1;
	} else if (UNLIKELY(tok[2U].type != JSMN_OBJECT)) {
		return -1;
	}
	{
		const char *vs = on + tok[1U].start;
		size_t vz = tok[1U].end - tok[1U].start;
		hx_t hx = hash(vs, vz);

		/* we don't actually care about full or incr
		 * they look the same anyway */
		if (hx != hx_full && hx != hx_incr) {
			return -1;
		}
	}
	/* check timestamp */
	if (UNLIKELY(tok[r - 2U].type != JSMN_STRING)) {
		return -1;
	} else if (UNLIKELY(tok[r - 1U].type != JSMN_PRIMITIVE)) {
		return -1;
	}
	{
		static const char time[] = "timestamp";
		const char *vs = on + tok[r - 2U].start;

		if (memcmp(vs, time, strlenof(time))) {
			return -1;
		}
		/* skip to actual value field */
		vs = on + tok[r - 1U].start;
		if (UNLIKELY((xt = smstotv(vs, NULL)) == NOT_A_TIME)) {
			return -1;
		}
	}

	/* prep the buffer */
	ini += tvtostr(buf + ini, sizeof(buf) - ini, xt);
	buf[ini++] = '\t';
	ini += tvtostr(buf + ini, sizeof(buf) - ini, rt);
	buf[ini++] = '\t';

	/* loop and print */
	for (size_t i = 3U; i < (size_t)r; i++) {
		hit_side_t sd = SIDE_UNK;
		hx_t hx;

		if (UNLIKELY(tok[i].type != JSMN_STRING)) {
			continue;
		}
		/* otherwise */
		hx = hash(on + tok[i].start, 3U);

		if (0) {
			;
		} else if (hx == hx_sym && tok[++i].type == JSMN_STRING) {
			const char *vs = on + tok[i].start;
			size_t vz = tok[i].end - tok[i].start;
			ini += memncpy(buf + ini, vs, vz);
			buf[ini++] = '\t';
		} else if (hx == hx_bid && tok[++i].type == JSMN_ARRAY) {
			sd = SIDE_BID;
			goto arr;
		} else if (hx == hx_ask && tok[++i].type == JSMN_ARRAY) {
			sd = SIDE_ASK;
			goto arr;
		} else if (hx == hx_tra && tok[++i].type == JSMN_ARRAY) {
			sd = SIDE_TRA;
			goto arr;
		}
		continue;
	arr:;
		/* array loop */
		static const char *sides[] = {"????", "BID2", "ASK2", "TRA0"};
		size_t in2;

		in2 = ini;
		in2 += memncpy(buf + in2, sides[sd], 4U);
		buf[in2++] = '\t';

		for (size_t j = 0U, n = tok[i].size; j < n; j++) {
			size_t len = in2;
			const char *vs;
			size_t vz;

			if (UNLIKELY(tok[++i].type != JSMN_OBJECT)) {
				break;
			} else if (UNLIKELY(tok[++i].type != JSMN_STRING)) {
				break;
			} else if (memcmp(on + tok[i++].start, "price", 5U)) {
				break;
			}
			/* get price */
			vs = on + tok[i].start;
			vz = tok[i].end - tok[i].start;
			len += memncpy(buf + len, vs, vz);
			buf[len++] = '\t';

			if (UNLIKELY(tok[++i].type != JSMN_STRING)) {
				break;
			} else if (memcmp(on + tok[i++].start, "size", 4U)) {
				continue;
			}
			vs = on + tok[i].start;
			vz = tok[i].end - tok[i].start;
			len += memncpy(buf + len, vs, vz);
			buf[len++] = '\n';

			fwrite(buf, 1, len, stdout);
		}
	}
	return 0;
}


#include "hit2b.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	int rc = EXIT_SUCCESS;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = EXIT_FAILURE;
		goto out;
	}

	/* initialise the processor */
	init();
	{
		char *line = NULL;
		size_t llen = 0UL;

		for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
			procln(line, nrd);
		}
		free(line);
	}
	/* finalise the processor */
	fini();

out:
	yuck_free(argi);
	return rc;
}

/* hit2b.c ends here */
