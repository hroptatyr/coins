#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <ev.h>
#include <assert.h>
#include "nifty.h"
#include "fix.h"
#include "tls.h"

#define API_PORT	9880

#define ONE_DAY		86400.0
#define MIDNIGHT	0.0

typedef enum {
	BTCC_STATE_UNK,
	BTCC_STATE_ON,
	BTCC_STATE_SUB,
	BTCC_STATE_OFF,
} btcc_state_t;

typedef enum {
	SPOT,
	PRO,
	SPOT_USD,
} btcc_srv_t;

typedef enum {
	BTCCNY,
	LTCCNY,
	LTCBTC,
	XBTCNY,
	BPICNY,
	BTCUSD,
} btcc_ins_t;

static const btcc_srv_t srvs[] = {
	[BTCCNY] = SPOT,
	[LTCCNY] = SPOT,
	[LTCBTC] = SPOT,
	[XBTCNY] = PRO,
	[BPICNY] = PRO,
	[BTCUSD] = SPOT_USD,
};

static const char *const tcomps[] = {
	[SPOT] = "BTCC-FIX-SERVER",
	[PRO] = "BTCC-PRO-EXCHANGE-SERVER",
	[SPOT_USD] = "BTCC",
};

static const char *const hosts[] = {
	[SPOT] = "fix.btcc.com",
	[PRO] = "pro-fix.btcc.com",
	[SPOT_USD] = "spotusd-fix.btcc.com",
};


static __attribute__((format(printf, 1, 2))) void
serror(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

static char*
xmemmem(const char *hay, const size_t hayz, const char *ndl, const size_t ndlz)
{
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
		return deconst(hay);
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
			return deconst(cand);
		}
	}
	return NULL;
}

static inline size_t
hrclock_print(char *buf, size_t __attribute__((unused)) len)
{
	struct timespec tsp[1];
	clock_gettime(CLOCK_REALTIME_COARSE, tsp);
	return sprintf(buf, "%ld.%09li", tsp->tv_sec, tsp->tv_nsec);
}


static btcc_ins_t ins;
static btcc_srv_t srv;
static char logfile[] = "xxxxxx";
static btcc_state_t st;
static ssl_ctx_t sc;
static unsigned int susp;
static char hostname[256];
static size_t hostnsz;
static int logfd;

static void
buf_print(const char *buf, ssize_t bsz)
{
	char out[16384U];
	size_t sz;

	if (UNLIKELY(bsz < 0)) {
		return;
	}
	/* this is a prefix that we prepend to each line */
	sz = hrclock_print(out, sizeof(out));
	out[sz++] = '\t';

	for (ssize_t i = 0; i < bsz; i++, sz++) {
		out[sz] = (char)(buf[i] != *SOH ? buf[i] : '|');
	}
	out[sz++] = '\n';

	write(logfd, out, sz);
	write(STDOUT_FILENO, out, sz);
	return;
}

static void
open_outfile(void)
{
	if ((logfd = open(logfile, O_WRONLY | O_CREAT, 0644)) < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	/* ffw to the end */
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
	strncpy(n, logfile, strlenof(logfile));
	n += strlenof(logfile);
	*n++ = '-';
	strncpy(n, hostname, hostnsz);
	n += hostnsz;
	*n++ = '-';
	strftime(n, sizeof(new) - strlenof(logfile) - hostnsz,
		 "%Y-%m-%dT%H:%M:%S\0", tm);

	fprintf(stderr, "new \"%s\"\n", new);

	/* close the old file */
	buf_print(msg, strlenof(msg));
	fdatasync(logfd);
	close(logfd);
	/* rename it and reopen under the old name */
	rename(logfile, new);
	open_outfile();
	return;
}


static int
fix_hello(void)
{
	static const char helo[] = "98=0" SOH "108=30" SOH "141=Y" SOH;
	char buf[1536U];
	size_t len;
	ssize_t nwr;

	fix_reset();
	len = fix_render(buf, sizeof(buf),
			 (fix_msg_t){'A', strlenof(helo), helo});

	if (UNLIKELY(sc == NULL)) {
		;
	} else if (UNLIKELY((nwr = tls_send(sc, buf, len, 0)) <= 0)) {
		errno = 0, serror("\
Error: ssl write %d", tls_errno(sc, nwr));
	}
	buf_print(buf, len);
	return 0;
}

static int
fix_gdbye(void)
{
	char buf[1536U];
	size_t len;
	ssize_t nwr;

	len = fix_render(buf, sizeof(buf), (fix_msg_t){'5'});

	if (UNLIKELY(sc == NULL)) {
		;
	} else if (UNLIKELY((nwr = tls_send(sc, buf, len, 0)) <= 0)) {
		errno = 0, serror("\
Error: ssl write %d", tls_errno(sc, nwr));
	}
	buf_print(buf, len);
	return 0;
}

static int
fix_subsc(void)
{
	static char mreq[] =
		"262=mreq" SOH "263=1" SOH		\
		"264=100" SOH "265=1" SOH "267=3" SOH	\
		"269=0" SOH "269=1" SOH "269=2" SOH	\
		"146=1" SOH "55=XBTCNY" SOH;
	char buf[256U];
	size_t len;
	int nwr;

	/* make use of the fact that logfile is named after INS */
	memcpy(mreq + strlenof(mreq) - 7U, logfile, strlenof(logfile));

	len = fix_render(buf, sizeof(buf),
			 (fix_msg_t){'V', strlenof(mreq), mreq});

	if (UNLIKELY(sc == NULL)) {
		;
	} else if (UNLIKELY((nwr = tls_send(sc, buf, len, 0)) <= 0)) {
		errno = 0, serror("\
Error: ssl write %d", tls_errno(sc, nwr));
	}
	buf_print(buf, len);
	return 0;
}

static int
fix_hbeat(void)
{
	char buf[1536U];
	size_t len;
	int nwr;

	len = fix_render(buf, sizeof(buf), (fix_msg_t){'0'});

	if (UNLIKELY(sc == NULL)) {
		;
	} else if (UNLIKELY((nwr = tls_send(sc, buf, len, 0)) <= 0)) {
		errno = 0, serror("\
Error: ssl write %d", tls_errno(sc, nwr));
	}
	buf_print(buf, len);
	return 0;
}

static bool
fix_weekend_p(fix_msg_t m)
{
	static const char rsn[] =
		NUL "58=Logon attempt not within session time";

	if (xmemmem(m.msg, m.len, rsn, sizeof(rsn)) == NULL) {
		return false;
	}
	return true;
}


static void
beef_cb(EV_P_ ev_io *UNUSED(w), int UNUSED(revents))
{
	char buf[16384U];
	fix_msg_t msg;
	ssize_t nrd;
	int e;

	(void)EV_A;
	assert(sc);
	nrd = tls_recv(sc, buf, sizeof(buf), 0);

	switch ((e = tls_errno(sc, nrd))) {
	case SSL_ERROR_NONE:
		/* success */
		break;
	case SSL_ERROR_SYSCALL:
		serror("\
Connection reset");
		goto unroll;
	case SSL_ERROR_ZERO_RETURN:
		errno = 0, serror("\
Connection reset: %d", e);
		goto unroll;
	default:
		errno = 0, serror("\
Error: ssl error %d", e);
		goto unroll;
	}

	buf_print(buf, nrd);
	msg = fix_parse(buf, nrd);

	switch (UINTIFY_TYP(msg.typ)) {
	case UINTIFY_TYP('W'):
	case UINTIFY_TYP('X'):
		switch (st) {
		case BTCC_STATE_SUB:
			break;
		default:
			errno = 0, serror("\
Warning: quote message received while nothing's been subscribed");
			break;
		}
		break;

	case UINTIFY_TYP('A'):
		switch (st) {
		case BTCC_STATE_ON:
		case BTCC_STATE_SUB:
			errno = 0, serror("\
Warning: logon message received while logged on already");
		default:
			break;
		}
		/* logon */
		st = BTCC_STATE_ON;
		/* subscribe to everyone */
		fix_subsc();
		st = BTCC_STATE_SUB;
		break;
	case UINTIFY_TYP('5'):
		/* logout, set state accordingly */
		st = BTCC_STATE_OFF;
		errno = 0, serror("\
Notice: clocking off");
		susp = 0U;
		/* check for this session string */
		if (UNLIKELY(fix_weekend_p(msg))) {
			errno = 0, serror("\
Notice: sleeping till full hour");
			susp = 60U;
		}
		break;

	case UINTIFY_TYP('0'):
		/* heartbeat, mute him */
		break;

	case 0U:
		/* message buggered */
		errno = 0, serror("\
Warning: message buggered");
		break;
	default:
		/* message not supported */
		errno = 0, serror("\
Warning: unsupported message %c", msg.typ);
		break;
	}
	return;

unroll:
	st = BTCC_STATE_UNK;
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sigint_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	fix_gdbye();
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	st = BTCC_STATE_UNK;
	return;
}

static void
hbeat_cb(EV_P_ ev_timer *UNUSED(w), int UNUSED(revents))
{
	(void)EV_A;
	fix_hbeat();
	return;
}

static void
midnight_cb(EV_P_ ev_periodic *UNUSED(w), int UNUSED(r))
{
	(void)EV_A;
	rotate_outfile();
	return;
}


#include "btcc.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	static char scomp[] = "coins_btcc_xxxxxx";
	EV_P = ev_default_loop(EVFLAG_AUTO);
	ev_signal sigint_watcher[1U];
	ev_signal sigterm_watcher[1U];
	ev_periodic midnight[1];
	ev_timer hbeat[1U];
	ev_io beef[1U];
	int rc = EXIT_SUCCESS;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = EXIT_FAILURE;
		goto out;
	}

	/* try and get the instrument in question */
	if (!argi->nargs) {
		errno = 0, serror("\
Error: no instrument specified, must be one of BTCCNY, LTCCNY, LTCBTC, XBTCNY");
		rc = EXIT_FAILURE;
		goto out;
	} else if (!memcmp(*argi->args, "BTCCNY", 6U)) {
		ins = BTCCNY;
	} else if (!memcmp(*argi->args, "LTCCNY", 6U)) {
		ins = LTCCNY;
	} else if (!memcmp(*argi->args, "LTCBTC", 6U)) {
		ins = LTCBTC;
	} else if (!memcmp(*argi->args, "XBTCNY", 6U)) {
		ins = XBTCNY;
	} else if (!memcmp(*argi->args, "BPICNY", 6U)) {
		ins = BPICNY;
	} else if (!memcmp(*argi->args, "BTCUSD", 6U)) {
		ins = BTCUSD;
	} else {
		errno = 0, serror("\
Error: unknown instrument `%s'", *argi->args);
		rc = EXIT_FAILURE;
		goto out;
	}
	/* name the logfile after the instrument */
	memcpy(logfile, *argi->args, strlenof(logfile));
	/* determine server */
	srv = srvs[ins];

	/* put the hostname behind logfile */
	(void)gethostname(hostname, sizeof(hostname));
	hostnsz = strlen(hostname);

	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	ev_signal_init(sigterm_watcher, sigint_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);

	/* start the heartbeat timer */
	ev_timer_init(hbeat, hbeat_cb, 30.0, 30.0);
	ev_timer_start(EV_A_ hbeat);

	/* the midnight tick for file rotation, also upon sighup */
	ev_periodic_init(midnight, midnight_cb, MIDNIGHT, ONE_DAY, NULL);
	ev_periodic_start(EV_A_ midnight);

	/* make ourselves known */
	memcpy(scomp + strlenof(scomp) - 6U, *argi->args, 6U);
	fix_set_scomp_id(scomp, strlenof(scomp));
	fix_set_tcomp_id(tcomps[srv], strlen(tcomps[srv]));

	/* open our outfile */
	open_outfile();

	do {
		if ((sc = open_tls(hosts[srv], API_PORT)) == NULL) {
			serror("\
Error: cannot connect");
			rc = 1;
			break;
		}

		/* yay, hook the guy into our event loop */
		ev_io_init(beef, beef_cb, tls_fd(sc), EV_READ);
		ev_io_start(EV_A_ beef);

		fix_hello();

		/* start the main loop */
		ev_run(EV_A_ 0);

		/* ready to shut everything down are we? */
		ev_io_stop(EV_A_ beef);
		close_tls(sc);
		sc = NULL;

		sleep(susp);
	} while (st);

	ev_signal_stop(EV_A_ sigint_watcher);
	ev_signal_stop(EV_A_ sigterm_watcher);
	ev_timer_stop(EV_A_ hbeat);
	ev_periodic_stop(EV_A_ midnight);

	close(logfd);

out:
	/* destroy the default evloop */
	ev_default_destroy();
	yuck_free(argi);
	return rc;
}

/* btcc.c ends here */
