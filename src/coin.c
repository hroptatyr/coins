#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include "wssnarf.h"
#include "nifty.h"

#define API_URL		"wss://ws-feed.gdax.com/"
#define REST_URL	"https://api.gdax.com"

static const char *const *subs;
static size_t nsubs;
static wssnarf_t wss;


int
join_coin(ws_t ws)
{
	static char buf[256U];

	for (size_t i = 0U; i < nsubs; i++) {
		int len = snprintf(
			buf, sizeof(buf),
			"{\"type\": \"subscribe\", \"product_id\": \"%s\"}\r\n",
			subs[i]);
		ws_send(ws, buf, len, 0);
	}
	return 0;
}

int
heartbeat(ws_t ws)
{
	static char url[] = "/products/xxx-xxx/book?level=2";
	static size_t isub;

	/* cycle instruments */
	if (UNLIKELY(ws_proto(ws) != WS_PROTO_REST)) {
		return 0;
	} else if (UNLIKELY(isub >= nsubs)) {
		isub = 0U;
		return 0;
	}
	/* copy instrument */
	memcpy(url + strlenof("/products/"), subs[isub++], 7U);
	wssnarf_log(wss, url, strlenof(url));
	rest_send(ws, url, strlenof(url), 0);
	return 1;
}


#include "coin.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];

	if (yuck_parse(argi, argc, argv) < 0) {
		return 1;
	}

	/* make sure we won't forget them subscriptions */
	subs = argi->args;
	nsubs = argi->nargs;

	/* obtain a loop */
	wss = make_wssnarf("prices");
	add_wssnarf(wss, (wssnarf_param_t){API_URL, 6.0, 60.0});
	add_wssnarf(wss, (wssnarf_param_t){REST_URL, 15.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* coin.c ends here */
