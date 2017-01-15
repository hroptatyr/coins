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
#include "wssnarf.h"
#include "nifty.h"

#define API_URL		"wss://ws.pusherapp.com/app/de504dc5763aeef9ff52?protocol=7"


int
join_coin(ws_t ws)
{
	static const char *subs[] = {"{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_trades\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_trades_btceur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_trades_eurusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_trades_xrpusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_trades_xrpeur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_orders\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_orders_btceur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_orders_eurusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_orders_xrpusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"live_orders_xrpeur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"diff_order_book\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"diff_order_book_btceur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"diff_order_book_eurusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"diff_order_book_xrpusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"diff_order_book_xrpeur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"order_book\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"order_book_btceur\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"order_book_eurusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"order_book_xrpusd\"}\
}", "{\
\"event\":\"pusher:subscribe\",\
\"data\":{\"channel\":\"order_book_xrpeur\"}\
}"
	};

	for (size_t i = 0U; i < countof(subs); i++) {
		const size_t z = strlen(subs[i]);
		ws_send(ws, subs[i], z, 0);
	}
	return 1;
}


#include "btsp.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	wssnarf_t wss;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

	/* obtain a loop */
	wss = make_wssnarf((wssnarf_param_t){API_URL, "prices"});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* btsp.c ends here */
