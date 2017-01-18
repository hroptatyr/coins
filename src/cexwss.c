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
#include <openssl/hmac.h>
#include "wssnarf.h"
#include "nifty.h"
#include "../cex_cred.h"

#define API_URL		"wss://ws.cex.io/ws"

static const char *const *subs;
static size_t nsubs;


static size_t
hmac(
	char *restrict buf, size_t bsz,
	const char *msg, size_t len,
	const char *key, size_t ksz)
{
	unsigned int hlen = bsz;
	unsigned char *sig = HMAC(
		EVP_sha256(), key, ksz,
		(const unsigned char*)msg, len,
		NULL, &hlen);

	/* hexl him */
	for (size_t i = 0U; i < hlen * 2U; i += 2U) {
		buf[i + 0U] = (char)((unsigned char)(sig[i / 2U] >> 4U) & 0xfU);
		buf[i + 1U] = (char)((unsigned char)(sig[i / 2U] >> 0U) & 0xfU);
	}
	for (size_t i = 0U; i < hlen * 2U; i++) {
		if ((unsigned char)buf[i] < 10U) {
			buf[i] ^= '0';
		} else {
			buf[i] += 'W';
		}
	}
	return hlen;
}


int
auth_coin(ws_t ws)
{
#define AUTH	"{\
\"e\":\"auth\",\"auth\":{\"key\":\"" API_KEY "\",\
\"timestamp\":%ld,\
\"signature\":\"%.*s\"}\
}"
	static char buf[256U];
	static char tok[] = "\0\0\0\0\0\0\0\0\0\0" API_KEY;
	/* signature is HMAC-rsa256(tst+key) */
	static char sig[64U];

	/* prepare the login signature */
	with (time_t t = time(NULL)) {
		int len = snprintf(tok, sizeof(tok), "%ld", t);
		if (len < 0) {
			return -1;
		}
		len += memncpy(tok + len, API_KEY, strlenof(API_KEY));
		hmac(sig, sizeof(sig), tok, len, API_SEC, strlenof(API_SEC));

		len = snprintf(buf, sizeof(buf), AUTH "\r\n", t, 64, sig);
		ws_send(ws, buf, len, 0);
	}
	return 0;
}

int
join_coin(ws_t ws)
{
#define MREQ	"{\
\"e\":\"order-book-subscribe\",\
\"data\":{\"pair\":[\"%s\",\"%s\"],\"subscribe\":true,\"depth\":0}\
}"
#define TREQ	"{\
\"e\":\"subscribe\",\
\"rooms\":[\"tickers\"]\
}"
	static char buf[256U];

	with (size_t len = memncpy(buf, TREQ "\r\n", strlenof(TREQ "\r\n"))) {
		ws_send(ws, buf, len, 0);
	}

	for (size_t i = 0U; i < nsubs; i++) {
		int len = snprintf(buf, sizeof(buf), MREQ "\r\n",
				   subs[i] + 0U, subs[i] + 4U);
		ws_send(ws, buf, len, 0);
	}
	return 0;
}

int
heartbeat(ws_t ws)
{
	static const char pong[] = "{\"e\":\"pong\"}\r\n";

	ws_send(ws, pong, strlenof(pong), 0);
	fwrite(pong, 1, strlenof(pong), stderr);
	return 0;
}


#include "cexwss.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	wssnarf_t wss;
	int rc = EXIT_SUCCESS;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

	/* check symbols */
	for (size_t i = 0U; i < argi->nargs; i++) {
		if (argi->args[i][3U] != '/' && argi->args[i][3U] != ':') {
			fputs("\
Error: specify pairs like CCY:CCY", stderr);
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
	wss = make_wssnarf((wssnarf_param_t){API_URL, "prices", 30.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

out:
	/* that's it */
	yuck_free(argi);
	return rc;
}

/* cexwss.c ends here */
