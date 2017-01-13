#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
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

static inline size_t
memncpy(char *restrict tgt, const char *src, size_t zrc)
{
	(void)memcpy(tgt, src, zrc);
	return zrc;
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
route_quote(fix_msg_t msg, const char *prfx, size_t prfz, bool rstp)
{
/* extract bid, ask and instrument and send to mcast channel */
	static const char tim[] = NUL "52=";
	static const char ins[] = NUL "55=";
	static const char ccy[] = NUL "15=";
	static const char nup[] = NUL "268=";
	static const char ety[] = NUL "269=";
	static const char prc[] = NUL "270=";
	static const char qty[] = NUL "271=";
	const char *on, *const eom = msg.msg + msg.len;
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

	if (UNLIKELY(!(on = xmemmem(msg.msg, msg.len, ins, strlenof(ins))))) {
		return -1;
	}
	len += memncpy(buf + len, on, strlen(on));
	if ((on = xmemmem(msg.msg, msg.len, ccy, strlenof(ccy)))) {
		/* they encode the contract in field 15 */
		buf[len++] = '/';
		len += memncpy(buf + len, on, strlen(on));
	}
	buf[len++] = '\t';

	/* look for updates */
	if (UNLIKELY(!(on = xmemmem(msg.msg, msg.len, nup, strlenof(nup))))) {
		/* no updates? */
		return -1;
	}
	if (UNLIKELY(rstp)) {
		/* indicate reset */
		static const char clr[] = "clr0\t\t\n";
		size_t z = len;

		z += memncpy(buf + z, clr, strlenof(clr));
		fwrite(buf, 1, z, stdout);
	}
	while ((on = xmemmem(on, eom - on, ety, strlenof(ety)))) {
		static const char *f[] = {"BID2", "ASK2"};
		unsigned char sd = (unsigned char)(*on++ ^ '0');
		size_t z = len;

		if (UNLIKELY(sd >= countof(f))) {
			continue;
		}
		/* copy side */
		z += memncpy(buf + z, f[sd], 4U);
		buf[z++] = '\t';
		/* on should now be 270=... */
		if (UNLIKELY(memcmp(on, prc, strlenof(prc)))) {
			continue;
		}
		on += strlenof(prc);
		/* on points to the price level */
		z += memncpy(buf + z, on, strlen(on));
		/* try and find 271= within 2 fields */
		if ((on += strlen(on), memcmp(on, qty, strlenof(qty))) &&
		    /* try again */
		    (on += strlen(++on), memcmp(on, qty, strlenof(qty)))) {
			continue;
		}
		/* yay */
		on += strlenof(qty);
		buf[z++] = '\t';
		z += memncpy(buf + z, on, strlen(on));
		buf[z++] = '\n';
		fwrite(buf, 1, z, stdout);
	}
	return 0;
}

static int
procln(char *ln, size_t lz)
{
	fix_msg_t msg;
	size_t prfz;
	bool rstp = false;

	with (const char *prfx = memchr(ln, '\t', lz)) {
		prfz = prfx ? prfx + 1U - ln : 0U;
	}
	msg = fix_parse(ln + prfz, lz - prfz);

	switch (UINTIFY_TYP(msg.typ)) {
	case UINTIFY_TYP("W"):	/* full refresh */
		rstp = true;
	case UINTIFY_TYP("X"):	/* inc refresh */
		route_quote(msg, ln, prfz, rstp);
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


#include "ok2b.yucc"

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

/* ok2b.c ends here */
