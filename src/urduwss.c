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


int
join_coin(ws_t ws)
{
	static const char req[] = "{\
\"MsgType\":\"V\",\
\"MDReqID\":\"31415\",\
\"SubscriptionRequestType\":\"1\",\
\"MarketDepth\":0,\
\"MDUpdateType\":\"1\",\
\"MDEntryTypes\":[\"0\",\"1\",\"2\"],\
\"Instruments\":[\"BTCPKR\"]\
}";
	ws_send(ws, req, strlenof(req), 0);
	return 1;
}


#include "urduwss.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	wssnarf_t wss;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

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

/* urduwss.c ends here */
