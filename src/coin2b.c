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

#define NSECS	(1000000000)
#define MSECS	(1000)

typedef long unsigned int tv_t;
#define NOT_A_TIME	((tv_t)-1ULL)

typedef enum {
	TYPE_UNK,
	TYPE_OPEN,
	TYPE_DONE,
	TYPE_MTCH,
} coin_type_t;

typedef enum {
	SIDE_UNK,
	SIDE_BID,
	SIDE_ASK,
} coin_side_t;

typedef struct {
	const char *ins;
	size_t inz;
} coin_ins_t;


static inline size_t
memncpy(char *restrict tgt, const char *src, size_t len)
{
	(void)memcpy(tgt, src, len);
	return len;
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

static tv_t
strtons(const char *buf, char **endptr)
{
/* interpret BUF (looks like .8455) as subsecond stamp */
	char *on;
	tv_t r;

	if (UNLIKELY(*buf++ != '.')) {
		/* no subseconds here, mate */
		return 0UL;
	}
	r = strtoul(buf, &on, 10);	
	switch (on - buf) {
	case 0U:
		r *= 10U;
	case 1U:
		r *= 10U;
	case 2U:
		r *= 10U;
	case 3U:
		r *= 10U;
	case 4U:
		r *= 10U;
	case 5U:
		r *= 10U;
	case 6U:
		r *= 10U;
	case 7U:
		r *= 10U;
	case 8U:
		r *= 10U;
	default:
		break;
	}
	if (LIKELY(endptr != NULL)) {
		*endptr = on;
	}
	return r;
}


/* specific values */
static coin_type_t
snarf_type(const char *val, size_t UNUSED(len))
{
	switch (*val) {
	case 'o':
		return TYPE_OPEN;
	case 'd':
		return TYPE_DONE;
	case 'm':
		return TYPE_MTCH;
	default:
		break;
	}
	return TYPE_UNK;
}

static coin_side_t
snarf_side(const char *val, size_t UNUSED(len))
{
	switch (*val) {
	case 'b':
		return SIDE_BID;
	case 's':
		return SIDE_ASK;
	default:
		break;
	}
	return SIDE_UNK;
}


/* processor */
static hx_t hx_type, hx_time, hx_prod, hx_side, hx_pric, hx_size, hx_rsiz;

static void
init(void)
{
	hx_type = hash("type", 4U);
	hx_side = hash("side", 4U);
	hx_time = hash("time", 4U);
	hx_prod = hash("product_id", 4U);
	hx_pric = hash("price", 4U);
	hx_size = hash("size", 4U);
	hx_rsiz = hash("remaining_size", 4U);
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
	static const char *sides[] = {"???3", "BID3", "ASK3"};
	const char *const eol = line + llen;
	jsmntok_t tok[64U];
	jsmn_parser p;
	ssize_t r;
	char *on;
	/* the info to fill */
	struct {
		tv_t xt;
		tv_t rt;
		coin_ins_t ins;
		coin_side_t sd;
		char p[16U];
		char q[16U];
		/* auxiliary stuff */
		coin_type_t ty;
	} beef = {0U};

	if (UNLIKELY((beef.rt = strtotv(line, &on)) == NOT_A_TIME)) {
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

	/* loop and fill beef */
	for (size_t i = 1U; i < (size_t)r; i++) {
		hx_t hx;

		if (UNLIKELY(tok[i].type != JSMN_STRING)) {
			continue;
		}
		/* otherwise */
		hx = hash(on + tok[i].start, 4U);

		if (0) {
			;
		} else if (hx == hx_type && tok[++i].type == JSMN_STRING) {
			const char *vs = on + tok[i].start;
			size_t vz = tok[i].end - tok[i].start;
			beef.ty = snarf_type(vs, vz);

			if (UNLIKELY(!beef.ty)) {
				/* short-cut here */
				return 0;
			}
		} else if (hx == hx_side && tok[++i].type == JSMN_STRING) {
			const char *vs = on + tok[i].start;
			size_t vz = tok[i].end - tok[i].start;
			beef.sd = snarf_side(vs, vz);
		} else if ((hx == hx_size || hx == hx_rsiz) &&
			   tok[++i].type == JSMN_STRING) {
			const char *vs = on + tok[i].start;
			size_t vz = tok[i].end - tok[i].start;
			memcpy(beef.q, vs, vz);
		} else if (hx == hx_time && tok[++i].type == JSMN_STRING) {
			struct tm tm[1U];
			const char *vs = on + tok[i].start;
			const char *nanos = strptime(vs, "%FT%T", tm);
			/* exchange time */
			beef.xt = timegm(tm);
			beef.xt *= NSECS;
			beef.xt += strtons(nanos, NULL);
		} else if (hx == hx_pric && tok[++i].type == JSMN_STRING) {
			const char *vs = on + tok[i].start;
			size_t vz = tok[i].end - tok[i].start;
			memcpy(beef.p, vs, vz);
		} else if (hx == hx_prod && tok[++i].type == JSMN_STRING) {
			const char *vs = on + tok[i].start;
			size_t vz = tok[i].end - tok[i].start;
			beef.ins = (coin_ins_t){vs, vz};
		}
	}
	/* check quantity */
	if (UNLIKELY(!beef.q[0U] || beef.q[0U] == '0' && !beef.q[1U])) {
		return 0;
	}

	{
		char buf[256U];
		size_t len = 0U;

		len += tvtostr(buf + len, sizeof(buf) - len, beef.xt);
		buf[len++] = '\t';
		len += tvtostr(buf + len, sizeof(buf) - len, beef.rt);
		buf[len++] = '\t';
		len += memncpy(buf + len, beef.ins.ins, beef.ins.inz);
		buf[len++] = '\t';
		len += memncpy(buf + len, sides[beef.sd], 4U);
		buf[len++] = '\t';
		len += memncpy(buf + len, beef.p, strlen(beef.p));
		buf[len++] = '\t';
		if (beef.ty != TYPE_OPEN) {
			buf[len++] = '-';
		}
		len += memncpy(buf + len, beef.q, strlen(beef.q));
		buf[len++] = '\n';

		fwrite(buf, 1, len, stdout);
	}
	return 0;
}


#include "coin2b.yucc"

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

/* coin2csv.c ends here */
