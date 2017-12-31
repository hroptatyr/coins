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

#define API_URL		"wss://stream.binance.com:9443/ws/!ticker@arr"

static wssnarf_t wss;


#include "bncwss.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];

	if (yuck_parse(argi, argc, argv) < 0) {
		return 1;
	}

	/* obtain a loop */
	wss = make_wssnarf("prices");
	add_wssnarf(wss, (wssnarf_param_t){API_URL, 3.0, 60.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* coin.c ends here */
