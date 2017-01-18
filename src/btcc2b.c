#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include "fix.h"
#include "nifty.h"

#define NSECS	(1000000000)
#define MSECS	(1000)

typedef long unsigned int tv_t;
#define NOT_A_TIME	((tv_t)-1ULL)


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
/* point to NDLZ octets after NDL in HAY */
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


static int
route_quote(fix_msg_t msg, const char *prfx, size_t prfz)
{
/* extract bid, ask and instrument and send to mcast channel */
	static const char tim[] = NUL "52=";
	static const char ins[] = NUL "55=";
	static const char nup[] = NUL "268=";
	static const char ety[] = NUL "269=";
	static const char prc[] = NUL "270=";
	static const char qty[] = NUL "271=";
	const char *on, *const eom = msg.msg + msg.len;
	const char *in;
	size_t inz;
	char buf[256U];
	size_t len = 0U;

	if (UNLIKELY(!(on = xmemmem(msg.msg, msg.len, tim, strlenof(tim))))) {
		return -1;
	}
	with (tv_t t) {
		struct tm tm[1U];

		if (UNLIKELY((on = strptime(on, "%Y%m%d-%T", tm)) == NULL)) {
			return -1;
		}
		t = timegm(tm);
		t *= NSECS;
		t += strtons(on, NULL);
		len += tvtostr(buf + len, sizeof(buf), t);
	}
	buf[len++] = '\t';

	/* put the prefix in */
	if (LIKELY(prfz)) {
		len += memncpy(buf + len, prfx, prfz);
	}

	/* make sure we've got at least one ins */
	if (UNLIKELY(!(on = xmemmem(msg.msg, msg.len, ins, strlenof(ins))))) {
		/* no instrument at all */
		return -1;
	}
	in = on, inz = strlen(on);

	/* look for updates */
	if (UNLIKELY(!(on = xmemmem(msg.msg, msg.len, nup, strlenof(nup))))) {
		/* no updates? */
		return -1;
	}
	while ((on = xmemmem(on, eom - on, ety, strlenof(ety)))) {
		static const char *sides[] = {"BID", "ASK", "TRA"};
		unsigned int sd, l;
		size_t onz;
		size_t z = len;

		/* stash side, copy to buf later */
		sd = *on++ ^ '0';
		if (UNLIKELY(sd >= countof(sides))) {
			continue;
		}

		/* on should now be 55=... */
		if (LIKELY(!memcmp(on, ins, strlenof(ins)))) {
			/* got a quote instrument */
			in = on += strlenof(ins), inz = strlen(on);
			on += inz;
		}
		/* on points to the instrument identifier */
		z += memncpy(buf + z, in, inz);

		buf[z++] = '\t';
		z += memncpy(buf + z, sides[sd], 3U);
		/* guess the actual flavour,
		 * only XBTCNY and BTCUSD have real depth data,
		 * the rest seems to be top level */
		l = sd < 2;
		if ((in[0U] == 'X' || in[3U] == 'U') && l) {
			l = 2U + (UINTIFY_TYP(msg.typ) == UINTIFY_TYP("X"));
		}
		/* print level */
		buf[z++] = (char)(l ^ '0');

		/* on should now be 270=... */
		if (UNLIKELY(memcmp(on, prc, strlenof(prc)))) {
			/* no price */
			continue;
		}
		on += strlenof(prc);
		buf[z++] = '\t';
		/* on points to the price level */
		z += memncpy(buf + z, on, onz = strlen(on));
		on += onz;

		/* on should now be 271 */
		if (LIKELY(!memcmp(on, qty, strlenof(qty)))) {
			/* yay, got a quantity */
			on += strlenof(qty);
			buf[z++] = '\t';
			z += memncpy(buf + z, on, strlen(on));
		}
		buf[z++] = '\n';
		fwrite(buf, 1, z, stdout);
	}
	return 0;
}

static int
procln(char *ln, size_t lz)
{
	size_t prfz;
	fix_msg_t msg;

	with (const char *prfx = memchr(ln, '\t', lz)) {
		prfz = prfx ? prfx + 1U - ln : 0U;
	}
	msg = fix_parse(ln + prfz, lz - prfz);

	switch (UINTIFY_TYP(msg.typ)) {
	case UINTIFY_TYP("W"):	/* full refresh */
	case UINTIFY_TYP("X"):	/* inc refresh */
		route_quote(msg, ln, prfz);
		break;

	case UINTIFY_TYP("A"):
	case UINTIFY_TYP("5"):
	case UINTIFY_TYP("V"):
	case UINTIFY_TYP("0"):
		/* stuff we won't deal with in offline mode */
		break;

	case 0U:
		/* message buggered */
		errno = 0, serror("\
Warning: message buggered");
		fwrite(ln, 1, lz, stderr);
		break;
	default:
		/* message not supported */
		errno = 0, serror("\
Warning: unsupported message %s", msg.typ);
		break;
	}
	return 0;
}


#include "btcc2b.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = 1;
		goto out;
	}

	{
		char *line = NULL;
		size_t llen = 0UL;
		ssize_t nrd;

		while ((nrd = getline(&line, &llen, stdin)) > 0) {
			procln(line, nrd);
		}

		free(line);
	}

out:
	yuck_free(argi);
	return rc;
}

/* btcc2b.c ends here */
