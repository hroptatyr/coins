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
#include "wssnarf.h"
#include "nifty.h"

#define API_URL		"wamps://api.poloniex.com/"
#define REST_URL	"https://poloniex.com/public?command=returnOrderBook&currencyPair=all"

static const char *const *subs;
static size_t nsubs;
static wssnarf_t wss;


int
heartbeat(ws_t ws)
{
#define PRFX	"/public?command=returnOrderBook&currencyPair="
	static char url[256U] = PRFX;
	static size_t isub;
	size_t z;

	/* cycle instruments */
	if (UNLIKELY(ws_proto(ws) != WS_PROTO_REST)) {
		return 0;
	} else if (UNLIKELY(isub >= nsubs)) {
		isub = 0U;
		return 0;
	}
	/* copy instrument */
	z = strlenof(PRFX);
	z += memncpy(url + z, subs[isub], strlen(subs[isub]));
	wssnarf_log(wss, url, z);
	rest_send(ws, url, z, 0);
	isub++;
	return 1;
}

int
auth_coin(ws_t ws)
{
	static char ehlo[] =
		"[1,\"realm1\",{\"roles\" : {\"subscriber\":{}}}]\r\n";

	ws_send(ws, ehlo, strlenof(ehlo), 0);
	return 0;
}

int
join_coin(ws_t ws)
{
#define MREQ	"[32, %zu, {}, \"%s\"]\r\n"
	static char buf[256U];

	for (size_t i = 0U; i < nsubs; i++) {
		int len = snprintf(buf, sizeof(buf), MREQ, i, subs[i]);
		ws_send(ws, buf, len, 0);
	}
	return 1;
}


#include "plxwss.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

	/* make sure we won't forget them subscriptions */
	subs = argi->args;
	nsubs = argi->nargs;

	/* obtain a loop */
	wss = make_wssnarf("prices");
	add_wssnarf(wss, (wssnarf_param_t){API_URL, 0.0, 30.0});
	add_wssnarf(wss, (wssnarf_param_t){"https://poloniex.com", 3.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* plxwss.c ends here */
