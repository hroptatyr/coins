#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "fix.h"
#include "nifty.h"

#define CLCK_STR	"YYYYMMDD-HH:MM:SS.000"
#define CLCK_FMT	"%Y%m%d-%H:%M:%S.000"

static unsigned int seq = 1U;
static const char *scomp_id;
static size_t nscomp_id;
static const char *tcomp_id;
static size_t ntcomp_id;

static char clck[] = CLCK_STR;
static time_t metr;


static uint8_t
fix_chksum(const char *str, size_t len)
{
        unsigned int res = 0;
        for (const char *p = str, *ep = str + len; p < ep; res += *p++);
        return (uint8_t)(res & 0xff);
}


/* public API */
void
fix_reset(void)
{
	seq = 1U;
	return;
}

size_t
fix_render_tm(char *restrict buf, size_t UNUSED(bsz))
{
	time_t now = time(NULL);

	if (UNLIKELY(now > metr)) {
		struct tm *tm;
		tm = gmtime(&now);
		strftime(clck, sizeof(clck), CLCK_FMT, tm);
	}
	memcpy(buf, clck, strlenof(clck));
	return strlenof(clck);
}

void
fix_set_scomp_id(const char *sc, size_t sz)
{
	scomp_id = sc;
	nscomp_id = sz;
	return;
}

void
fix_set_tcomp_id(const char *tc, size_t tz)
{
	tcomp_id = tc;
	ntcomp_id = tz;
	return;
}

size_t
fix_render(char *restrict buf, size_t bsz, fix_msg_t msg)
{
	static const char fhdr[] = "8=FIX.4.4" SOH "9=XXX" SOH;
	size_t len = 0U;

	memcpy(buf, fhdr, len = strlenof(fhdr));

	/* print message type */
	buf[len++] = '3', buf[len++] = '5', buf[len++] = '=';
	buf[len++] = msg.typ[0U];
	if (LIKELY(!(buf[len++] = msg.typ[1U]))) {
		len--;
	}
	buf[len++] = *SOH;

	/* print sequence number */
	buf[len++] = '3', buf[len++] = '4', buf[len++] = '=';
	with (size_t prev = len) {
		for (unsigned int s = seq++; s;
		     buf[len++] = (char)((s % 10U) ^ '0'), s /= 10U);
		for (size_t i = 0U; i < (len - prev) / 2U; i++) {
			char tmp = buf[prev + i];
			buf[prev + i] = buf[len - (i + 1U)];
			buf[len - (i + 1U)] = tmp;
		}
	}
	buf[len++] = *SOH;

	/* print our comp id */
	if (LIKELY(nscomp_id)) {
		buf[len++] = '4', buf[len++] = '9', buf[len++] = '=';
		memcpy(buf + len, scomp_id, nscomp_id);
		len += nscomp_id;
		buf[len++] = *SOH;
	}

	/* print time */
	buf[len++] = '5', buf[len++] = '2', buf[len++] = '=';
	len += fix_render_tm(buf + len, bsz - len);
	buf[len++] = *SOH;

	/* print target comp id */
	buf[len++] = '5', buf[len++] = '6', buf[len++] = '=';
	memcpy(buf + len, tcomp_id, ntcomp_id);
	len += ntcomp_id;
	buf[len++] = *SOH;

	/* copy user message */
	for (size_t i = 0U; i < msg.len; i++) {
		buf[len++] = (char)(msg.msg[i] ? msg.msg[i] : *SOH);
	}

	with (size_t msz = len - strlenof(fhdr)) {
		buf[strlenof(fhdr) - 2U] = (char)((msz % 10U) ^ '0');
		msz /= 10U;
		buf[strlenof(fhdr) - 3U] = (char)((msz % 10U) ^ '0');
		msz /= 10U;
		buf[strlenof(fhdr) - 4U] = (char)((msz % 10U) ^ '0');
	}

	/* print chksum */
	with (uint8_t ck = fix_chksum(buf, len)) {
		buf[len++] = '1', buf[len++] = '0', buf[len++] = '=';
		buf[len + 2] = (char)((ck % 10U) ^ '0');
		ck /= 10U;
		buf[len + 1] = (char)((ck % 10U) ^ '0');
		ck /= 10U;
		buf[len + 0] = (char)((ck % 10U) ^ '0');
		len += 3U;
		buf[len++] = *SOH;
	}

	buf[len + 0U] = '\n';
	buf[len + 1U] = '\0';
	return len;
}

fix_msg_t
fix_parse(char *restrict buf, size_t bsz)
{
	static const char fhdr[] = "8=FIX.4.4" SOH "9=";
	fix_msg_t res;
	size_t j;

	if (UNLIKELY(bsz < strlenof(fhdr))) {
		goto buggered;
	} else if (UNLIKELY(memcmp(buf, fhdr, strlenof(fhdr)))) {
		/* we want FIX.4.4 */
		goto buggered;
	}
	/* look for that 35 tag (msg type),
	 * we only scan messages whose lengths are 5 digits max */
	if (j = sizeof(fhdr),
	    buf[j++] != *SOH &&
	    buf[j++] != *SOH &&
	    buf[j++] != *SOH &&
	    buf[j++] != *SOH &&
	    buf[j++] != *SOH) {
		goto buggered;
	} else if (buf[j++] != '3' || buf[j++] != '5' || buf[j++] != '=') {
		goto buggered;
	}
	/* now then, snarf message type */
	res.typ[0U] = buf[j++];
	res.typ[1U] = (uint8_t)(buf[j] == *SOH ? '\0' : buf[j++]);
	res.typ[2U] = '\0', res.typ[3U] = '\0';

	/* keep leading and trailing SOH */
	res.msg = buf + j;
	res.len = bsz - j;

	/* NUL'ify instead of SOH */
	for (size_t i = 0U; i < bsz; i++) {
		buf[i] = (char)(buf[i] != *SOH ? buf[i] : *NUL);
	}
	return res;

buggered:
	return (fix_msg_t){};
}

/* fix.c ends here */
