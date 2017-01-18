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

static inline ssize_t
tvtostr(char *restrict buf, size_t bsz, tv_t t)
{
	return snprintf(buf, bsz, "%lu.%09lu", t / NSECS, t % NSECS);
}

static inline size_t
tokncpy(char *restrict buf, const char *base, jsmntok_t tok)
{
	return memncpy(buf, base + tok.start, tok.end - tok.start);
}


/* processor */
static hx_t hx_time, hx_stmp, hx_pair, hx_bids, hx_asks;

static void
init(void)
{
	static const char time[] = "time";
	static const char stmp[] = "timestamp";
	static const char pair[] = "pair";
	static const char bids[] = "bids";
	static const char asks[] = "asks";

	hx_time = hash(time, strlenof(time));
	hx_stmp = hash(stmp, strlenof(stmp));
	hx_pair = hash(pair, strlenof(pair));
	hx_bids = hash(bids, strlenof(bids));
	hx_asks = hash(asks, strlenof(asks));
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
	jsmntok_t tok[65536U];
	jsmn_parser p;
	ssize_t r;
	char *on;
	tv_t rt;
	tv_t xt = NOT_A_TIME;
	ssize_t pair = -1, bids = -1, asks = -1;

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
	if (UNLIKELY(r <= 3U)) {
		return -1;
	} else if (UNLIKELY(tok[3U].type != JSMN_STRING)) {
		return -1;
	} else if (UNLIKELY(memcmp(on + tok[3U].start, "data", 4U))) {
		return -1;
	} else if (UNLIKELY(tok[4U].type != JSMN_OBJECT)) {
		return -1;
	}

	/* now find: time/timestamp, pair, bids and asks */
	for (size_t i = 5U; i < (size_t)r; i++) {
		const char *vs = on + tok[i].start;
		size_t vz = tok[i].end - tok[i].start;
		hx_t hx = hash(vs, vz);

		if (0) {
		ffw:
			for (const size_t eoa = tok[i].end;
			     tok[i + 1U].start < eoa; i++);
		} else if (hx == hx_time) {
			if (UNLIKELY(tok[++i].type != JSMN_PRIMITIVE)) {
				return -1;
			}
			xt = smstotv(on + tok[i].start, NULL);
		} else if (hx == hx_stmp) {
			if (UNLIKELY(tok[++i].type != JSMN_PRIMITIVE)) {
				return -1;
			}
			xt = strtotv(on + tok[i].start, NULL);
		} else if (hx == hx_pair) {
			if (UNLIKELY(tok[pair = ++i].type != JSMN_STRING)) {
				return -1;
			}
		} else if (hx == hx_bids) {
			if (UNLIKELY(tok[bids = ++i].type != JSMN_ARRAY)) {
				return -1;
			}
			goto ffw;
		} else if (hx == hx_asks) {
			if (UNLIKELY(tok[asks = ++i].type != JSMN_ARRAY)) {
				return -1;
			}
			goto ffw;
		}
	}
	/* check that we've got them all */
	if (UNLIKELY(pair >= r || bids >= r || asks >= r || xt == NOT_A_TIME)) {
		return -1;
	}
	/* just to make the loops down there easier */
	tok[r].start = -1ULL;

	/* otherwise go for printing */
	char buf[256U];
	size_t len = 0U, ini;

	len += tvtostr(buf + len, sizeof(buf) - len, xt);
	buf[len++] = '\t';
	len += tvtostr(buf + len, sizeof(buf) - len, rt);
	buf[len++] = '\t';
	len += tokncpy(buf + len, on, tok[pair]);
	buf[len++] = '\t';
	ini = len;
#define assume(...)	if (UNLIKELY(!(__VA_ARGS__))) continue

	/* bids first */
	buf[len++] = 'B';
	buf[len++] = '2';
	buf[len++] = '\t';
	ini = len;
	for (size_t i = bids + 1U, eo = tok[bids].end; tok[i].start < eo; i++) {
		len = ini;
		assume(tok[i].type == JSMN_ARRAY);
		len += tokncpy(buf + len, on, tok[++i]);
		buf[len++] = '\t';
		len += tokncpy(buf + len, on, tok[++i]);
		buf[len++] = '\n';
		fwrite(buf, 1, len, stdout);
	}
	/* rewind */
	len = ini -= 3U;
	/* asks next */
	buf[len++] = 'A';
	buf[len++] = '2';
	buf[len++] = '\t';
	ini = len;
	for (size_t i = asks + 1U, eo = tok[asks].end; tok[i].start < eo; i++) {
		len = ini;
		assume(tok[i].type == JSMN_ARRAY);
		len += tokncpy(buf + len, on, tok[++i]);
		buf[len++] = '\t';
		len += tokncpy(buf + len, on, tok[++i]);
		buf[len++] = '\n';
		fwrite(buf, 1, len, stdout);
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
