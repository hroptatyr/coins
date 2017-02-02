#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
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
} trt_side_t;


static __attribute__((format(printf, 1, 2))) void
serror(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputs(": ", stderr);
		fputs(strerror(errno), stderr);
	}
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


/* processor */
static hx_t hx_evt, hx_dat;
static hx_t hx_obk, hx_odf;
static hx_t hx_cha;

#define GET_TOK(x)					\
	({						\
		const char *vs = on + x.start;		\
		const size_t vz = x.end - x.start;	\
		hash(vs, vz);				\
	})

static void
init(void)
{
	static const char evt[] = "event";
	static const char dat[] = "data";
	static const char obk[] = "orderbook";
	static const char odf[] = "orderbook_diff";
	static const char cha[] = "channel";

	hx_evt = hash(evt, strlenof(evt));
	hx_dat = hash(dat, strlenof(dat));

	hx_obk = hash(obk, strlenof(obk));
	hx_odf = hash(odf, strlenof(odf));

	hx_cha = hash(cha, strlenof(cha));
	return;
}

static void
fini(void)
{
	return;
}

static int
proc_obk(tv_t rt, const char *on, const jsmntok_t *tok, size_t ntok)
{
	/* prep the buffer */
	char buf[256U];
	size_t ini = 0U, in2;
	size_t j, n;

	ini += tvtostr(buf + ini, sizeof(buf) - ini, rt);
	buf[ini++] = '\t';

	if (UNLIKELY(GET_TOK(tok[ntok - 2U]) != hx_cha)) {
		return -1;
	}

	/* symbol should be last */
	with (jsmntok_t ch = tok[ntok - 1U]) {
		ini += memncpy(buf + ini, on + ch.start, ch.end - ch.start);
		buf[ini++] = '\t';
	}

	/* orderbook should always be complete, clear it now then */
	with (size_t len) {
		len = ini + memncpy(buf + ini, "clr0\t\t\n", 7U);
		fwrite(buf, 1, len, stdout);
	}

	/* loop and print */
	in2 = ini + memncpy(buf + ini, "ask2", 4U);
	buf[in2++] = '\t';

	/* tok[4U] should be object
	 * tok[5U] should be string "asks"
	 * tok[6U] should be array */
	n = tok[6U].end;
	for (j = 7U; j < ntok && tok[j].start < n; j++) {
		size_t len;

		/* skip to "price" */
		j++;
		jsmntok_t px = tok[++j];
		len = in2 + memncpy(buf + in2, on + px.start, px.end - px.start);
		buf[len++] = '\t';

		/* skip to "amount" */
		j++;
		jsmntok_t qx = tok[++j];
		len += memncpy(buf + len, on + qx.start, qx.end - qx.start);
		buf[len++] = '\n';

		fwrite(buf, 1, len, stdout);
	}

	/* and go over bids now */
	in2 = ini + memncpy(buf + ini, "bid2", 4U);
	buf[in2++] = '\t';

	/* tok[j] should be string "bids"
	 * tok[j + 1U] should be array */
	n = tok[j + 1U].end;
	for (j += 2U; j < ntok && tok[j].start < n; j++) {
		size_t len;

		/* skip to "price" */
		j++;
		jsmntok_t px = tok[++j];
		len = in2 + memncpy(buf + in2, on + px.start, px.end - px.start);
		buf[len++] = '\t';

		/* skip to "amount" */
		j++;
		jsmntok_t qx = tok[++j];
		len += memncpy(buf + len, on + qx.start, qx.end - qx.start);
		buf[len++] = '\n';

		fwrite(buf, 1, len, stdout);
	}
	return 0;
}

static int
proc_odf(tv_t rt, const char *on, const jsmntok_t *tok, size_t ntok)
{
	/* prep the buffer */
	char buf[256U];
	size_t len = 0U;

	len += tvtostr(buf + len, sizeof(buf) - len, rt);
	buf[len++] = '\t';

	if (UNLIKELY(GET_TOK(tok[ntok - 2U]) != hx_cha)) {
		return -1;
	}

	/* symbol should be last */
	with (jsmntok_t ch = tok[ntok - 1U]) {
		len += memncpy(buf + len, on + ch.start, ch.end - ch.start);
		buf[len++] = '\t';
	}

	/* tok[4U] should be object
	 * tok[5U] should be string "side"
	 * tok[6U] should be bid/ask */
	with (jsmntok_t s = tok[6U]) {
		len += memncpy(buf + len, on + s.start, s.end - s.start);
		buf[len++] = '2';
	}
	buf[len++] = '\t';

	/* tok[7U] is "price" */
	with (jsmntok_t px = tok[8U]) {
		len += memncpy(buf + len, on + px.start, px.end - px.start);
		buf[len++] = '\t';
	}
	/* tok[9U] is "amount" */
	with (jsmntok_t px = tok[10U]) {
		len += memncpy(buf + len, on + px.start, px.end - px.start);
		buf[len++] = '\n';
	}
	fwrite(buf, 1, len, stdout);
	return 0;
}

static int
procln(const char *line, size_t llen)
{
/* process one line */
	static size_t nl;
	const char *const eol = line + llen;
	jsmntok_t tok[4096U];
	jsmn_parser p;
	ssize_t r;
	char *on;
	tv_t rt;
	hx_t e;

	nl++;
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
		errno = 0, serror("\
Warning: couldn't read json off line %zu", nl);
		return -1;
	}

	/* top-level element should be an object */
	if (UNLIKELY(!r || tok->type != JSMN_OBJECT)) {
		return -1;
	}
	/* make sure we're dealing with market data */
	if (UNLIKELY(tok[1U].type != JSMN_STRING)) {
		return -1;
	}

	if (UNLIKELY(GET_TOK(tok[1U]) != hx_evt)) {
		goto non_conf;
	}
	if (UNLIKELY(GET_TOK(tok[3U]) != hx_dat)) {
		goto non_conf;
	}

	/* capture the actual event */
	e = GET_TOK(tok[2U]);

	if (0) {
		;
	} else if (e == hx_obk) {
		if (UNLIKELY(proc_obk(rt, on, tok, (size_t)r) < 0)) {
			goto non_conf;
		}
	} else if (e == hx_odf) {
		if (UNLIKELY(proc_odf(rt, on, tok, (size_t)r) < 0)) {
			goto non_conf;
		}
	}
	return 0;

non_conf:
	errno = 0, serror("\
Warning: json in line %zu is non-conformant", nl);
	return -1;
}


#include "trt2b.yucc"

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

/* trt2b.c ends here */
