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
} btsp_side_t;

typedef enum {
	INS_UNK,
	INS_BTCUSD,
	INS_BTCEUR,
	INS_EURUSD,
} btsp_ins_t;

typedef enum {
	CHAN_UNK,
	CHAN_TRAS,
	CHAN_BOOK,
	CHAN_DIFF,
	CHAN_ORDS,
} btsp_chan_t;


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

static inline size_t
tokncpy(char *restrict buf, const char *base, jsmntok_t tok)
{
	return memncpy(buf, base + tok.start, tok.end - tok.start);
}


static hx_t hx_live, hx_diff, hx_ords;
static hx_t hx_btceur, hx_eurusd, hx_orders, hx_trades, hx_r_book;

/* specific values */
static btsp_chan_t
snarf_chan(const char *val, size_t UNUSED(len))
{
	hx_t hx = hash(val, 4U);

	if (0) {
		;
	} else if (hx == hx_live) {
		/* check if live orders or live trades */
		hx = hash(val + 5U, 6U);

		if (UNLIKELY(val[4U] != '_')) {
			;
		} else if (hx == hx_orders) {
			return CHAN_ORDS;
		} else if (hx == hx_trades) {
			return CHAN_TRAS;
		}
	} else if (hx == hx_diff) {
		hx = hash(val + 4U + 1U + 4U, 6U);
		if (LIKELY(hx == hx_r_book)) {
			return CHAN_DIFF;
		}
	} else if (hx == hx_ords) {
		hx = hash(val + 4U, 6U);
		if (LIKELY(hx == hx_r_book)) {
			return CHAN_BOOK;
		}
	}
	return CHAN_UNK;
}

static btsp_ins_t
snarf_ins(const char *val, size_t len)
{
	if (LIKELY(len >= 6U)) {
		hx_t hx = hash(val + len - 6U, 6U);

		if (0) {
			;
		} else if (hx == hx_orders ||
			   hx == hx_trades ||
			   hx == hx_r_book) {
			/* there's no suffix for this one */
			return INS_BTCUSD;
		} else if (hx == hx_btceur) {
			return INS_BTCEUR;
		} else if (hx == hx_eurusd) {
			return INS_EURUSD;
		}
	}
	return INS_UNK;
}

static btsp_side_t
UNUSED(snarf_side)(const char *val, size_t UNUSED(len))
{
	if (LIKELY(len == 1U)) {
		switch (*val) {
		case '0':
			return SIDE_BID;
		case '1':
			return SIDE_ASK;
		default:
			break;
		}
	}
	return SIDE_UNK;
}


/* processor */
static hx_t hx_data, hx_chan, hx_evnt;
static hx_t UNUSED(hx_crea), UNUSED(hx_dele), UNUSED(hx_modi);
static hx_t UNUSED(hx_px), UNUSED(hx_qx);
static hx_t hx_bids, hx_asks;

static void
init(void)
{
	hx_evnt = hash("event", 4U);
	hx_data = hash("data", 4U);
	hx_chan = hash("channel", 4U);

	hx_crea = hash("order_created", strlenof("order_created"));
	hx_dele = hash("order_deleted", strlenof("order_deleted"));
	hx_modi = hash("order_changed", strlenof("order_changed"));

	hx_px = hash("price", 4U);
	hx_qx = hash("amount", 4U);

	hx_live = hash("live", 4U);
	hx_diff = hash("diff_order_book", 4U);
	hx_ords = hash("order_book", 4U);

	hx_bids = hash("bids", 4U);
	hx_asks = hash("asks", 4U);

	/* suffixes */
	hx_btceur = hash("btceur", 6U);
	hx_eurusd = hash("eurusd", 6U);
	hx_r_book = hash("r_book", 6U);
	hx_trades = hash("trades", 6U);
	hx_orders = hash("orders", 6U);
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
	/* the info to fill */
	struct {
		tv_t xt;
		tv_t rt;
		btsp_chan_t ch;
		btsp_ins_t ins;
		btsp_side_t sd;
		char p[16U];
		char q[16U];
		/* auxiliary stuff */
		hx_t ev;
		size_t dbeg;
		size_t dend;
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
	/* just to have a finaliser */
	tok[r].start = -1ULL;

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
		} else if (hx == hx_chan && tok[++i].type == JSMN_STRING) {
			const char *vs = on + tok[i].start;
			size_t vz = tok[i].end - tok[i].start;
			beef.ch = snarf_chan(vs, vz);
			beef.ins = snarf_ins(vs, vz);
		} else if (hx == hx_evnt && tok[++i].type == JSMN_STRING) {
			const char *vs = on + tok[i].start;
			size_t vz = tok[i].end - tok[i].start;
			beef.ev = hash(vs, vz);
		} else if (hx == hx_data && tok[++i].type == JSMN_OBJECT) {
			beef.dend = tok[i].end;
			beef.dbeg = i + 1U;
			/* skip pairs */
			for (size_t end = tok[i].end;
			     tok[i + 1U].start < end; i++);
		}
	}

	switch (beef.ch) {
		size_t bid, eob;
		size_t ask, eoa;

	case CHAN_BOOK:
	case CHAN_DIFF:;
		bid = 0U, eob = 0U;
		ask = 0U, eoa = 0U;
		for (size_t i = beef.dbeg; tok[i].start < beef.dend; i++) {
			const char *vs = on + tok[i].start;
			size_t vz = tok[i].end - tok[i].start;
			hx_t hx = hash(vs, vz);

			if (0) {
			ffw:
				for (size_t end = tok[i].end;
				     tok[i + 1U].start < end; i++);
			} else if (hx == hx_bids) {
				bid = ++i;
				eob = tok[i].end;
				goto ffw;
			} else if (hx == hx_asks) {
				ask = ++i;
				eoa = tok[i].end;
				goto ffw;
			}
		}
		/* otherwise go for printing */
		static const char *const inss[] =
			{"??????", "BTCUSD", "BTCEUR", "EURUSD"};
		char buf[256U];
		size_t len, ini;

		len = 0U;
		len += tvtostr(buf + len, sizeof(buf) - len, beef.rt);
		buf[len++] = '\t';
		len += memncpy(buf + len, inss[beef.ins], 6U);
		buf[len++] = '\t';
		ini = len;
#define assume(...)	if (UNLIKELY(!(__VA_ARGS__))) continue

		/* bids first */
		buf[len++] = 'B';
		buf[len++] = (char)('2' - (beef.ch == CHAN_BOOK));
		buf[len++] = '\t';
		ini = len;
		for (size_t i = bid + 1U; tok[i].start < eob; i++) {
			len = ini;
			assume(tok[i].type == JSMN_ARRAY);
			len += tokncpy(buf + len, on, tok[++i]);
			buf[len++] = '\t';
			len += tokncpy(buf + len, on, tok[++i]);
			buf[len++] = '\n';
			fwrite(buf, 1, len, stdout);
			len = ini;
			buf[len - 2U] = '2';
		}
		/* rewind */
		len = ini -= 3U;
		/* asks next */
		buf[len++] = 'A';
		buf[len++] = (char)('2' - (beef.ch == CHAN_BOOK));
		buf[len++] = '\t';
		ini = len;
		for (size_t i = ask + 1U; tok[i].start < eoa; i++) {
			assume(tok[i].type == JSMN_ARRAY);
			len += tokncpy(buf + len, on, tok[++i]);
			buf[len++] = '\t';
			len += tokncpy(buf + len, on, tok[++i]);
			buf[len++] = '\n';
			fwrite(buf, 1, len, stdout);
			len = ini;
			buf[len - 2U] = '2';
		}
		break;
	case CHAN_TRAS:
		/* precision issues */
		break;
	case CHAN_ORDS:
		/* precision issues */
		break;
	default:
		break;
	}
	return 0;
}


#include "btsp2b.yucc"

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

/* btsp2b.c ends here */
