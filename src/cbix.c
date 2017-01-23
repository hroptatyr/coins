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

#define API_BK		"wss://socket.cbix.ca/orderbook"
#define API_TRA		"wss://socket.cbix.ca/trades"


int
join_coin(ws_t UNUSED(foo))
{
	return 0;
}


#include "cbix.yucc"

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

	add_wssnarf(wss, (wssnarf_param_t){API_BK, 6.0, 60.0});
	add_wssnarf(wss, (wssnarf_param_t){API_TRA, 6.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* hitwss.c ends here */
