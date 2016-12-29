#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "nifty.h"

/* bitstamp needs a full order book as orders can change in price and
 * quantity and are referenced by order id only
 * apart from that bitstamp suffers badly from ill-formatted doubles */

#define NSECS	(1000000000)
#define MSECS	(1000)

typedef long unsigned int tv_t;
#define NOT_A_TIME	((tv_t)-1ULL)

typedef char btf_ins_t[16U];

/* bitfinex channel ids seem to be small, let's try uint8_fast_t here and
 * up it if necessary */
typedef uint8_t btf_chid_t;

typedef enum {
	SIDE_UNK,
	SIDE_BID,
	SIDE_ASK,
	SIDE_TRA,
} btf_side_t;


static inline size_t
memncpy(char *restrict tgt, const char *src, size_t len)
{
	(void)memcpy(tgt, src, len);
	return len;
}

static char*
xmemmem(const char *hay, const size_t hayz, const char *ndl, const size_t ndlz)
{
/* modified version to point to AFTER the needle in HAY. */
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

static ssize_t
tvtostr(char *restrict buf, size_t bsz, tv_t t)
{
	return snprintf(buf, bsz, "%lu.%09lu", t / NSECS, t % NSECS);
}


/* processor */
static btf_ins_t subins[1U << (sizeof(btf_chid_t) * 8U)];
static tv_t metr;

static void
init(void)
{
	return;
}

static void
fini(void)
{
	return;
}

static inline size_t
instostr(char *restrict buf, size_t UNUSED(bsz), btf_chid_t ci)
{
	size_t z = 15U - subins[ci][15U];
	return memncpy(buf, subins[ci], z);
}

static inline size_t
strtoins(const char *ins, size_t len, btf_chid_t ci)
{
	size_t z = memncpy(subins[ci], ins, len < 16U ? len : 15U);
	subins[ci][15U] = (char)(15U - z);
	return z;
}

static inline int
inssubdp(btf_chid_t ci)
{
	return *subins[ci];
}

static int
subscr(const char *line, size_t llen)
{
	static const char subs[] = "\
\"event\":\"subscribed\",\"channel\":\"book\",\"chanId\":";
	static const char pair[] = "\
\"pair\":";
	const char *const eol = line + llen;
	btf_chid_t ci;
	const char *p, *eop;

	if (llen <= strlenof(subs)) {
		return -1;
	} else if (memcmp(line, subs, strlenof(subs))) {
		return -1;
	}
	/* otherwise shrink line to selection */
	line += strlenof(subs);
	llen -= strlenof(subs);
	/* ... and snarf channel id */
	with (long unsigned int x = strtoul(line, NULL, 10)) {
		ci = (btf_chid_t)x;
	}
	/* try and find the pair bit */
	if (UNLIKELY((p = xmemmem(line, llen, pair, strlenof(pair))) == NULL)) {
		return -1;
	} else if (UNLIKELY(*p++ != '"')) {
		return -1;
	} else if (UNLIKELY((eop = memchr(p, '"', eol - p)) == NULL)) {
		return -1;
	}
	/* otherwise memorise him */
	strtoins(p, eop - p, ci);
	return 0;
}

static int
events(const char *line, size_t llen)
{
	const char *const eol = line + llen;
	btf_chid_t ci;
	char *on = deconst(line);
	struct {
		size_t z;
		const char *s;
	} p, q = {1U, "0"};
	size_t cnt;
	btf_side_t sd;

	with (long unsigned int x = strtoul(on, &on, 10)) {
		if (UNLIKELY(*on++ != ',')) {
			return -1;
		}
		ci = (btf_chid_t)x;
	}
	if (UNLIKELY(!inssubdp(ci))) {
		/* could happen because there's tickers and trades and stuff */
		return -1;
	}
	/* next up is the price level */
	if (UNLIKELY((on = memchr(p.s = on, ',', eol - on)) == NULL)) {
		/* shame that */
		return -1;
	}
	p.z = on++ - p.s;
	/* next up is a count value, we're only interested in whether it's 0 */
	with (long unsigned int x = strtoul(on, &on, 10)) {
		if (UNLIKELY(*on++ != ',')) {
			/* weird that */
			return -1;
		}
		cnt = x;
	}
	/* now we inspect ON, if positive it's a bid, if negative an ask */
	if (*on == '-') {
		sd = SIDE_ASK;
	} else {
		sd = SIDE_BID;
	}
	on += sd == SIDE_ASK;
	/* and now the quantity, only when cnt > 0 */
	if (cnt > 0U) {
		if (UNLIKELY((on = memchr(q.s = on, ']', eol - on)) == NULL)) {
			/* oops, what's wrong now? */
			return -1;
		}
		q.z = on - q.s;
	}
	/* we're all set for printing now */
	char buf[256U];
	size_t len = 0U;

	len += tvtostr(buf + len, sizeof(buf) - len, metr);
	buf[len++] = '\t';
	len += instostr(buf + len, sizeof(buf) - len, ci);
	buf[len++] = '\t';
	buf[len++] = (char)((SIDE_TRA - sd) ^ '@');
	buf[len++] = '2';
	buf[len++] = '\t';
	len += memncpy(buf + len, p.s, p.z);
	buf[len++] = '\t';
	len += memncpy(buf + len, q.s, q.z);
	buf[len++] = '\n';

	fwrite(buf, 1, len, stdout);
	return 0;
}

static int
procln(const char *line, size_t llen)
{
/* process one line */
	const char *const eol = line + llen;
	char *on;

	if (UNLIKELY((metr = strtotv(line, &on)) == NOT_A_TIME)) {
		/* just skip him */
		return -1;
	} else if (UNLIKELY(*on++ != '\t')) {
		/* naw */
		return -1;
	}
	/* now comes the json bit, or maybe not? */
	switch (*on++) {
	case '{':
		/* that's subscriptions and stuff, very valuable */
		return subscr(on, eol - on);
	case '[':
		return events(on, eol - on);
	default:
		break;
	}
	return -1;
}


#include "bitfin2b.yucc"

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

/* bitfin2b.c ends here */
