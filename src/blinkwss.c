#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "wssnarf.h"
#include "nifty.h"

#define API_URL		"wss://api.blinktrade.com/trade/"

static const char *const *subs;
static size_t nsubs;
static wssnarf_t wss;


int
join_coin(ws_t ws)
{
#define PROTO_REQ	"{\
\"MsgType\":\"V\",\
\"MDReqID\":\"31416\",\
\"SubscriptionRequestType\":\"1\",\
\"MarketDepth\":0,\
\"MDUpdateType\":\"1\",\
\"MDEntryTypes\":[\"0\",\"1\",\"2\"],\
\"Instruments\":[\"xxxxxx\"]\
}"
	static char req[1024U] = PROTO_REQ;
	size_t z = strlenof(PROTO_REQ) - 11U;

	req[z++] = '[';
	for (size_t i = 0U; i < nsubs; i++, req[z++] = ',') {
		req[z++] = '"';
		z += memncpy(req + z, subs[i], strlen(subs[i]));
		req[z++] = '"';
	}
	/* delete last comma */
	z -= nsubs > 0U;
	req[z++] = ']';
	req[z++] = '}';

	wssnarf_log(wss, req, z);
	ws_send(ws, req, z, 0);
	return 0;
}


#include "blinkwss.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

	/* memorise instrs */
	subs = argi->args;
	nsubs = argi->nargs;

	/* obtain a loop */
	wss = make_wssnarf("prices");

	add_wssnarf(wss, (wssnarf_param_t){API_URL, 6.0, 300.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* blinkwss.c ends here */
