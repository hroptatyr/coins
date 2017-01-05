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
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <ev.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include "../deribit_cred.h"
#include "nifty.h"
#include "fix.h"

#define API_HOST	"deribit.com"
#define API_PORT	9880

#define TCOMP_ID	"DERIBITSERVER"
#define SCOMP_ID	"deribitclient"

#define ONE_DAY		86400.0
#define MIDNIGHT	0.0

typedef enum {
	OKUM_STATE_UNK,
	OKUM_STATE_ON,
	OKUM_STATE_SUB,
	OKUM_STATE_OFF,
} okum_state_t;


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

static size_t
sha256(char *restrict buf, size_t UNUSED(bsz), const char *msg, size_t len)
{
	unsigned char *sig = SHA256((const unsigned char*)msg, len, NULL);

	/* base64 him */
	return EVP_EncodeBlock((unsigned char*)buf, sig, 256U / 8U);
}

static int
put_sockaddr(struct sockaddr_in *sa, const char *name, uint16_t port)
{
	struct addrinfo hints;
	struct addrinfo *res;

	memset(sa, 0, sizeof(*sa));
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(name, NULL, &hints, &res)) {
		return -1;
	}
	*sa = *(struct sockaddr_in*)res->ai_addr;
	sa->sin_port = htons(port);
	freeaddrinfo(res);
	return 0;
}

static int
fix_open(const char *host, short unsigned int port)
{
	struct sockaddr_in sa;
	int s;

	if (UNLIKELY(put_sockaddr(&sa, host, port) < 0)) {
		goto nil;
	}
	if (UNLIKELY((s = socket(PF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)) {
		goto nil;
	}

	if (UNLIKELY(connect(s, (void*)&sa, sizeof(sa)) < 0)) {
		goto clo;
	}
	return s;

clo:
	close(s);
nil:
	return -1;
}


static const char logfile[] = "prices";
static okum_state_t st;
static int s;
static size_t ninstr;
static char *const *instr;
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
#define NONCE	"8RD8SxJxUe12c4BY/jGMUsyrOofEN6J1" API_SEC
#define NNB64	"OFJEOFN4SnhVZTEyYzRCWS9qR01Vc3lyT29mRU42SjE="
	static char helo[] = "108=30" SOH
		"96=" NNB64 SOH
		"553=" API_KEY SOH
		"554=" "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" SOH;
	char buf[1536U];
	size_t len;
	ssize_t nwr;
	size_t z;

	z = strlenof(helo) - 48U;
	z += sha256(helo + z, strlenof(helo) - z, NONCE, strlenof(NONCE));
	helo[z++] = *SOH;

	fix_reset();
	len = fix_render(buf, sizeof(buf),
			 (fix_msg_t){"A", z, helo});

	if (UNLIKELY((nwr = send(s, buf, len, 0)) <= 0)) {
		serror("\
Error: write %d", s);
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

	len = fix_render(buf, sizeof(buf), (fix_msg_t){"5"});

	if (UNLIKELY((nwr = send(s, buf, len, 0)) <= 0)) {
		serror("\
Error: write %d", s);
	}
	buf_print(buf, len);
	return 0;
}

static int
fix_subsc(const char *sym)
{
#define MREQ	"262=mreq" SOH "263=1" SOH		\
		"264=0" SOH "265=1" SOH			\
		"267=3" SOH "269=0" SOH "269=1" SOH	\
		"146=1" SOH "55=BTC/USD" SOH
	static char mreq[256U] = MREQ;
	char buf[1536U];
	size_t len;
	int nwr;
	const size_t ssz = strlen(sym);

	/* request quotes first */
	memcpy(mreq + strlenof(MREQ) - 8U, sym, ssz);
	len = strlenof(MREQ) - 8U + ssz;
	mreq[len++] = *SOH;

	len = fix_render(buf, sizeof(buf), (fix_msg_t){"V", len, mreq});

	if (UNLIKELY((nwr = send(s, buf, len, 0)) <= 0)) {
		serror("\
Error: write %d", s);
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

	len = fix_render(buf, sizeof(buf), (fix_msg_t){"0"});

	if (UNLIKELY((nwr = send(s, buf, len, 0)) <= 0)) {
		serror("\
Error: write %d", s);
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

	(void)EV_A;
	if ((nrd = recv(s, buf, sizeof(buf), 0)) <= 0) {
		serror("\
Error: read %d", s);
		goto unroll;
	}

	buf_print(buf, nrd);
	msg = fix_parse(buf, nrd);

	switch (UINTIFY_TYP(msg.typ)) {
	case UINTIFY_TYP("W"):
	case UINTIFY_TYP("X"):
		switch (st) {
		case OKUM_STATE_SUB:
			break;
		default:
			errno = 0, serror("\
Warning: quote message received while nothing's been subscribed");
			break;
		}
		break;

	case UINTIFY_TYP("A"):
		switch (st) {
		case OKUM_STATE_ON:
		case OKUM_STATE_SUB:
			errno = 0, serror("\
Warning: logon message received while logged on already");
		default:
			break;
		}
		/* logon */
		st = OKUM_STATE_ON;
		/* subscribe to everyone */
		for (size_t i = 0U; i < ninstr; i++) {
			fix_subsc(instr[i]);
			st = OKUM_STATE_SUB;
		}
		break;
	case UINTIFY_TYP("5"):
		/* logout, set state accordingly */
		st = OKUM_STATE_OFF;
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

	case 0U:
		/* message buggered */
		errno = 0, serror("\
Warning: message buggered");
		break;
	default:
		/* message not supported */
		errno = 0, serror("\
Warning: unsupported message %s", msg.typ);
		break;
	}
	return;

unroll:
	st = OKUM_STATE_UNK;
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sigint_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	fix_gdbye();
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	st = OKUM_STATE_UNK;
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


#include "deribit.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	EV_P = ev_default_loop(EVFLAG_AUTO);
	ev_signal sigint_watcher[1U];
	ev_signal sigterm_watcher[1U];
	ev_periodic midnight[1];
	ev_timer hbeat[1U];
	ev_io beef[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = 1;
		goto out;
	}

	ninstr = argi->nargs;
	instr = argi->args;

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
	fix_set_scomp_id(SCOMP_ID, strlenof(SCOMP_ID));
	fix_set_tcomp_id(TCOMP_ID, strlenof(TCOMP_ID));

	/* open our outfile */
	open_outfile();

	do {
		if ((s = fix_open(API_HOST, API_PORT)) < 0) {
			serror("\
Error: cannot connect");
			rc = 1;
			break;
		}

		/* yay, hook the guy into our event loop */
		ev_io_init(beef, beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef);

		fix_hello();

		/* start the main loop */
		ev_run(EV_A_ 0);

		/* ready to shut everything down are we? */
		ev_io_stop(EV_A_ beef);
		close(s);

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

/* derifix.c ends here */
