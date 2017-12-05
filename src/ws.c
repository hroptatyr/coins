#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "tls.h"
#include "boobs.h"
#include "ws.h"
#include "nifty.h"

typedef struct  __attribute__((packed)) {
	uint8_t code:4;
	uint8_t rsv3:1;
	uint8_t rsv2:1;
	uint8_t rsv1:1;
	uint8_t finp:1;

	uint8_t plen:7;
	uint8_t mask:1;

	uint16_t plen16;
	uint16_t plen16_1;
	uint16_t plen16_2;
	uint16_t plen16_3;
	uint32_t mkey;
} wsfr_t;

_Static_assert(sizeof(wsfr_t) == 14, "wsfr_t is improperly packed");

struct ws_s {
	/* protocol */
	ws_proto_t p;
	int s;
	ssl_ctx_t c;
	/* size to go */
	ssize_t togo;
	unsigned int state;
	union {
		/* frame leftovers */
		wsfr_t frml;
		struct {
			const char *host;
			size_t hstz;
		};
	};
};


static char *gbuf;
static size_t gbsz;
/* reference count */
static size_t gbrc;

static char*
xmemmem(const char *hay, const size_t hayz, const char *ndl, const size_t ndlz)
{
/* the one that points NDLZ behind HAY */
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

static ws_t
_open(const char *host, short unsigned int port, int ssl)
{
	struct sockaddr_in sa;
	ssl_ctx_t c = NULL;
	ws_t this;
	int s;

	if (UNLIKELY(put_sockaddr(&sa, host, port) < 0)) {
		goto nil;
	}
	if (UNLIKELY((s = socket(PF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)) {
		goto nil;
	}

	if (UNLIKELY((this = malloc(sizeof(*this))) == NULL)) {
		goto clo;
	}

	if (UNLIKELY(connect(s, (void*)&sa, sizeof(sa)) < 0)) {
		goto fre;
	}
	if (ssl && UNLIKELY((c = conn_tls(s)) == NULL)) {
		goto fre;
	}
	*this = (struct ws_s){.s = s, .c = c};
	return this;

fre:
	free(this);
clo:
	close(s);
nil:
	return NULL;
}

static ssize_t
_sioi(char *restrict buf, size_t bsz, ws_t ws, const char *host, const char *rsrc)
{
	static const char verb[] = "GET ";
	static const char rsrc1[] = "\
socket.io/1/ HTTP/1.1\r\n\
Host: ";
	static const char rsrc3[] = "\
socket.io/?EIO=3&transport=polling HTTP/1.1\r\n\
Host: ";
	static const char req[] = "\
User-Agent: Dark Ninja Secret/1.0\r\n\
Accept: */*\r\n\
\r\n\
";
	char sidbuf[64U];
	const char *sid;
	size_t siz;
	size_t nrq = 0U;
	ssize_t nrd;

	nrq += memncpy(gbuf + nrq, verb, strlenof(verb));
	if (rsrc[0U] != '/') {
		gbuf[nrq++] = '/';
	}
	nrq += memncpy(gbuf + nrq, rsrc, strlen(rsrc));
	if (gbuf[nrq - 1U] != '/') {
		gbuf[nrq++] = '/';
	}

	switch (ws->p) {
	case WS_PROTO_SIO1:
		nrq += memncpy(gbuf + nrq, rsrc1, strlenof(rsrc1));
		break;
	case WS_PROTO_SIO3:
		nrq += memncpy(gbuf + nrq, rsrc3, strlenof(rsrc3));
		break;
	}
	nrq += memncpy(gbuf + nrq, host, strlen(host));
	gbuf[nrq++] = '\r';
	gbuf[nrq++] = '\n';
	nrq += memncpy(gbuf + nrq, req, strlenof(req));

	fwrite(gbuf, 1, nrq, stderr);
	if (ws->c) {
		tls_send(ws->c, gbuf, nrq, 0);
	} else {
		send(ws->s, gbuf, nrq, 0);
	}

	const char *eoh;
	do {
		if (ws->c) {
			nrd = tls_recv(ws->c, gbuf, gbsz, 0);
		} else {
			nrd = recv(ws->s, gbuf, gbsz, 0);
		}
	} while (nrd > 0 && (eoh = xmemmem(gbuf, nrd, "\r\n\r\n", 4U)) == NULL);
chk:
	if (nrd <= 0 || (nrd -= eoh - gbuf) < 0) {
		return -1;
	} else if (nrd == 0) {
		/* just one more read */
		if (ws->c) {
			nrd = tls_recv(ws->c, gbuf, gbsz, 0);
		} else {
			nrd = recv(ws->s, gbuf, gbsz, 0);
		}
		eoh = gbuf;
		goto chk;
	}

	/* search for the sid */
	switch (ws->p) {
	case WS_PROTO_SIO1:
		/* sio1 simply prints the sid and other stuff sep'd by : */
		with (const char *eos = memchr(eoh, ':', nrd)) {
			if (UNLIKELY(eos == NULL)) {
				sid = NULL;
				siz = 0UL;
				break;
			}
			/* otherwise go back to the beginning of the buffer
			 * or the beginning of the line (for chunked xfers) */
			for (sid = eos; sid > eoh && sid[-1] != '\n'; sid--);
			siz = eos - sid;
		}
		break;
	case WS_PROTO_SIO3:
		/* sio3 gives us json */
		if ((sid = xmemmem(eoh, nrd, "\"sid\"", 5U))) {
			const char *const eob = eoh + nrd;
			const char *eos;

			/* fast forward to : */
			while (sid < eob && *sid++ != ':');
			/* more */
			while (sid < eob && *sid++ != '"');
			for (eos = sid; eos < eob && *eos != '"'; eos++);
			if (UNLIKELY(!(siz = eos - sid))) {
				sid = NULL;
			}
		}
		break;
	}
	if (UNLIKELY(sid == NULL)) {
		fputs("Error: cannot find session id\n", stderr);
		return -1;
	} else if (UNLIKELY(siz >= sizeof(sidbuf) || siz >= bsz)) {
		fputs("Error: session id exceeds buffer\n", stderr);
		return -1;
	}
	memcpy(sidbuf, sid, siz);

	/* massage for output */
	sidbuf[siz] = '\n';
	fputs("session-id: ", stderr);
	fwrite(sidbuf, 1, siz + 1U, stderr);
	sidbuf[siz] = '\0';

	/* prep resource return value */
	nrq = memncpy(gbuf, rsrc, strlen(rsrc));
	if (gbuf[nrq - 1U] != '/') {
		gbuf[nrq++] = '/';
	}
	switch (ws->p) {
		static const char frq1[] = "socket.io/1/websocket/";
		static const char frq3[] =
			"socket.io/?EIO=3&transport=websocket&t=1234567890000&sid=";
	case WS_PROTO_SIO1:
		nrq += memncpy(buf + nrq, frq1, strlenof(frq1));
		nrq += memncpy(buf + nrq, sidbuf, siz + 1U/*\nul*/);
		break;
	case WS_PROTO_SIO3:
		nrq += memncpy(buf + nrq, frq3, strlenof(frq3));
		nrq += memncpy(buf + nrq, sidbuf, siz + 1U/*\nul*/);
		break;
	}
	return --nrq;
}

static ssize_t
_init(ws_t ws, const char *host, const char *rsrc, size_t rlen)
{
	static const char ws_req[] = "\
Sec-WebSocket-Version: 13\r\n\
Sec-WebSocket-Key: e8w+o5wQsV0rXFezPUS8XQ==\r\n\
Upgrade: websocket\r\n\
Connection: Upgrade\r\n\
\r\n";
	static const char wamp_req[] = "\
Sec-WebSocket-Version: 13\r\n\
Sec-WebSocket-Key: e8w+o5wQsV0rXFezPUS8XQ==\r\n\
Sec-WebSocket-Protocol: wamp.2.json\r\n\
Upgrade: websocket\r\n\
Connection: Upgrade\r\n\
\r\n";
	static const char rest_req[] = "\
User-Agent: Mozilla/4.0\r\n\
\r\n";
	size_t nrq = 0U;
	ssize_t nrd;
	ssize_t rc = 0;

	nrq = memnmove(gbuf + 4U + (rsrc[0U] != '/'), rsrc, rlen);
	nrq += memncpy(gbuf + 0U, "GET /", 4U + (rsrc[0U] != '/'));
	gbuf[nrq++] = ' ';
	nrq += memncpy(gbuf + nrq, "HTTP/1.1\r\nHost: ", 16U);
	nrq += memncpy(gbuf + nrq, host, strlen(host));
	gbuf[nrq++] = '\r';
	gbuf[nrq++] = '\n';

	switch (ws->p) {
	case WS_PROTO_RAW:
	case WS_PROTO_SIO1:
	case WS_PROTO_SIO3:
		nrq += memncpy(gbuf + nrq, ws_req, strlenof(ws_req));
		break;
	case WS_PROTO_WAMP:
		nrq += memncpy(gbuf + nrq, wamp_req, strlenof(wamp_req));
		break;
	case WS_PROTO_REST:
		nrq += memncpy(gbuf + nrq, rest_req, strlenof(rest_req));
		break;
	default:
		/* don't even try */
		return -1;
	}

	fwrite(gbuf, 1, nrq, stderr);
	if (ws->c) {
		tls_send(ws->c, gbuf, nrq, 0);
		nrd = tls_recv(ws->c, gbuf, gbsz, MSG_PEEK);
	} else {
		send(ws->s, gbuf, nrq, 0);
		nrd = recv(ws->s, gbuf, gbsz, MSG_PEEK);
	}
	if (UNLIKELY(nrd <= 0)) {
		return -1;
	}
	/* find 101 or error */
	if (memcmp(gbuf, "HTTP/1.1 101", 12U)) {
		fputs("Error: protocol switch failed\n", stderr);
		rc = -1;
	}
	/* find end of HTTP header and read the stuff off the wire */
	for (const char *eoh; (eoh = xmemmem(gbuf, nrd, "\r\n\r\n", 4U));) {
		if (ws->c) {
			nrd = tls_recv(ws->c, gbuf, eoh - gbuf, 0);
		} else {
			nrd = recv(ws->s, gbuf, eoh - gbuf, 0);
		}
		break;
	}
	fwrite(gbuf, 1, nrd, stderr);
	return rc;
}

static ws_proto_t
guess_proto(const char *url, const char **host, int *sslp)
{
	ws_proto_t p = WS_PROTO_RAW;

	if (0) {
		;
	} else if (!memcmp(url, "ws", 2U) &&
		   (*sslp = url[2U] == 's',
		    !memcmp(url + 2U + *sslp, "://", 3U))) {
		p = WS_PROTO_RAW;
		*host = url + 2U + *sslp + 3U;
	} else if (!memcmp(url, "wamp", 4U) &&
		   (*sslp = url[4U] == 's',
		    !memcmp(url + 4U + *sslp, "://", 3U))) {
		p = WS_PROTO_WAMP;
		*host = url + 4U + *sslp + 3U;
	} else if (!memcmp(url, "http", 4U) &&
		   (*sslp = url[4U] == 's',
		    !memcmp(url + 4U + *sslp, "://", 3U))) {
		p = WS_PROTO_REST;
		*host = url + 4U + *sslp + 3U;
	} else if (!memcmp(url, "sio", 3U) &&
		   (*sslp = url[4U] == 's',
		    !memcmp(url + 4U + *sslp, "://", 3U))) {
		p = url[3U] == '1' ? WS_PROTO_SIO1 : WS_PROTO_SIO3;
		*host = url + 4U + *sslp + 3U;
	} else {
		/* otherwise assume the host right away */
		*host = url;
		*sslp = 0;
	}
	return p;
}


ws_t
ws_open(const char *url)
{
	size_t urz = strlen(url);
	char buf[urz + 1U];
	ws_proto_t p;
	int sslp;
	const char *host;
	const char *rsrc;;
	size_t rlen;
	short unsigned int port;
	char *on;
	ws_t ws;

	/* copy to stack buffer */
	memcpy(buf, url, urz + 1U);

	/* guess the proto */
	p = guess_proto(buf, &host, &sslp);

	/* look for the resource */
	if (UNLIKELY((on = memchr(host, '/', buf + urz - host)) == NULL)) {
		/* ok doke, we're using / and the whole thing as host */
		rsrc = buf + urz;
	} else {
		/* mark the end of host */
		*on++ = '\0';
		rsrc = on;
	}

	/* look out for ports in the host part */
	if (UNLIKELY((on = memchr(host, ':', rsrc - host)) == NULL)) {
		/* no port given */
		port = (unsigned short)(sslp ? 443 : 80);
	} else {
		long unsigned int x;
		*on = '\0';
		if (UNLIKELY((x = strtoul(on + 1U, NULL, 10)) == 0U)) {
			/* cannot read port argument, fuck off */
			goto nil;
		} else if (UNLIKELY(x >= 65536U)) {
			goto nil;
		}
		/* all is good */
		port = x;
	}

	if (UNLIKELY(gbsz < 4096U)) {
		gbsz = 4096U;
		if (UNLIKELY((gbuf = malloc(gbsz)) == NULL)) {
			goto nil;
		}
	}
	/* inc ref count */
	gbrc++;

	/* try and establish a connection */
	if (UNLIKELY((ws = _open(host, port, sslp)) == NULL)) {
		goto nil;
	}
	/* stash proto */
	switch ((ws->p = p)) {
		ssize_t z;
	default:
		rlen = buf + urz - rsrc;
		break;
	case WS_PROTO_SIO1:
	case WS_PROTO_SIO3:
		if (UNLIKELY((z = _sioi(gbuf, gbsz, ws, host, rsrc)) < 0)) {
			goto fre;
		}
		/* otherwise assign resources */
		rsrc = gbuf;
		rlen = z;
		break;
	case WS_PROTO_REST:
		/* don't call nothing yet */
		ws->host = strdup(host);
		ws->hstz = strlen(host);
		return ws;
	}
	/* fiddle with the port param again */
	if (UNLIKELY(on != 0)) {
		*on = ':';
	}
	/* construct the request */
	if (UNLIKELY(_init(ws, host, rsrc, rlen) < 0)) {
		goto fre;
	}
	return ws;

fre:
	ws_close(ws);
nil:
	return NULL;
}

int
ws_close(ws_t ws)
{
	if (ws->c) {
		close_tls(ws->c);
	} else {
		close(ws->s);
	}
	switch (ws->p) {
	default:
		break;
	case WS_PROTO_REST:
		free(deconst(ws->host));
		break;
	}
	*ws = (struct ws_s){.s = -1};
	free(ws);

	/* just for clarity free the global buffer too */
	if (LIKELY(gbuf != NULL && !--gbrc)) {
		free(gbuf);
		gbuf = NULL;
		gbsz = 0UL;
	}
	return 0;
}

ssize_t
ws_recv(ws_t ws, void *restrict buv, size_t bsz, int flags)
{
/* we guarantee that full frames end in \n */
	uint8_t *restrict buf = buv;
	size_t pp = 0U, rp = 0U;
	size_t pz;
	ssize_t nrd;

	if (UNLIKELY((nrd = -(size_t)ws->state & ws->togo))) {
		/* replay stash from last time */
		memncpy(buf, &ws->frml, nrd);
		ws->togo = 0U;
		ws->state = 0U;
	}
	if (ws->c) {
		nrd += tls_recv(ws->c, buf + nrd, bsz - nrd, flags);
	} else {
		nrd += recv(ws->s, buf + nrd, bsz - nrd, flags);
	}
	if (UNLIKELY(nrd <= 0)) {
		return -1;
	}

	/* we've got to deal with these cases:
	 * - buf too small
	 * - ws frame fragmented
	 * - multiple ws frames in one go */

	if (UNLIKELY(ws->togo)) {
		if (nrd < ws->togo) {
			ws->togo -= nrd;
			return nrd;
		}
		unsigned int finp = ws->frml.finp;
		/* otherwise there's the rest of the old packet
		 * and new packets afterwards */
		rp = pp = ws->togo;
		ws->togo = 0U;
		/* finalise */
		if (pp >= (size_t)nrd) {
			/* no need, finalise */
			buf[pp] = '\n';
			return pp + finp;
		}
		/* otherwise copy over the ws frame so we can insert the
		 * newline as indicator of a finished frame */
		memcpy(&ws->frml, buf + pp, sizeof(ws->frml));
		buf[rp] = '\n';
		rp += finp;
		goto unpkd;
	}

again:
	/* unpack him */
	memcpy(&ws->frml, buf + pp, sizeof(ws->frml));
unpkd:
	switch (ws->frml.plen) {
	case 126U:
		pz = be16toh(ws->frml.plen16);
		pp += offsetof(wsfr_t, plen16) + sizeof(ws->frml.plen16);
		break;
	case 127U:
		with (uint64_t _z) {
			memcpy(&_z, &ws->frml.plen16, sizeof(_z));
			pz = be64toh(_z);
		}
		pp += offsetof(wsfr_t, mkey);
		break;
	default:
		pz = ws->frml.plen;
		pp += offsetof(wsfr_t, plen16);
		break;
	}
	/* mind the masking */
	pp += ws->frml.mask ? sizeof(ws->frml.mkey) : 0U;

	if (pz + pp > (size_t)nrd) {
		fprintf(stderr, "CONT?  need %zu  got %zu %zu->%zd\n", pz, nrd - pp, pp, nrd);
		pz -= nrd - pp;
		ws->togo = pz;
		pz = nrd - pp;
	}

	switch (ws->frml.code) {
	case 0x0U:
		/* frame continuation */
	case 0x1U:
		/* text */
	case 0x2U:
		/* binary */
		rp += memnmove(buf + rp, buf + pp, pz);
		buf[rp] = '\n';
		rp += !ws->togo && ws->frml.finp;
		break;

	case 0x9U:
		/* ping, respond immediately */
		fputs("PING???\n", stderr);
		ws_pong(ws, buf + pp, pz);
		fputs("PONG!!!\n", stderr);
		rp += memnmove(buf + rp, buf + pp, pz);
		buf[rp++] = '\n';
		break;
	case 0xaU:
		/* pong */
		fputs("PONG!!!\n", stderr);
		rp += memnmove(buf + rp, buf + pp, pz);
		buf[rp++] = '\n';
		break;

	case 0x8U:
		/* close */
		rp += memnmove(buf + rp, buf + pp, pz);
		buf[rp++] = '\n';
		break;
	default:
		fputs("HUH?!?!\n", stderr);
		abort();
		break;
	}
	if ((pp += pz) + sizeof(ws->frml) < (size_t)nrd) {
		/* more frames, good for us */
		goto again;
	} else if (pp < (size_t)nrd) {
		/* stuff the rest into frml and process it next */
		ws->togo = memncpy(&ws->frml, buf + pp, nrd - pp);
		ws->state = 1U;
	}
	return rp;
}

ssize_t
ws_send(ws_t ws, const void *buf, size_t bsz, int flags)
{
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

	if (bsz >= 65536U) {
		uint64_t blen = htobe64(bsz);
		fr.plen = 127U;
		fr.plen16 = (blen >> 48) & 0xffffU;
		fr.plen16_1 = (blen >> 32) & 0xffffU;
		fr.plen16_2 = (blen >> 16) & 0xffffU;
		fr.plen16_3 = (blen >> 0) & 0xffffU;
		slen = sizeof(wsfr_t);
	} else if (bsz >= 126U) {
		fr.plen = 126U;
		fr.plen16 = htobe16(bsz);
		slen = offsetof(wsfr_t, plen16) +
			sizeof(fr.plen16) + sizeof(fr.mkey);
	} else {
		fr.plen = bsz;
		slen = offsetof(wsfr_t, plen16) + sizeof(fr.mkey);
	}

	if (UNLIKELY(gbsz < slen + bsz)) {
		/* just free the old guy and get a new one */
		if (LIKELY(gbuf != NULL)) {
			free(gbuf);
		}
		/* round up to next 256-multiple */
		gbsz = ((slen + bsz) | 0xff) + 1U;
		if (UNLIKELY((gbuf = malloc(gbsz)) == NULL)) {
			gbsz = 0U;
			return -1;
		}
	}
	/* copy framing ... */
	memcpy(gbuf, &fr, slen);
	/* .. and payload */
	memcpy(gbuf + slen, buf, bsz);

	if (ws->c) {
		return tls_send(ws->c, gbuf, slen + bsz, flags);
	}
	/* otherwise */
	return send(ws->s, gbuf, slen + bsz, flags);
}

int
ws_ping(ws_t ws, const void *msg, size_t msz)
{
	static const char _ping[] = {0x89, 0x80, 0x00, 0x00, 0x00, 0x00};
	char buf[sizeof(_ping) + msz];
	size_t pinz = sizeof(_ping);
	const char *ping = _ping;

	_Static_assert(sizeof(_ping) == 6U, "PING frame of wrong size");

	if (UNLIKELY(msz > 0U)) {
		if (UNLIKELY(msz > 125U)) {
			msz = 125U;
		}
		buf[0U] = 0x8a;
		buf[1U] = 0x80 ^ msz;
		/* masking key */
		buf[2U] = buf[3U] = buf[4U] = buf[5] = 0;
		memcpy(buf + sizeof(_ping), msg, msz);
		ping = buf, pinz = sizeof(_ping) + msz;
	}
	if (ws->c) {
		return (tls_send(ws->c, ping, pinz, 0) >= 0) - 1U;
	}
	return (send(ws->s, ping, pinz, 0) >= 0) - 1U;
}

int
ws_pong(ws_t ws, const void *msg, size_t msz)
{
	static const char _pong[] = {0x8a, 0x80, 0x00, 0x00, 0x00, 0x00};
	char buf[sizeof(_pong) + msz];
	size_t ponz = sizeof(_pong);
	const char *pong = _pong;

	_Static_assert(sizeof(_pong) == 6U, "PONG frame of wrong size");

	if (UNLIKELY(msz > 0U)) {
		if (UNLIKELY(msz > 125U)) {
			msz = 125U;
		}
		buf[0U] = 0x8a;
		buf[1U] = 0x80 ^ msz;
		/* masking key */
		buf[2U] = buf[3U] = buf[4U] = buf[5] = 0;
		memcpy(buf + sizeof(_pong), msg, msz);
		pong = buf, ponz = sizeof(_pong) + msz;
	}
	if (ws->c) {
		return (tls_send(ws->c, pong, ponz, 0) >= 0) - 1U;
	}
	return (send(ws->s, pong, ponz, 0) >= 0) - 1U;
}

ssize_t
rest_recv(ws_t ws, void *restrict buv, size_t bsz, int flags)
{
/* we guarantee that a whole message ends in \n */
#define HDR_SEEN	1U
#define LEN_SEEN	2U
#define ENC_SEEN	4U
	char *restrict buf = buv;
	ssize_t nrd = 0;

	if (ws->c) {
		nrd = tls_recv(ws->c, buf, bsz, flags);
	} else {
		nrd = recv(ws->s, buf, bsz, flags);
	}
	if (UNLIKELY(nrd <= 0)) {
		return -1;
	}
	/* see if we need to snarf headers first */
	if (!ws->state) {
		/* ah, headers innit */
		static const char clen1[] = "Content-Length:";
		static const char clen2[] = "Transfer-Encoding:";
		const char *p;

		if ((p = xmemmem(buf, nrd, clen1, strlenof(clen1)))) {
			/* couldn't be more perfect */
			ws->togo = strtoul(p, NULL, 10);
			ws->state |= LEN_SEEN;
		} else if ((p = xmemmem(buf, nrd, clen2, strlenof(clen2)))) {
			/* chunked, shit */
			ws->togo = 0U;
			ws->state |= ENC_SEEN;
		}
	}
	if (!(ws->state & HDR_SEEN)) {
		const char *p;

		if (UNLIKELY(!(p = xmemmem(buf, nrd, "\r\n\r\n", 4U)))) {
			/* just forget about it all */
			return 0;
		} else if ((nrd -= p - buf)) {
			memmove(buf, p, nrd);
			ws->state |= HDR_SEEN;
		}
	}

	if (ws->state & LEN_SEEN) {
		if (nrd < ws->togo) {
			ws->togo -= nrd;
		} else {
			/* otherwise we've seen it all */
			nrd = ws->togo;
			ws->togo = 0U;
			ws->state = 0U;
			/* finalise */
			buf[nrd++] = '\n';
		}
	} else if (ws->state & ENC_SEEN) {
		if (!ws->togo) {
			char *on;
			ws->togo = strtoul(buf, &on, 16U);
			if (!ws->togo || *on++ != '\r' || *on++ != '\n') {
				/* god knows */
				ws->state = 0;
				nrd = 0;
				/* finalise */
				buf[nrd++] = '\n';
				return nrd;
			} else {
				nrd -= on - buf;
				memmove(buf, on, nrd);
			}
		}

		if (nrd < ws->togo) {
			ws->togo -= nrd;
		} else {
			nrd = ws->togo;
			ws->togo = 0U;
		}
	}
	return nrd;
}

ssize_t
rest_send(ws_t ws, const char *url, size_t urz, int flags)
{
	static const char rest_req[] = "\
User-Agent: Mozilla/4.0\r\n\
\r\n";
	size_t nrq = 0U;
	ssize_t nwr;

	if (UNLIKELY(ws->p != WS_PROTO_REST)) {
		return -1;
	}

	nrq += memncpy(gbuf + nrq, "GET ", 4U);
	nrq += memncpy(gbuf + nrq, url, urz);
	gbuf[nrq++] = ' ';
	nrq += memncpy(gbuf + nrq, "HTTP/1.1\r\nHost: ", 16U);
	nrq += memncpy(gbuf + nrq, ws->host, ws->hstz);
	gbuf[nrq++] = '\r';
	gbuf[nrq++] = '\n';
	nrq += memncpy(gbuf + nrq, rest_req, strlenof(rest_req));

	fwrite(gbuf, 1, nrq, stderr);
	if (ws->c) {
		nwr = tls_send(ws->c, gbuf, nrq, flags);
	} else {
		nwr = send(ws->s, gbuf, nrq, flags);
	}
	/* reset state */
	ws->state = 0U;
	ws->togo = 0U;
	return nwr;
}

/* ws.c ends here */
