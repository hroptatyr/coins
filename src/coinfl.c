#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#undef EV_COMPAT3
#include <ev.h>
#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/ecdsa.h>
#include "wssnarf.h"
#include "nifty.h"
#include "../coinfl_cred.h"

#define API_URL		"ws://api.coinfloor.co.uk/"
#define CLI_NONCE	"n1MMMrSPIgWdwUQ2"
#define CLI_NNC64	"bjFNTU1yU1BJZ1dkd1VRMg=="

static const char *const *subs;
static size_t nsubs;


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

static inline size_t
memncpy(void *restrict tgt, const void *src, size_t zrc)
{
	memcpy(tgt, src, zrc);
	return zrc;
}

static char*
xmemmem(const char *hay, const size_t hayz, const char *ndl, const size_t ndlz)
{
/* the one that goes NDLZ past HAY */
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


static unsigned char sigr[64U], sigs[64U];
static size_t zigr, zigs;

static struct {
	char ccy[4U];
	uint32_t cod;
} tbl[] = {
	{"XBT", 0xf800},
	{"EUR", 0xfa00},
	{"GBP", 0xfa20},
	{"USD", 0xfa80},
	{"PLN", 0xfda8},
};

static size_t
sha224(unsigned char buf[static 28U], const void *msg, size_t len)
{
	unsigned char *sig = SHA224((const unsigned char*)msg, len, NULL);

	/* base64 him */
	return memncpy(buf, sig, 224U / 8U);
}

static void
calc_auth(const void *nonce, size_t nonze)
{
	static unsigned char sec[] = "00000000" API_SEC;
	static unsigned char nnc[] = "00000000" CLI_NONCE CLI_NONCE;
	unsigned char key[28U], dgst[28U];
	unsigned char r[64U], s[64U];
	size_t rz, sz;

	with (uint64_t x = htobe64(API_UID)) {
		memcpy(sec, &x, sizeof(x));
		memcpy(nnc, &x, sizeof(x));
	}

	sha224(key, sec, strlenof(sec));
	EVP_DecodeBlock(nnc + sizeof(uint64_t)/*API_UID*/, nonce, nonze);
	memncpy(nnc + sizeof(uint64_t)/*API_UID*/ + strlenof(CLI_NONCE),
		CLI_NONCE, strlenof(CLI_NONCE));
	sha224(dgst, nnc, strlenof(nnc));

	BIGNUM *k;
	/* bang private key */
	EC_KEY *K = EC_KEY_new_by_curve_name(NID_secp224k1);
	k = BN_bin2bn(key, sizeof(key), NULL);
	EC_KEY_set_private_key(K, k);

	/* construct the pub key */
	const EC_GROUP *G = EC_KEY_get0_group(K);
	EC_POINT *P = EC_POINT_new(G);
	if (!EC_POINT_mul(G, P, k, NULL, NULL, NULL)) {
		serror("Error: EC_POINT_mul");
		return;
	}
	EC_KEY_set_public_key(K, P);

	/* ready for signing */
	ECDSA_SIG *x = ECDSA_do_sign(dgst, sizeof(dgst), K);

	if (x == NULL) {
		serror("Error: cannot calculate signature");
		return;
	}
	rz = BN_bn2bin(x->r, r);
	sz = BN_bn2bin(x->s, s);
	ECDSA_SIG_free(x);
	EC_KEY_free(K);
	EC_POINT_free(P);
	BN_free(k);
	zigr = EVP_EncodeBlock(sigr, r, rz);
	zigs = EVP_EncodeBlock(sigs, s, sz);
	return;
}


int
connd_coin(const char *buf, size_t bsz)
{
	puts("CONND?");
	for (const char *nnc; (nnc = xmemmem(buf, bsz, "nonce", 5U));) {
		const char *eon;

		/* find : */
		while (*nnc++ != ':');
		/* find " */
		while (*nnc++ != '"');
		/* find " */
		for (eon = nnc; *eon != '"'; eon++);

		fputs("CONND nonce:", stderr);
		fwrite(nnc, 1, eon - nnc, stderr);
		fputc('\n', stderr);

		calc_auth(nnc, eon - nnc);
		return 1;
	}
	/* consider conn failed */
	return -1;
}

int
authd_coin(const char *buf, size_t bsz)
{
	static const char yep[] = "\"error_code\":0";

	puts("AUTHD?");
	for (const char *err; (err = xmemmem(buf, bsz, yep, strlenof(yep)));) {
		return 1;
	}
	return -1;
}

int
conn_coin(ws_t UNUSED(ws))
{
	return 0;
}

int
auth_coin(ws_t ws)
{
	static char aut[] = "{\
\"method\":\"Authenticate\",\
\"user_id\":                \
\"cookie\":\"" API_CKY "\",\
\"nonce\":\"" CLI_NNC64 "\",\
\"signature\":[\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\",\
\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"]\
}";
	size_t z;

	if (UNLIKELY(!zigr || !zigs)) {
		/* no signature? */
		errno = 0, serror("\
Error: no signature has been computed");
		return -1;
	}

	/* fill in uid */
	aut[36U + snprintf(aut + 36U, 12U, "%d", API_UID)] = ',';
	/* copy signatures */
	z = strlenof(aut) - 3U - 64U - 3U - 64U;
	z += memncpy(aut + z, sigr, zigr);
	aut[z++] = '"';
	aut[z++] = ',';
	aut[z++] = '"';
	z += memncpy(aut + z, sigs, zigs);
	aut[z++] = '"';
	aut[z++] = ']';
	aut[z++] = '}';

	ws_send(ws, aut, z, 0);
	aut[z++] = '\n';
	fwrite(aut, 1, z, stderr);
	return 0;
}

int
join_coin(ws_t ws)
{
	static const char fmt[] = "{\
\"method\":\"WatchOrders\",\
\"base\":%u,\
\"counter\":%u,\
\"watch\":true\
}";
	char req[256U];
	size_t z;

	for (size_t i = 0U; i < nsubs; i++) {
		/* find the base */
		size_t b, t;
		for (b = 0U; b < countof(tbl); b++) {
			if (!memcmp(tbl[b].ccy, subs[i] + 0U, 4U)) {
				goto terms;
			}
		}
		continue;
	terms:
		for (t = 0U; t < countof(tbl); t++) {
			if (!memcmp(tbl[t].ccy, subs[i] + 4U, 4U)) {
				goto format;
			}
		}
		continue;
	format:
		z = snprintf(req, sizeof(req), fmt, tbl[b].cod, tbl[t].cod);

		ws_send(ws, req, z, 0);
		fwrite(req, 1, z, stderr);
		fputc('\n', stderr);
	}
	return 0;
}



#include "coinfl.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	wssnarf_t wss;
	int rc = EXIT_SUCCESS;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = EXIT_FAILURE;
		goto out;
	}

	/* check symbols */
	for (size_t i = 0U; i < argi->nargs; i++) {
		if (argi->args[i][3U] != '/' && argi->args[i][3U] != ':') {
			errno = 0, serror("\
Error: specify pairs like CCY:CCY, with CCY out of XBT, EUR, GBP, USD, PLN");
			rc = EXIT_FAILURE;
			goto out;
		}
		/* split string in two halves */
		argi->args[i][3U] = '\0';
	}
	/* make sure we won't forget them subscriptions */
	subs = argi->args;
	nsubs = argi->nargs;

	/* obtain a loop */
	wss = make_wssnarf((wssnarf_param_t){API_URL, "prices"});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

out:
	/* that's it */
	yuck_free(argi);
	return rc;
}

/* deribit.c ends here */
