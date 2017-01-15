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
#include "wssnarf.h"
#include "nifty.h"

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

typedef struct timespec *timespec_t;

typedef enum {
	COIN_ST_UNK,
	COIN_ST_CONN,
	COIN_ST_CONND,
	COIN_ST_AUTH,
	COIN_ST_AUTHD,
	COIN_ST_JOIN,
	COIN_ST_JOIND,
	COIN_ST_NODATA,
	COIN_ST_INTR,
	/* final state */
	COIN_ST_FINI,
} coin_st_t;

struct wssnarf_s {
	/* libev's idea of the socket below */
	ev_io watcher[1];
	ev_timer timer[1];
	ev_signal sigi[1];
	ev_signal sigp[1];
	ev_signal sigh[1];
	ev_periodic midnight[1];
	ev_prepare prep[1];

	/* websocket context */
	ws_t ws;

	/* state */
	coin_st_t st;

	struct timespec last_act[1];

	/* user shit */
	struct coin_bla_s *ctx;

	char hostname[256];
	wssnarf_param_t p;
	int logfd;
};

/* always have room for the timestamp */
#define INI_GBOF	21U
static char gbuf[1048576U];
static size_t gbof = INI_GBOF;


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


static inline size_t
memncpy(char *restrict tgt, const char *src, size_t zrc)
{
	memcpy(tgt, src, zrc);
	return zrc;
}

static inline size_t
memnmove(char *tgt, const char *src, size_t zrc)
{
	memmove(tgt, src, zrc);
	return zrc;
}

static int
loghim(int logfd, const char *buf, size_t len)
{
	size_t prfz;

	/* this is a prefix that we prepend to each line */
	prfz = hrclock_print(gbuf, INI_GBOF);
	gbuf[prfz++] = '\t';

	memmove(gbuf + prfz, buf, len);
	gbuf[prfz + len++] = '\n';
	write(logfd, gbuf, prfz + len);
	fwrite(gbuf, 1, prfz + len, stderr);
	return 0;
}

static ssize_t
logwss(int logfd, const char *buf, size_t len)
{
	const char *lp = buf;
	size_t prfz;

	/* this is a prefix that we prepend to each line */
	prfz = hrclock_print(gbuf, INI_GBOF);
	gbuf[prfz++] = '\t';

	for (const char *eol, *const ep = buf + len;
	     lp < ep && (eol = memchr(lp, '\n', ep - lp));
	     lp = eol + 1U) {
		size_t z;
		z = memnmove(gbuf + prfz, lp, eol - lp);
		gbuf[prfz + z++] = '\n';
		z = massage(gbuf + prfz, z);
		write(logfd, gbuf, prfz + (eol - lp) + 1U);
		fwrite(gbuf, 1, prfz + (eol - lp) + 1U, stderr);
	}
	return lp - buf;
}


static void
open_outfile(wssnarf_t ctx)
{
	if ((ctx->logfd = open(ctx->p.ofn, O_WRONLY | O_CREAT, 0644)) < 0) {
		serror("cannot open outfile `%s'", ctx->p.ofn);
		exit(EXIT_FAILURE);
	}
	/* coinw to the end */
	lseek(ctx->logfd, 0, SEEK_END);
	return;
}

static void
rotate_outfile(wssnarf_t ctx)
{
	static char msg[] = "rotate...midnight";
	struct tm tm[1];
	char new[256], *n = new;
	size_t lnz, hnz;
	time_t now;

	/* get a recent time stamp */
	now = time(NULL);
	gmtime_r(&now, tm);
	n += lnz = memncpy(n, ctx->p.ofn, strlen(ctx->p.ofn));
	*n++ = '-';
	n += hnz = memncpy(n, ctx->hostname, strlen(ctx->hostname));
	*n++ = '-';
	strftime(n, sizeof(new) - lnz - hnz, "%Y-%m-%dT%H:%M:%S", tm);

	fprintf(stderr, "new \"%s\"\n", new);

	/* close the old file */
	loghim(ctx->logfd, msg, sizeof(msg) - 1);
	close_sock(ctx->logfd);
	/* rename it and reopen under the old name */
	rename(ctx->p.ofn, new);
	open_outfile(ctx);
	return;
}


static void
ws_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
/* we know that w is part of the coin_ctx_s structure */
	wssnarf_t ctx = w->data;
	size_t maxr = sizeof(gbuf) - gbof;
	ssize_t nrd;

	if ((nrd = ws_recv(ctx->ws, gbuf + gbof, maxr, 0)) <= 0) {
		/* connexion reset or something? */
		serror("recv(%d) failed, read %zi", w->fd, nrd);
		goto unroll;
	}

#if 1
/* debugging */
	fprintf(stderr, "WS (%u) read %zu+%zi/%zu bytes\n", ctx->st, gbof, nrd, maxr);
#endif	/* 1 */

	switch (ctx->st) {
		ssize_t npr;

	case COIN_ST_CONN:
		ctx->st = COIN_ST_CONND;
	case COIN_ST_CONND:
		goto log;

	case COIN_ST_AUTH:
		/* we've got a cb whilst authing, consider ourselves authd */
		ctx->st = COIN_ST_AUTHD;
	case COIN_ST_AUTHD:
		goto log;

	case COIN_ST_JOIN:
		ctx->st = COIN_ST_JOIND;
	case COIN_ST_JOIND:
	log:
		gbof += nrd;
		/* log him */
		npr = logwss(ctx->logfd, gbuf + INI_GBOF, gbof - INI_GBOF);
		memnmove(gbuf + INI_GBOF, gbuf + INI_GBOF + npr, gbof - npr);
		gbof -= npr;

		/* keep a reference of our time stamp */
		*ctx->last_act = *tsp;
		break;

	default:
		gbof = INI_GBOF;
		break;
	}
	return;

unroll:
	/* connection reset */
	loghim(ctx->logfd, "restart", 7);
	ev_break(EV_A_ EVBREAK_ALL);
	return;
}

static void
midnight_cb(EV_PU_ ev_periodic *UNUSED(w), int UNUSED(r))
{
	wssnarf_t ctx = w->data;
	rotate_outfile(ctx);
	return;
}

static void
sighup_cb(EV_PU_ ev_signal *w, int UNUSED(r))
{
	wssnarf_t ctx = w->data;
	rotate_outfile(ctx);
	return;
}

static void
hbeat_cb(EV_PU_ ev_timer *w, int UNUSED(revents))
{
	wssnarf_t ctx = w->data;

	loghim(ctx->logfd, "HEARTBEAT", 9U);
	if (UNLIKELY(heartbeat(ctx->ws) < 0)) {
		ctx->st = COIN_ST_NODATA;
	}
	return;
}

static void
sigint_cb(EV_PU_ ev_signal *w, int UNUSED(revents))
{
	wssnarf_t ctx = w->data;
	/* quit the whole shebang */
	loghim(ctx->logfd, "C-c", 3);
	ctx->st = COIN_ST_INTR;
	return;
}


static int
conn_coin(EV_P_ wssnarf_t ctx)
{
/* this init process is two part: request a token, then do the subscriptions */
	gbof = INI_GBOF;

	fprintf(stderr, "INIT\n");
	ev_timer_again(EV_A_ ctx->timer);
	if ((ctx->ws = ws_open(ctx->p.url)) < 0) {
		serror("Error: cannot connect");
		/* better retry */
		return -1;
	}

	ev_io_init(ctx->watcher, ws_cb, ws_fd(ctx->ws), EV_READ);
	ev_io_start(EV_A_ ctx->watcher);
	ctx->watcher->data = ctx;
	return 0;
}

static int
fini_coin(EV_P_ wssnarf_t ctx)
{
	fprintf(stderr, "DEINIT\n");
	/* stop the watcher */
	ev_io_stop(EV_A_ ctx->watcher);

	/* shutdown the network socket */
	if (ctx->ws != NULL) {
		ws_close(ctx->ws);
	}
	ctx->ws = NULL;

	/* set the state to unknown */
	gbof = INI_GBOF;
	return 0;
}

__attribute__((weak)) int
auth_coin(ws_t UNUSED(ws))
{
	fputs("TRIVIAL AUTH\n", stderr);
	return 1;
}

__attribute__((weak)) int
join_coin(ws_t UNUSED(ws))
{
	fputs("TRIVIAL JOIN\n", stderr);
	return 1;
}

__attribute__((weak)) int
heartbeat(ws_t UNUSED(ws))
{
	return 0;
}

__attribute__((weak)) size_t
massage(char *restrict UNUSED(buf), size_t bsz)
{
	return bsz;
}


/* only cb's we allow here */
static void
prepare(EV_P_ ev_prepare *w, int UNUSED(revents))
{
	wssnarf_t ctx = w->data;

	fprintf(stderr, "PREP(%u)\n", ctx->st);
	switch (ctx->st) {
		int r;

	case COIN_ST_UNK:

	case COIN_ST_CONN:
		/* initialise everything, sets the state */
		if ((r = conn_coin(EV_A_ ctx) < 0)) {
			break;
		}
		ctx->st = COIN_ST_CONND;

	case COIN_ST_CONND:
		/* reset nothing counter and start the nothing timer */
		gbof = INI_GBOF;
		ctx->st = COIN_ST_AUTH;

	case COIN_ST_AUTH:
		/* let's auth */
		if ((r = auth_coin(ctx->ws)) < 0) {
			/* unsuccessful */
			ctx->st = COIN_ST_CONND;
			break;
		} else if (r == 0) {
			break;
		}
		/* otherwise we're good to go */
		ctx->st = COIN_ST_AUTHD;

	case COIN_ST_AUTHD:
		ctx->st = COIN_ST_JOIN;

	case COIN_ST_JOIN:
		/* let's join */
		if ((r = join_coin(ctx->ws)) < 0) {
			/* unsuccessful, go back to authd */
			ctx->st = COIN_ST_AUTHD;
			break;
		} else if (r == 0) {
			break;
		}
		/* otherwise we're good to go */
		ctx->st = COIN_ST_JOIND;

	case COIN_ST_JOIND:
		/* check if there's messages from the channel */
		if (tsp->tv_sec - ctx->last_act->tv_sec < MAX_INACT) {
			break;
		}
		/* fallthrough */
	case COIN_ST_NODATA:
	case COIN_ST_INTR:
		/* disconnect and unroll */
		fini_coin(EV_A_ ctx);
		ctx->st = COIN_ST_FINI;
	case COIN_ST_FINI:
		ev_break(EV_A_ EVBREAK_ALL);
		break;
	default:
		break;
	}
	return;
}

static void
init_ev(EV_P_ wssnarf_t ctx)
{
	ev_signal_init(ctx->sigi, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ ctx->sigi);
	ev_signal_init(ctx->sigp, sigint_cb, SIGPIPE);
	ev_signal_start(EV_A_ ctx->sigp);
	ctx->sigi->data = ctx;
	ctx->sigp->data = ctx;

	/* inc nothing counter every 3 seconds */
	ev_timer_init(ctx->timer, hbeat_cb, 0.0, TIMEOUT);
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
deinit_ev(EV_P_ wssnarf_t ctx)
{
	ev_timer_stop(EV_A_ ctx->timer);
	ev_signal_stop(EV_A_ ctx->sigi);
	ev_signal_stop(EV_A_ ctx->sigp);

	ev_signal_stop(EV_A_ ctx->sigh);
	ev_periodic_stop(EV_A_ ctx->midnight);

	ev_prepare_stop(EV_A_ ctx->prep);
	return;
}


wssnarf_t
make_wssnarf(wssnarf_param_t prm)
{
	struct wssnarf_s *r = calloc(1U, sizeof(*r));
	EV_P = ev_default_loop(0);

	r->p = prm;
	init_ev(EV_A_ r);

	/* make sure we've got something to write to */
	open_outfile(r);

	/* put the hostname behind logfile */
	(void)gethostname(r->hostname, sizeof(r->hostname));
	return r;
}

void
free_wssnarf(wssnarf_t ctx)
{
	EV_P = ev_default_loop(0);
	deinit_ev(EV_A_ ctx);
	close_sock(ctx->logfd);
	free(ctx);
	ev_loop_destroy(EV_DEFAULT_UC);
	return;
}

int
run_wssnarf(wssnarf_t UNUSED(ctx))
{
	EV_P = ev_default_loop(0);
	/* work */
	ev_run(EV_A_ 0);
	return 0;
}

/* wssnarf.c ends here */
