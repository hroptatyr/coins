#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
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

typedef long long unsigned int prim_t;
#define NOT_A_PRIM	((prim_t)-1ULL)

typedef char plx_ins_t[16U];

typedef enum {
	SIDE_UNK,
	SIDE_BID,
	SIDE_ASK,
	SIDE_TRA,
} plx_side_t;


static __attribute__((format(printf, 1, 2))) void
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	fputc('\n', stderr);
	return;
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

static prim_t
strtoprim(const char *str, size_t len)
{
	char *on;
	long long unsigned int r = strtoull(str, &on, 10U);
	if (UNLIKELY(on != str + len)) {
		return NOT_A_PRIM;
	}
	return r;
}


/* processor */
static prim_t subids[128U];
static plx_ins_t subins[128U];
static tv_t metr;
static hx_t hx_modi, hx_dele, hx_mtch;

static void
init(void)
{
	static const char modi[] = "orderBookModify";
	static const char dele[] = "orderBookRemove";
	static const char mtch[] = "newTrade";

	hx_modi = hash(modi, strlenof(modi));
	hx_dele = hash(dele, strlenof(dele));
	hx_mtch = hash(mtch, strlenof(mtch));
	return;
}

static void
fini(void)
{
	return;
}

static size_t
instostr(char *restrict buf, size_t UNUSED(bsz), prim_t iid)
{
	size_t z = 15U - subins[iid][15U];
	return memncpy(buf, subins[iid], z);
}

static size_t
strtoins(const char *ins, size_t len, prim_t iid)
{
	size_t z = memncpy(subins[iid], ins, len < 16U ? len : 15U);
	subins[iid][15U] = (char)(15U - z);
	return z;
}

static inline prim_t
toktoprim(const char *base, jsmntok_t tok)
{
	return strtoprim(base + tok.start, tok.end - tok.start);
}

static int
subscr(const char *base, const jsmntok_t *toks, size_t ntoks)
{
	prim_t subid;

	if (UNLIKELY(ntoks <= 4U)) {
		return -1;
	} else if (UNLIKELY(toks[2U].type != JSMN_PRIMITIVE)) {
		return -1;
	} else if (UNLIKELY(toks[4U].type != JSMN_STRING)) {
		return -1;
	}
	/* try and get subscription id */
	subid = toktoprim(base, toks[2U]);
	if (UNLIKELY(subid >= countof(subids))) {
		return -1;
	}
	/* otherwise memorise him */
	subids[subid] = 0U;
	strtoins(base + toks[4U].start, toks[4U].end - toks[4U].start, subid);
	return 0;
}

static int
subscd(const char *base, const jsmntok_t *toks, size_t ntoks)
{
	prim_t subix, subid;

	if (UNLIKELY(ntoks <= 3U)) {
		return -1;
	} else if (UNLIKELY(toks[2U].type != JSMN_PRIMITIVE)) {
		return -1;
	} else if (UNLIKELY(toks[3U].type != JSMN_PRIMITIVE)) {
		return -1;
	}
	/* try and get subscription id */
	subix = toktoprim(base, toks[2U]);
	subid = toktoprim(base, toks[3U]);
	if (UNLIKELY(subix >= countof(subids))) {
		return -1;
	} else if (UNLIKELY(!*subins[subix])) {
		error("\
Warning: subscription confirmation for unsubscribed instrument");
		return -1;
	}
	/* otherwise memorise him */
	subids[subix] = subid;
	return 0;
}

static int
events(const char *base, const jsmntok_t *toks, size_t ntoks)
{
	prim_t subid;
	size_t subix;

	if (UNLIKELY(ntoks <= 8U)) {
		return -1;
	} else if (UNLIKELY(toks[2U].type != JSMN_PRIMITIVE)) {
		return -1;
	} else if (UNLIKELY(toks[5U].type != JSMN_ARRAY)) {
		return -1;
	} else if (UNLIKELY(toks[6U].type != JSMN_OBJECT)) {
		return -1;
	}
	/* try and get subscription id */
	subid = toktoprim(base, toks[2U]);
	/* try and find this one */
	for (subix = 0U; subix < countof(subids); subix++) {
		if (subids[subix] == subid) {
			goto found;
		}
	}
	error("\
Warning: event for unsubscribed instrument");
	return -1;
found:;
	char buf[256U];
	size_t ini = 0U, len;

	ini += tvtostr(buf + ini, sizeof(buf) - ini, metr);
	buf[ini++] = '\t';
	ini += instostr(buf + ini, sizeof(buf) - ini, subix);
	buf[ini++] = '\t';

	for (size_t i = 6U, n = toks[5U].end; toks[i].start < n;) {
		const size_t eot = toks[i++].end;
		hx_t hx;

		if (memcmp(base + toks[i++].start, "type", 4U)) {
			goto next;
		}

		/* otherwise hash the value */
		hx = hash(base + toks[i].start, toks[i].end - toks[i].start);

		if (0) {
			;
		} else if (hx == hx_modi) {
			;
		} else if (hx == hx_dele) {
			;
		} else if (hx == hx_mtch) {
			/* we can't do with trades yet */
			goto tra0;
		} else {
			error("\
Warning: unknown event type `%.*s'",
			      (int)(toks[i].end - toks[i].start),
			      base + toks[i].start);
			goto next;
		}

		/* next up should be data */
		i++;
		if (memcmp(base + toks[i++].start, "data", 4U)) {
			goto next;
		} else if (toks[i++].type != JSMN_OBJECT) {
			goto next;
		}

		len = ini;
		/* find type, amount and rate */
		if (memcmp(base + toks[i++].start, "type", 4U)) {
			/* shit */
			goto next;
		}
		len += memncpy(buf + len, base + toks[i++].start, 3U);
		buf[len++] = '2';
		buf[len++] = '\t';
		if (memcmp(base + toks[i++].start, "rate", 4U)) {
			/* shit */
			goto next;
		}
		len += memncpy(buf + len,
			       base + toks[i].start,
			       toks[i].end - toks[i].start);
		buf[len++] = '\t';
		if (toks[++i].start < eot) {
			if (memcmp(base + toks[i++].start, "amount", 6U)) {
				/* shit */
				goto next;
			}
			len += memncpy(buf + len,
				       base + toks[i].start,
				       toks[i].end - toks[i].start);
		} else if (hx == hx_dele) {
			buf[len++] = '0';
		} else {
			/* it's fucked */
			goto next;
		}
		buf[len++] = '\n';
		fwrite(buf, 1, len, stdout);
		goto next;

	tra0:
		/* we simply expect amount/date/rate/... */
		i++;
		if (memcmp(base + toks[i++].start, "data", 4U)) {
			goto next;
		} else if (toks[i].type != JSMN_OBJECT) {
			goto next;
		}
		len = ini;
		len += memncpy(buf + len, "tra0", 4U);
		buf[len++] = '\t';
		if (memcmp(base + toks[i + 5U].start, "rate", 4U)) {
			goto next;
		}
		len += memncpy(buf + len,
			       base + toks[i + 6U].start,
			       toks[i + 6U].end - toks[i + 6U].start);
		buf[len++] = '\t';
		if (memcmp(base + toks[i + 1U].start, "amount", 6U)) {
			goto next;
		}
		len += memncpy(buf + len,
			       base + toks[i + 2U].start,
			       toks[i + 2U].end - toks[i + 2U].start);
		buf[len++] = '\n';

		fwrite(buf, 1, len, stdout);
		goto next;

	next:
		/* fast forward past EOT */
		for (; toks[i].start < eot; i++);
	}
	return 0;
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

	if (UNLIKELY((metr = strtotv(line, &on)) == NOT_A_TIME)) {
		/* just skip him */
		return -1;
	} else if (UNLIKELY(*on++ != '\t')) {
		/* naw */
		return -1;
	}
	/* now comes the json bit, or maybe not? */
	if (UNLIKELY(*on != '[')) {
		return -1;
	}

	jsmn_init(&p);
	r = jsmn_parse(&p, on, eol - on, tok, countof(tok));
	if (UNLIKELY(r < 0)) {
		/* didn't work */
		error("\
Warning: invalid json %zd", r);
		return -1;
	}

	/* top-level element should be an object */
	if (UNLIKELY(!r || tok->type != JSMN_ARRAY)) {
		return -1;
	}

	if (UNLIKELY(tok[1U].type != JSMN_PRIMITIVE)) {
		return -1;
	}

	with (prim_t x = toktoprim(on, tok[1U])) {
		switch (x) {
		case 32U/*sub*/:
			subscr(on, tok, r);
			break;
		case 33U/*sub'd*/:
			subscd(on, tok, r);
			break;
		case 36U/*evt*/:
			events(on, tok, r);
			break;
		default:
			break;
		case NOT_A_PRIM:
			break;
		}
	}
	return 0;
}


#include "plx2b.yucc"

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

/* plx2b.c ends here */
