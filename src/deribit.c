#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#undef EV_COMPAT3
#include <ev.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include "wssnarf.h"
#include "nifty.h"
#include "../deribit_cred.h"

#define API_URL		"wss://www.deribit.com/ws/api/v1/private/subscribe"


static size_t
sha256(char *restrict buf, size_t UNUSED(bsz), const char *msg, size_t len)
{
	unsigned char *sig = SHA256((const unsigned char*)msg, len, NULL);

	/* base64 him */
	return EVP_EncodeBlock((unsigned char*)buf, sig, 256U / 8U);
}


#define NONCE	"1452237485895"
static char sub[] = "{\
\"id\":5533,\"action\":\"/api/v1/private/subscribe\",\"arguments\":{\
\"instrument\":[\"all\"],\
\"event\":[\"order_book\",\"trade\"]\
},\
\"sig\":\"" API_KEY "." NONCE "."
		"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"\
}";
static const char *sline;
static size_t sllen;

int
auth_coin(ws_t UNUSED(ws))
{
	static const char sig[] = "_=" NONCE
		"&_ackey=" API_KEY
		"&_acsec=" API_SEC
		"&_action=/api/v1/private/subscribe"
		"&event=order_booktrade&instrument=all";
	size_t z;

	z = strlenof(sub) - 48U;
	z += sha256(sub + z, strlenof(sub) - z, sig, strlenof(sig));
	sub[z++] = '"';
	sub[z++] = '}';
	/* store him */
	sline = sub;
	sllen = z;
	return 1;
}

int
join_coin(ws_t ws)
{
	ws_send(ws, sline, sllen, 0);
	fwrite(sline, 1, sllen, stderr);
	fputc('\n', stderr);
	return 0;
}

int
heartbeat(ws_t ws)
{
	fputs("PING!!!\n", stderr);
	ws_send(ws, sline, sllen, 0);
	return 0;
}


#include "deribit.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	wssnarf_t wss;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

	/* obtain a loop */
	wss = make_wssnarf("prices");
	add_wssnarf(wss, (wssnarf_param_t){API_URL, 6.0, 30.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* deribit.c ends here */
