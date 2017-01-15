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

#define API_URL		"ws://fastmatch.com/node/"

static const char *const *subs;
static size_t nsubs;


int
join_coin(ws_t ws)
{
	static const char flt[] = "{\
\"filter\":[\"NMC\",\"SPH\",\"SD\",\"BK\",\"MA\"]\
}";
	char buf[64U];

	ws_send(ws, flt, strlenof(flt), 0);
	for (size_t i = 0U; i < nsubs; i++) {
		int len = snprintf(buf, sizeof(buf), "\
{\"currency\":\"%s\"}", subs[i]);
		ws_send(ws, buf, len, 0);
	}
	return 0;
}


#include "fast.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	wssnarf_t wss;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
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

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* fast.c ends here */
