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
#undef EV_COMPAT3
#include <ev.h>
#include "boobs.h"
#include "tls.h"

static const char logfile[] = "prices";
#define logfsz	(sizeof(logfile) - 1U)
static char hostname[256];
static size_t hostnsz;

#define API_HOST	"ws-feed.gdax.com"
#define API_PORT	443

#define TIMEOUT		6.0
#define NTIMEOUTS	10
#define ONE_DAY		86400.0
#define MIDNIGHT	0.0
#define ONE_WEEK	604800.0
#define SATURDAY	172800.0
#define SUNDAY		302400.0

/* number of seconds we tolerate inactivity in the beef channels */
#define MAX_INACT	(30)

#define strlenof(x)	(sizeof(x) - 1U)

#if defined __INTEL_COMPILER
# pragma warning (disable:2259)
#endif	/* __INTEL_COMPILER */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) __##_x
#endif	/* !UNUSED */
#define EV_PU	EV_P __attribute__((unused))
#define EV_PU_	EV_PU,

typedef struct coin_ctx_s *coin_ctx_t;
typedef struct timespec *timespec_t;

typedef enum {
	COIN_ST_UNK,
	COIN_ST_CONN,
	COIN_ST_CONND,
	COIN_ST_JOIN,
	COIN_ST_JOIND,
	COIN_ST_SLEEP,
	COIN_ST_NODATA,
	COIN_ST_RECONN,
	COIN_ST_INTR,
} coin_st_t;

struct coin_ctx_s {
	/* libev's idea of the socket below */
	ev_io watcher[1];
	ev_timer timer[1];
	ev_signal sigi[1];
	ev_signal sigp[1];
	ev_signal sigh[1];
	ev_periodic midnight[1];
	ev_prepare prep[1];

	/* keep track of heart beats */
	int nothing;
	/* ssl context */
	ssl_ctx_t ss;
	/* internal state */
	coin_st_t st;

	/* subs */
	const char *const *subs;
	size_t nsubs;

	struct timespec last_act[1];
};

static char gbuf[1048576];
static volatile size_t boff = 0;
static int logfd;
static int ping;


#define countof(x)	(sizeof(x) / sizeof(*x))

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

static __attribute__((unused)) inline void
fputsl(FILE *fp, const char *s, size_t l)
{
	for (size_t i = 0; i < l; i++) {
		fputc_unlocked(s[i], fp);
	}
	fputc_unlocked('\n', fp);
	return;
}

static struct timespec tsp[1];

static inline size_t
hrclock_print(char *buf, size_t __attribute__((unused)) len)
{
	clock_gettime(CLOCK_REALTIME_COARSE, tsp);
	return sprintf(buf, "%ld.%09li", tsp->tv_sec, tsp->tv_nsec);
}

static void
linger_sock(int sock)
{
#if defined SO_LINGER
	struct linger lng[1] = {{.l_onoff = 1, .l_linger = 1}};
	(void)setsockopt(sock, SOL_SOCKET, SO_LINGER, lng, sizeof(*lng));
#endif	/* SO_LINGER */
	return;
}

static int
close_sock(int fd)
{
	linger_sock(fd);
	fdatasync(fd);
	shutdown(fd, SHUT_RDWR);
	return close(fd);
}


static char line[65536U];

static inline size_t
memncpy(char *restrict tgt, const char *src, size_t zrc)
{
	memcpy(tgt, src, zrc);
	return zrc;
}

static ssize_t
toout_logline(const char *buf, size_t len)
{
	const char *lp = buf;
	const char *const ep = buf + len;
	size_t sz, cz;

	/* this is a prefix that we prepend to each line */
	sz = hrclock_print(line, sizeof(line));
	line[sz++] = '\t';
	cz = sz;

	for (const char *eol;
	     lp < ep && (eol = memchr(lp, '\n', ep - lp)); lp = eol + 1U) {
		sz += memncpy(line + sz, line, cz);
		sz += memncpy(line + sz, lp, eol + 1U - lp);
	}
	if (sz == cz) {
		sz += memncpy(line + sz, buf, len);
		line[sz++] = '\n';
		/* use the prefix directly */
		cz = 0U;
	}

	/* write to stdout and to logfile */
	write(logfd, line + cz, sz - cz);
	fwrite(line + cz, 1, sz - cz, stderr);
	return sz - cz;
}

static ssize_t
toout_logline2(const char *pb, size_t pbz, const char *buf, size_t len)
{
	const char *lp = buf;
	const char *const ep = buf + len;
	const char *eol;
	size_t sz, cz;

	/* again, a prefix that we're about to prepend */
	sz = hrclock_print(line, sizeof(line));
	line[sz++] = '\t';
	cz = sz;

	/* copy left-overs from last time */
	sz += memncpy(line + sz, pb, pbz);

	for (; lp < ep && (eol = memchr(lp, '\n', ep - lp)); lp = eol + 1U) {
		sz += memncpy(line + sz, line, cz);
		sz += memncpy(line + sz, lp, eol + 1U - lp);
	}

	/* write to stdout and to logfile */
	write(logfd, line + cz, sz - cz);
	fwrite(line + cz, 1, sz - cz, stderr);
	return sz - cz;
}


typedef struct {
	struct {
		uint8_t code:4;
		uint8_t rsv3:1;
		uint8_t rsv2:1;
		uint8_t rsv1:1;
		uint8_t finp:1;
	};
	struct {
		uint8_t plen:7;
		uint8_t mask:1;
	};
	uint16_t plen16;
	struct {
		uint32_t plen64;
		uint32_t plen32;
	};
	uint32_t mkey;
} wsfr_t;

static ssize_t
proc_beef(const char *buf, size_t len)
{
/* assume there WS frame(s) and little-endian here */
	wsfr_t fr[1U];
	size_t npr;

	for (npr = 0U; npr < len;) {
		const char *bp;
		size_t bz;

		memcpy(fr, buf + npr, sizeof(*fr));
		switch (fr->plen) {
		case 126U:
			bp = buf + npr + offsetof(wsfr_t, plen64);
			bz = be16toh(fr->plen16);
			break;
		case 127U:
			bp = buf + npr + offsetof(wsfr_t, mkey);
			bz = be64toh(fr->plen64);
			break;
		default:
			bp = buf + npr + offsetof(wsfr_t, plen16);
			bz = fr->plen;
			break;
		}

		if (fr->mask) {
			fputs("MASK\n", stderr);
			bz += sizeof(fr->mkey);
		}

		if ((bp - buf) + bz > len) {
			fputs("CONT?\n", stderr);
			break;
		}

		switch (fr->code) {
			static char lefto[4096U];
			static size_t nlefto;
			ssize_t nwr;

		case 0x0U:
			/* frame continuation */
			if (nlefto) {
				toout_logline("CONT!", 5U);
				nwr = toout_logline2(lefto, nlefto, bp, bz);
				if ((size_t)nwr < bz) {
					/* stash the rest for CONT */
					nlefto = bz + nlefto - nwr;
					memcpy(lefto, bp + nwr, nlefto);
				}
				break;
			}
		case 0x1U:
			/* text message */
			nlefto = 0U;
			nwr = toout_logline(bp, bz);
			if ((size_t)nwr < bz) {
				/* stash the rest for CONT */
				nlefto = bz - nwr;
				memcpy(lefto, bp + nwr, nlefto);
			}
			break;
		case 0x2U:
			/* binary frame */
			toout_logline("BDATA", 5U);
			/* pretend we proc'd it */
			break;
		case 0x9U:
			/* ping */
			toout_logline("PING?", 5U);
			ping++;
			break;
		case 0xaU:
			/* pong */
			toout_logline("PONG?", 5U);
			break;
		default:
		case 0x8U:
			/* conn close :O */
			toout_logline("CLOS?", 5U);
			return -1;
		}
		/* calc new npr offset */
		npr = bp - buf + bz;
	}
	return npr;
}

static void
reply_heartbeat(ssl_ctx_t ss)
{
	static const char pong[] = {0x8a, 0x00};

	tls_send(ss, pong, sizeof(pong), 0);
	ping--;
	toout_logline("PONG!", 5U);
	return;
}

typedef struct coin_data_s {
	struct tm tm[1];
	char *bid;
	size_t blen;
	char *ask;
	size_t alen;
	char *tra;
	size_t tlen;
} *coin_data_t;

static void
open_outfile(void)
{
	if ((logfd = open(logfile, O_WRONLY | O_CREAT, 0644)) < 0) {
		serror("cannot open outfile `%s'", logfile);
		exit(EXIT_FAILURE);
	}
	/* coinw to the end */
	lseek(logfd, 0, SEEK_END);
	return;
}

static void
rotate_outfile(void)
{
	static char msg[] = "rotate...midnight";
	struct tm tm[1];
	char new[256], *n = new;
	time_t now;

	/* get a recent time stamp */
	now = time(NULL);
	gmtime_r(&now, tm);
	strncpy(n, logfile, logfsz);
	n += logfsz;
	*n++ = '-';
	strncpy(n, hostname, hostnsz);
	n += hostnsz;
	*n++ = '-';
	strftime(n, sizeof(new) - logfsz - hostnsz, "%Y-%m-%dT%H:%M:%S", tm);

	fprintf(stderr, "new \"%s\"\n", new);

	/* close the old file */
	toout_logline(msg, sizeof(msg) - 1);
	close_sock(logfd);
	/* rename it and reopen under the old name */
	rename(logfile, new);
	open_outfile(); 
	return;
}


static void
ws_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
/* we know that w is part of the coin_ctx_s structure */
	coin_ctx_t ctx = w->data;
	size_t maxr = sizeof(gbuf) - boff;
	ssize_t nrd;

	if ((nrd = tls_recv(ctx->ss, gbuf + boff, maxr, 0)) <= 0) {
		/* connexion reset or something? */
		serror("recv(%d) failed", w->fd);
		goto unroll;
	}
	/* terminate with \nul and check */
	gbuf[boff + nrd] = '\0';

#if 1
/* debugging */
	fprintf(stderr, "WS (%u) read %zu+%zi/%zu bytes\n", ctx->st, boff, nrd, maxr);
#endif	/* 1 */

	switch (ctx->st) {
	case COIN_ST_CONN:
	case COIN_ST_CONND:
		if (nrd < 12) {
			;
		} else if (!memcmp(gbuf, "HTTP/1.1 101", 12U)) {
			ctx->st = COIN_ST_CONND;
			fwrite(gbuf, 1, nrd, stderr);
			fputs("CONND\n", stderr);
		}
		boff = 0;
		break;

	case COIN_ST_JOIN:
		/* assume that we've successfully joined */
		ctx->st = COIN_ST_JOIND;
	case COIN_ST_JOIND:;
		ssize_t npr;

		if ((npr = proc_beef(gbuf, boff + nrd)) < 0) {
			goto unroll;
		}
		if (ping) {
			reply_heartbeat(ctx->ss);
		}
		/* keep a reference of our time stamp */
		*ctx->last_act = *tsp;
		/* move things around */
		if (npr < (ssize_t)(boff + nrd)) {
			/* havent'f finished processing a line */
			memmove(gbuf, gbuf + npr, boff = (boff + nrd - npr));
			break;
		}
	default:
		boff = 0;
		break;
	}

	ev_timer_again(EV_A_ ctx->timer);
	ctx->nothing = 0;
	return;

unroll:
	/* connection reset */
	toout_logline("restart in 3", 12);
	sleep(1);
	toout_logline("restart in 2", 12);
	sleep(1);
	toout_logline("restart in 1", 12);
	sleep(1);
	toout_logline("restart", 7);
	ctx->nothing = 0;
	ctx->st = COIN_ST_RECONN;
	return;
}

static void
midnight_cb(EV_PU_ ev_periodic *UNUSED(w), int UNUSED(r))
{
	rotate_outfile();
	return;
}

static void
sighup_cb(EV_PU_ ev_signal *w, int UNUSED(r))
{
	coin_ctx_t ctx = w->data;
	rotate_outfile();
	ctx->st = COIN_ST_RECONN;
	return;
}

static void
silence_cb(EV_PU_ ev_timer *w, int UNUSED(revents))
{
	coin_ctx_t ctx = w->data;

	toout_logline("nothing", 7);
	if (ctx->nothing++ >= NTIMEOUTS) {
		switch (ctx->st) {
		case COIN_ST_SLEEP:
			ctx->nothing = 0;
			toout_logline("wakey wakey", 11);
			ctx->st = COIN_ST_RECONN;
			break;
		default:
			/* only fall asleep when subscribed */
			ctx->nothing = 0;
			toout_logline("suspend", 7);
			ctx->st = COIN_ST_NODATA;
			break;
		}
	}
	return;
}

static void
sigint_cb(EV_PU_ ev_signal *w, int UNUSED(revents))
{
	coin_ctx_t ctx = w->data;
	/* quit the whole shebang */
	toout_logline("C-c", 3);
	ctx->nothing = 0;
	ctx->st = COIN_ST_INTR;
	return;
}


static ssize_t
request_sub(ssl_ctx_t ss, const char *chan, size_t clen)
{
	size_t loff = 0U;
	wsfr_t fr = {
		.code = 0x1U,
		.rsv3 = 0U,
		.rsv2 = 0U,
		.rsv1 = 0U,
		.finp = 1U,
		.mask = 1U,
		.plen = 0U,
	};
	size_t slen;

	if (clen >= 65536U) {
		fr.plen = 127U;
		fr.plen64 = htobe32((uint64_t)clen >> 32U);
		fr.plen32 = htobe32(clen & 0xffffffffU);
		slen = sizeof(wsfr_t);
	} else if (clen >= 126U) {
		fr.plen = 126U;
		fr.plen16 = htobe16(clen);
		slen = offsetof(wsfr_t, plen64) + sizeof(fr.mkey);
	} else {
		fr.plen = clen;
		slen = offsetof(wsfr_t, plen16) + sizeof(fr.mkey);
	}

	memcpy(line + loff, &fr, slen);
	loff += slen;
	memcpy(line + loff, chan, clen);
	loff += clen;
	line[loff] = '\0';

	fwrite(chan, 1, clen, stderr);
	return tls_send(ss, line, loff, 0);
}

static void
requst_coin(EV_P_ coin_ctx_t ctx)
{
	static const char greq[] = "\
GET / HTTP/1.1\r\n\
Host: " API_HOST "\r\n\
Pragma: no-cache\r\n\
Origin: http://coinmatch.com\r\n\
Sec-WebSocket-Version: 13\r\n\
Sec-WebSocket-Key: e8w+o5wQsV0rXFezPUS8XQ==\r\n\
User-Agent: Mozilla/5.0\r\n\
Upgrade: websocket\r\n\
Cache-Control: no-cache\r\n\
Connection: Upgrade\r\n\
\r\n";

	(void)EV_A;
	fputs("GETting\n", stderr);
	if (tls_send(ctx->ss, greq, sizeof(greq) - 1U, 0) < 0) {
		ctx->st = COIN_ST_UNK;
		return;
	}
	return;
}

static void
subscr_coin(EV_P_ coin_ctx_t ctx)
{
	static char buf[256U];

	for (size_t i = 0U; i < ctx->nsubs; i++) {
		int len = snprintf(buf, sizeof(buf), "\
{\"type\": \"subscribe\", \"product_id\": \"%s\"}\r\n", ctx->subs[i]);
		request_sub(ctx->ss, buf, len);
	}

	/* reset nothing counter and start the nothing timer */
	ctx->nothing = 0;
	ev_timer_again(EV_A_ ctx->timer);
	boff = 0;

	ctx->st = COIN_ST_JOIN;
	/* initialise our last activity stamp */
	*ctx->last_act = *tsp;
	return;
}

static void
init_coin(EV_P_ coin_ctx_t ctx)
{
/* this init process is two part: request a token, then do the subscriptions */
	ctx->st = COIN_ST_UNK;
	boff = 0U;

	fprintf(stderr, "INIT\n");
	ev_timer_again(EV_A_ ctx->timer);
	if ((ctx->ss = open_tls(API_HOST, API_PORT)) == NULL) {
			serror("\
Error: cannot connect");
		/* retry soon, we just use the watcher for this */
		ctx->st = COIN_ST_SLEEP;
		return;
	}

	ev_io_init(ctx->watcher, ws_cb, tls_fd(ctx->ss), EV_READ);
	ev_io_start(EV_A_ ctx->watcher);
	ctx->watcher->data = ctx;
	ctx->st = COIN_ST_CONN;

	/* send our friendly demand */
	requst_coin(EV_A_ ctx);
	return;
}

static void
deinit_coin(EV_P_ coin_ctx_t ctx)
{
	fprintf(stderr, "DEINIT\n");
	/* stop the watcher */
	ev_io_stop(EV_A_ ctx->watcher);

	/* shutdown the network socket */
	if (ctx->ss != NULL) {
		close_tls(ctx->ss);
	}
	ctx->ss = NULL;

	/* set the state to unknown */
	ctx->st = COIN_ST_UNK;
	boff = 0U;
	return;
}

static void
reinit_coin(EV_P_ coin_ctx_t ctx)
{
	fprintf(stderr, "REINIT\n");
	deinit_coin(EV_A_ ctx);
	init_coin(EV_A_ ctx);
	return;
}


/* only cb's we allow here */
static void
prepare(EV_P_ ev_prepare *w, int UNUSED(revents))
{
	coin_ctx_t ctx = w->data;

	fprintf(stderr, "PREP(%u)\n", ctx->st);
	switch (ctx->st) {
	case COIN_ST_UNK:
		/* initialise everything, sets the state */
		init_coin(EV_A_ ctx);
		break;

	case COIN_ST_CONND:
		/* waiting for that HTTP 101 */
		subscr_coin(EV_A_ ctx);
		break;

	case COIN_ST_JOIN:
		break;
	case COIN_ST_JOIND:
		/* check if there's messages from the channel */
		if (tsp->tv_sec - ctx->last_act->tv_sec >= MAX_INACT) {
			goto unroll;
		}
		break;

	case COIN_ST_NODATA:
		fprintf(stderr, "NODATA -> RECONN\n");
	case COIN_ST_RECONN:
		fprintf(stderr, "reconnection requested\n");
		reinit_coin(EV_A_ ctx);
		break;
	case COIN_ST_INTR:
		/* disconnect and unroll */
		deinit_coin(EV_A_ ctx);
		ev_break(EV_A_ EVBREAK_ALL);
		break;
	case COIN_ST_SLEEP:
	default:
		break;
	}
	return;

unroll:
	/* connection reset */
	toout_logline("restart in 3", 12);
	sleep(1);
	toout_logline("restart in 2", 12);
	sleep(1);
	toout_logline("restart in 1", 12);
	sleep(1);
	toout_logline("restart", 7);
	ctx->nothing = 0;
	ctx->st = COIN_ST_RECONN;
	return;
}

static void
init_ev(EV_P_ coin_ctx_t ctx)
{
	ev_signal_init(ctx->sigi, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ ctx->sigi);
	ev_signal_init(ctx->sigp, sigint_cb, SIGPIPE);
	ev_signal_start(EV_A_ ctx->sigp);
	ctx->sigi->data = ctx;
	ctx->sigp->data = ctx;

	/* inc nothing counter every 3 seconds */
	ev_timer_init(ctx->timer, silence_cb, 0.0, TIMEOUT);
	ctx->timer->data = ctx;

	/* the midnight tick for file rotation, also upon sighup */
	ev_periodic_init(ctx->midnight, midnight_cb, MIDNIGHT, ONE_DAY, NULL);
	ev_periodic_start(EV_A_ ctx->midnight);
	ev_signal_init(ctx->sigh, sighup_cb, SIGHUP);
	ev_signal_start(EV_A_ ctx->sigh);
	ctx->midnight->data = ctx;
	ctx->sigh->data = ctx;

	/* prepare and check cbs */
	ev_prepare_init(ctx->prep, prepare);
	ev_prepare_start(EV_A_ ctx->prep);
	ctx->prep->data = ctx;
	return;
}

static void
deinit_ev(EV_P_ coin_ctx_t ctx)
{
	ev_timer_stop(EV_A_ ctx->timer);
	ev_signal_stop(EV_A_ ctx->sigi);
	ev_signal_stop(EV_A_ ctx->sigp);

	ev_signal_stop(EV_A_ ctx->sigh);
	ev_periodic_stop(EV_A_ ctx->midnight);

	ev_prepare_stop(EV_A_ ctx->prep);
	return;
}


#include "coin.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	struct coin_ctx_s ctx[1U];

	if (yuck_parse(argi, argc, argv) < 0) {
		return 1;
	}

	EV_P = ev_default_loop(0);

	/* rinse */
	memset(ctx, 0, sizeof(*ctx));
	/* open our outfile */
	open_outfile();
	/* put the hostname behind logfile */
	(void)gethostname(hostname, sizeof(hostname));
	hostnsz = strlen(hostname);

	/* make sure we won't forget them subscriptions */
	ctx->subs = argi->args;
	ctx->nsubs = argi->nargs;

	/* and initialise the libev part of this project */
	init_ev(EV_A_ ctx);

	/* obtain a loop */
	{
		/* work */
		ev_run(EV_A_ 0);
	}

	/* hm? */
	deinit_ev(EV_A_ ctx);
	ev_loop_destroy(EV_DEFAULT_UC);

	/* that's it */
	close_sock(logfd);
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* coin.c ends here */
