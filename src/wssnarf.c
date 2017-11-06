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

#define ONE_DAY		86400.0
#define MIDNIGHT	0.0

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
	ev_signal sigi[1];
	ev_signal sigp[1];
	ev_signal sigh[1];
	ev_periodic midnight[1];
	ev_prepare prep[1];

	/* number of registered sockets */
	size_t ns;
	/* socket context */
	ws_t ws[4U];
	wssnarf_param_t p[4U];
	ev_io watcher[4U];
	ev_timer timer[4U];
	coin_st_t st[4U];
	struct timespec last_act[4U];
	struct {
		char *p;
		size_t z;
		size_t n;
	} sbuf[4U];

	char hostname[256U];

	const char *logfile;
	int logfd;
};

/* always have room for the timestamp */
#define INI_GBOF	21U


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


static int
loghim(int logfd, const char *buf, size_t len)
{
	char xbuf[len + INI_GBOF + 1U];
	size_t prfz;

	/* this is a prefix that we prepend to each line */
	prfz = hrclock_print(xbuf, INI_GBOF);
	xbuf[prfz++] = '\t';

	memcpy(xbuf + prfz, buf, len);
	xbuf[prfz + len++] = '\n';
	write(logfd, xbuf, prfz + len);
	fwrite(xbuf, 1, prfz + len, stderr);
	return 0;
}

static ssize_t
logwss(int logfd, char *buf, size_t len)
{
/* BUF is assumed to contain INI_GBOF bytes of breathing space */
	const char *lp = buf;
	size_t prfz;

	/* this is a prefix that we prepend to each line */
	prfz = hrclock_print(buf - INI_GBOF, INI_GBOF);
	buf[prfz++ - INI_GBOF] = '\t';

	for (const char *eol, *const ep = buf + len;
	     lp < ep && (eol = memchr(lp, '\n', ep - lp));
	     lp = eol + 1U) {
		size_t z;
		z = memnmove(buf + prfz - INI_GBOF, lp, eol - lp);
		buf[prfz - INI_GBOF + z++] = '\n';
		z = massage(buf + prfz - INI_GBOF, z);
		write(logfd, buf - INI_GBOF, prfz + z);
		fwrite(buf - INI_GBOF, 1, prfz + z, stderr);
	}
	return lp - buf;
}


static int
open_outfile(const char *logfile)
{
	int s;

	if ((s = open(logfile, O_WRONLY | O_CREAT, 0644)) < 0) {
		serror("cannot open outfile `%s'", logfile);
		return -1;
	}
	/* coinw to the end */
	lseek(s, 0, SEEK_END);
	return s;
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
	n += lnz = memncpy(n, ctx->logfile, strlen(ctx->logfile));
	*n++ = '-';
	n += hnz = memncpy(n, ctx->hostname, strlen(ctx->hostname));
	*n++ = '-';
	strftime(n, sizeof(new) - lnz - hnz, "%Y-%m-%dT%H:%M:%S", tm);

	fprintf(stderr, "new \"%s\"\n", new);

	/* close the old file */
	loghim(ctx->logfd, msg, sizeof(msg) - 1);
	close_sock(ctx->logfd);
	/* rename it and reopen under the old name */
	rename(ctx->logfile, new);
	ctx->logfd = open_outfile(ctx->logfile);
	return;
}


static void
ws_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
/* we know that w is part of the coin_ctx_s structure */
	wssnarf_t ctx = w->data;
	const size_t idx = w - ctx->watcher;
	size_t xn = ctx->sbuf[idx].n;
	size_t xz = ctx->sbuf[idx].z;
	char *xp = ctx->sbuf[idx].p;
	ssize_t nrd;

	/* check if there's still room for, say, 4000U bytes */
	if (UNLIKELY(xz < xn + 4000U)) {
		const size_t nuz = (xz * 2U) ?: 4096U;
		ctx->sbuf[idx].p = xp = realloc(xp, nuz);
		ctx->sbuf[idx].z = xz = nuz;
	}

	if ((nrd = ws_recv(ctx->ws[idx], xp + xn, xz - xn, 0)) < 0) {
		/* connexion reset or something? */
		serror("recv(%d) failed, read %zi", w->fd, nrd);
		goto unroll;
	}

#if 1
/* debugging */
	fprintf(stderr, "WS[%zu](%u) read %zu+%zi/%zu bytes\n", idx, ctx->st[idx], xn, nrd, xz);
#endif	/* 1 */

	/* advance gbof */
	xn += nrd;
	switch (ctx->st[idx]) {
		ssize_t npr;
		int r;

	case COIN_ST_UNK:
		ctx->st[idx] = COIN_ST_CONN;
	case COIN_ST_CONN:
		/* we've got a cb whilst authing,
		 * see what the authd predicate has to say */
		if ((r = connd_coin(xp + INI_GBOF, xn - INI_GBOF)) < 0) {
			/* complete failue */
			goto logunr;
		}
		/* see if we can upgrade the connection */
		ctx->st[idx] += (coin_st_t)(r > 0);
	case COIN_ST_CONND:
		goto log;

	case COIN_ST_AUTH:
		/* we've got a cb whilst authing,
		 * see what the authd predicate has to say */
		if ((r = authd_coin(xp + INI_GBOF, xn - INI_GBOF)) < 0) {
			/* complete failue */
			goto logunr;
		}
		/* see if we can upgrade the connection */
		ctx->st[idx] += (coin_st_t)(r > 0);
	case COIN_ST_AUTHD:
		goto log;

	case COIN_ST_JOIN:
		/* see what they say about the joinedness */
		if ((r = joind_coin(xp + INI_GBOF, xn - INI_GBOF)) < 0) {
			/* complete failue */
			goto logunr;
		}
		/* see if we can upgrade the connection */
		ctx->st[idx] += (coin_st_t)(r > 0);
	case COIN_ST_JOIND:
	log:
		/* log him */
		npr = logwss(ctx->logfd, xp + INI_GBOF, xn - INI_GBOF);
		memmove(xp + INI_GBOF, xp + INI_GBOF + npr, xn - npr);
		xn -= npr;

		/* keep a reference of our time stamp */
		ctx->last_act[idx] = *tsp;
		break;

	default:
		xn = INI_GBOF;
		break;
	}

	ctx->sbuf[idx].n = xn;
	return;

logunr:
	logwss(ctx->logfd, xp + INI_GBOF, xn + nrd - INI_GBOF);
unroll:
	/* connection reset */
	loghim(ctx->logfd, "restart", 7U);
	ev_break(EV_A_ EVBREAK_ALL);
	return;
}

static void
sio3_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
/* we know that w is part of the coin_ctx_s structure */
	wssnarf_t ctx = w->data;
	const size_t idx = w - ctx->watcher;
	size_t xn = ctx->sbuf[idx].n;
	size_t xz = ctx->sbuf[idx].z;
	char *xp = ctx->sbuf[idx].p;
	ssize_t nrd;

	/* check if there's still room for, say, 4000U bytes */
	if (UNLIKELY(xz < xn + 4000U)) {
		const size_t nuz = (xz * 2U) ?: 4096U;
		ctx->sbuf[idx].p = xp = realloc(xp, nuz);
		ctx->sbuf[idx].z = xz = nuz;
	}

	if ((nrd = ws_recv(ctx->ws[idx], xp + xn, xz - xn, 0)) <= 0) {
		/* connexion reset or something? */
		serror("recv(%d) failed, read %zi", w->fd, nrd);
		goto unroll;
	}

#if 1
/* debugging */
	fprintf(stderr, "WS[%zu](%u) read %zu+%zi/%zu bytes\n", idx, ctx->st[idx], xn, nrd, xz);
#endif	/* 1 */

	/* advance gbof */
	xn += nrd;
	switch (ctx->st[idx]) {
		ssize_t npr;
		int r;

	case COIN_ST_UNK:
		ctx->st[idx] = COIN_ST_CONN;
	case COIN_ST_CONN:
		/* we've got a cb whilst authing,
		 * see what the authd predicate has to say */
		if ((r = connd_coin(xp + INI_GBOF, xn - INI_GBOF)) < 0) {
			/* complete failue */
			goto logunr;
		}
		/* see if we can upgrade the connection */
		ctx->st[idx] += (coin_st_t)(r > 0);
	case COIN_ST_CONND:
		ws_send(ctx->ws[idx], "2probe", 6U, 0);
		goto log;

	case COIN_ST_AUTH:
		/* we've got a cb whilst authing,
		 * see what the authd predicate has to say */
		if ((r = authd_coin(xp + INI_GBOF, xn - INI_GBOF)) < 0) {
			/* complete failue */
			goto logunr;
		}
		/* see if we can upgrade the connection */
		ctx->st[idx] += (coin_st_t)(r > 0);
	case COIN_ST_AUTHD:
		ws_send(ctx->ws[idx], "5", 1U, 0);
		goto log;

	case COIN_ST_JOIN:
		/* see what they say about the joinedness */
		if ((r = joind_coin(xp + INI_GBOF, xn - INI_GBOF)) < 0) {
			/* complete failue */
			goto logunr;
		}
		/* see if we can upgrade the connection */
		ctx->st[idx] += (coin_st_t)(r > 0);
	case COIN_ST_JOIND:
	log:
		/* log him */
		npr = logwss(ctx->logfd, xp + INI_GBOF, xn - INI_GBOF);
		memmove(xp + INI_GBOF, xp + INI_GBOF + npr, xn - npr);
		xn -= npr;

		/* keep a reference of our time stamp */
		ctx->last_act[idx] = *tsp;
		break;

	default:
		xn = INI_GBOF;
		break;
	}

	ctx->sbuf[idx].n = xn;
	return;

logunr:
	logwss(ctx->logfd, xp + INI_GBOF, xn + nrd - INI_GBOF);
unroll:
	/* connection reset */
	loghim(ctx->logfd, "restart", 7U);
	ev_break(EV_A_ EVBREAK_ALL);
	return;
}

static void
rest_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
/* we know that w is part of the coin_ctx_s structure */
	wssnarf_t ctx = w->data;
	size_t idx = w - ctx->watcher;
	size_t xn = ctx->sbuf[idx].n;
	size_t xz = ctx->sbuf[idx].z;
	char *xp = ctx->sbuf[idx].p;
	ssize_t nrd;
	ssize_t npr;

	/* check if there's still room for, say, 4096U bytes */
	if (UNLIKELY(xz < xn + 4096U)) {
		const size_t nuz = (xz * 2U) ?: 4096U;
		ctx->sbuf[idx].p = xp = realloc(xp, nuz);
		ctx->sbuf[idx].z = xz = nuz;
	}

	if ((nrd = rest_recv(ctx->ws[idx], xp + xn, xz - xn, 0)) < 0) {
		/* connexion reset or something? */
		serror("recv(%d) failed, read %zi", w->fd, nrd);
		/* stop the watcher */
		ev_io_stop(EV_A_ w);

		/* shutdown the network socket */
		if (ctx->ws[idx] != NULL) {
			ws_close(ctx->ws[idx]);
		}
		ctx->ws[idx] = NULL;
		ctx->st[idx] = COIN_ST_UNK;
		xn = INI_GBOF;
		goto out;
	}

#if 1
/* debugging */
	fprintf(stderr, "REST (%u) read %zu+%zi/%zu bytes\n", ctx->st[idx], xn, nrd, xz);
#endif	/* 1 */

	/* advance gbof */
	xn += nrd;
	/* log him */
	npr = logwss(ctx->logfd, xp + INI_GBOF, xn - INI_GBOF);
	memmove(xp + INI_GBOF, xp + INI_GBOF + npr, xn - npr);
	xn -= npr;
out:
	ctx->sbuf[idx].n = xn;
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
	size_t idx = w - ctx->timer;

	loghim(ctx->logfd, "HEARTBEAT", 9U);
	if (UNLIKELY(heartbeat(ctx->ws[idx]) < 0)) {
		ctx->st[idx] = COIN_ST_NODATA;
	} else if (tsp->tv_sec - ctx->last_act[idx].tv_sec >
		   ctx->p[idx].max_inact) {
		ctx->st[idx] = COIN_ST_NODATA;
	}
	return;
}

static void
sigint_cb(EV_PU_ ev_signal *w, int UNUSED(revents))
{
	wssnarf_t ctx = w->data;
	/* quit the whole shebang */
	loghim(ctx->logfd, "C-c", 3U);

	/* consider them all interrupted */
	for (size_t i = 0U; i < ctx->ns; i++) {
		ctx->st[i] = COIN_ST_INTR;
	}
	return;
}


static int
open_coin(EV_P_ wssnarf_t ctx, size_t idx)
{
/* this init process is two part: request a token, then do the subscriptions */
	fprintf(stderr, "INIT\n");
	ev_timer_again(EV_A_ &ctx->timer[idx]);
	if ((ctx->ws[idx] = ws_open(ctx->p[idx].url)) == NULL) {
		serror("Error: cannot connect");
		/* better retry */
		return -1;
	}

	switch (ws_proto(ctx->ws[idx])) {
	case WS_PROTO_RAW:
	case WS_PROTO_WAMP:
	case WS_PROTO_SIO1:
		ev_io_init(&ctx->watcher[idx], ws_cb, ws_fd(ctx->ws[idx]), EV_READ);
		break;
	case WS_PROTO_REST:
		ev_io_init(&ctx->watcher[idx], rest_cb, ws_fd(ctx->ws[idx]), EV_READ);
		break;
	case WS_PROTO_SIO3:
		ev_io_init(&ctx->watcher[idx], sio3_cb, ws_fd(ctx->ws[idx]), EV_READ);
		break;
	default:
		errno = 0, serror("Error: unknown protocol");
		ws_close(ctx->ws[idx]);
		ctx->ws[idx] = NULL;
		return -1;
	}
	ev_io_start(EV_A_ &ctx->watcher[idx]);
	ctx->watcher[idx].data = ctx;
	ctx->st[idx] = COIN_ST_UNK;
	return 1;
}

static int
fini_coin(EV_P_ wssnarf_t ctx, size_t idx)
{
	fprintf(stderr, "DEINIT\n");
	/* stop the watcher */
	ev_io_stop(EV_A_ &ctx->watcher[idx]);

	/* shutdown the network socket */
	if (ctx->ws[idx] != NULL) {
		ws_close(ctx->ws[idx]);
	}
	ctx->ws[idx] = NULL;
	return 0;
}

__attribute__((weak)) int
conn_coin(ws_t UNUSED(ws))
{
	fputs("TRIVIAL CONN\n", stderr);
	return 1;
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

__attribute__((weak)) int
connd_coin(const char *UNUSED(rsp), size_t UNUSED(rsz))
{
	return 1;
}

__attribute__((weak)) int
authd_coin(const char *UNUSED(rsp), size_t UNUSED(rsz))
{
	return 1;
}

__attribute__((weak)) int
joind_coin(const char *UNUSED(rsp), size_t UNUSED(rsz))
{
	return 1;
}


/* only cb's we allow here */
static void
prepare(EV_P_ ev_prepare *w, int UNUSED(revents))
{
	wssnarf_t ctx = w->data;
	size_t idx = 0U;

more:
	fprintf(stderr, "PREP[%zu](%u)\n", idx, ctx->st[idx]);
	switch (ctx->st[idx]) {
		int r;

	case COIN_ST_UNK:
		if ((r = open_coin(EV_A_ ctx, idx)) < 0) {
			goto disc;
		}
		/* reset gbof and go for it */
		ctx->sbuf[idx].n = INI_GBOF;
		ctx->st[idx] = COIN_ST_CONN;
		/* consider this channel active */
		ctx->last_act[idx] = *tsp;
	case COIN_ST_CONN:
		/* initialise everything, sets the state */
		if ((r = conn_coin(ctx->ws[idx])) < 0) {
			goto disc;
		} else if (r == 0) {
			/* we need to wait */
			break;
		}
		/* otherwise fast propagation */
		;
	case COIN_ST_CONND:
		/* reset nothing counter and start the nothing timer */
		ctx->sbuf[idx].n = INI_GBOF;
		ctx->st[idx] = COIN_ST_AUTH;

	case COIN_ST_AUTH:
		/* let's auth */
		if (ws_proto(ctx->ws[idx]) == WS_PROTO_REST) {
			;
		} else if ((r = auth_coin(ctx->ws[idx])) < 0) {
			/* unsuccessful */
			ctx->st[idx] = COIN_ST_CONND;
			break;
		} else if (r == 0) {
			/* we need to wait */
			break;
		}
		/* otherwise fast propagation */
		;
	case COIN_ST_AUTHD:
		ctx->sbuf[idx].n = INI_GBOF;
		ctx->st[idx] = COIN_ST_JOIN;

	case COIN_ST_JOIN:
		/* let's join */
		if (ws_proto(ctx->ws[idx]) == WS_PROTO_REST) {
			;
		} else if ((r = join_coin(ctx->ws[idx])) < 0) {
			/* unsuccessful, go back to authd */
			ctx->st[idx] = COIN_ST_AUTHD;
			break;
		} else if (r == 0) {
			break;
		}
		/* otherwise we're good to go */
		ctx->st[idx] = COIN_ST_JOIND;

	case COIN_ST_JOIND:
		/* check if there's messages from the channel */
		if (ws_proto(ctx->ws[idx]) == WS_PROTO_REST) {
			break;
		} else if (tsp->tv_sec - ctx->last_act[idx].tv_sec <
		    ctx->p[idx].max_inact) {
			break;
		}
		/* fallthrough */
	case COIN_ST_NODATA:
	case COIN_ST_INTR:
	disc:
		/* disconnect and unroll */
		fini_coin(EV_A_ ctx, idx);
		ctx->st[idx] = COIN_ST_FINI;
	case COIN_ST_FINI:
		ev_break(EV_A_ EVBREAK_ALL);
		break;
	default:
		break;
	}
	if (++idx < ctx->ns) {
		/* go through all the other sockets too */
		goto more;
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

	ev_prepare_init(ctx->prep, prepare);
	ev_prepare_start(EV_A_ ctx->prep);
	ctx->prep->data = ctx;

	/* the midnight tick for file rotation, also upon sighup */
	ev_periodic_init(ctx->midnight, midnight_cb, MIDNIGHT, ONE_DAY, NULL);
	ev_periodic_start(EV_A_ ctx->midnight);
	ev_signal_init(ctx->sigh, sighup_cb, SIGHUP);
	ev_signal_start(EV_A_ ctx->sigh);
	ctx->midnight->data = ctx;
	ctx->sigh->data = ctx;
	return;
}

static void
deinit_ev(EV_P_ wssnarf_t ctx)
{
	ev_signal_stop(EV_A_ ctx->sigi);
	ev_signal_stop(EV_A_ ctx->sigp);

	ev_signal_stop(EV_A_ ctx->sigh);
	ev_periodic_stop(EV_A_ ctx->midnight);

	ev_prepare_stop(EV_A_ ctx->prep);
	return;
}


wssnarf_t
make_wssnarf(const char *logfile)
{
	struct wssnarf_s *r = calloc(1U, sizeof(*r));

	/* make sure we've got something to write to */
	r->logfile = logfile;
	r->logfd = open_outfile(r->logfile);

	/* put the hostname behind logfile */
	(void)gethostname(r->hostname, sizeof(r->hostname));
	return r;
}

void
free_wssnarf(wssnarf_t ctx)
{
	EV_P = ev_default_loop(0);
	close_sock(ctx->logfd);

	for (size_t i = 0U; i < ctx->ns; i++) {
		if (LIKELY(ctx->sbuf[i].p != NULL)) {
			free(ctx->sbuf[i].p);
		}
	}
	free(ctx);
	ev_loop_destroy(EV_A);
	return;
}

int
run_wssnarf(wssnarf_t ctx)
{
	EV_P = ev_default_loop(0);

	/* get a ref time */
	clock_gettime(CLOCK_REALTIME_COARSE, tsp);
	/* init */
	init_ev(EV_A_ ctx);
	/* work */
	ev_run(EV_A_ 0);
	/* fini */
	deinit_ev(EV_A_ ctx);
	return 0;
}

int
add_wssnarf(wssnarf_t ctx, wssnarf_param_t prm)
{
	EV_P = ev_default_loop(0);

	if (prm.max_inact <= 0) {
		prm.max_inact = 1e48;
	}
	ctx->p[ctx->ns] = prm;

	/* initialise heartbeat */
	if (LIKELY(prm.heartbeat > 0)) {
		ev_timer_init(&ctx->timer[ctx->ns], hbeat_cb, 0.0, prm.heartbeat);
		ev_timer_start(EV_A_ &ctx->timer[ctx->ns]);
		ctx->timer[ctx->ns].data = ctx;
	}
	ctx->ns++;
	return 0;
}

int
wssnarf_log(wssnarf_t ctx, const char *buf, size_t bsz)
{
	loghim(ctx->logfd, buf, bsz);
	return 0;
}

/* wssnarf.c ends here */
