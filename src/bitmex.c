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

#define API_URL		"wss://www.bitmex.com/realtime"

static const char *const *subs;
static size_t nsubs;


int
join_coin(ws_t ws)
{
	static char sub[] = "{\
\"op\":\"subscribe\",\"args\":[\"orderBookL2:xxxxxxxxxxxx\"]\
}";

	if (!nsubs) {
		size_t z = strlenof(sub) - 16U;

		sub[z++] = '"';
		sub[z++] = ']';
		sub[z++] = '}';

		ws_send(ws, sub, z, 0);
		fwrite(sub, 1, z, stderr);
		fputc('\n', stderr);
	}
	for (size_t i = 0U; i < nsubs; i++) {
		const char *ins = subs[i];
		const size_t inz = strlen(ins);
		size_t z = strlenof(sub) - 16U;

		if (UNLIKELY(inz > 12U)) {
			continue;
		}
		z++;
		z += memncpy(sub + z, ins, inz);
		sub[z++] = '"';
		sub[z++] = ']';
		sub[z++] = '}';

		ws_send(ws, sub, z, 0);
		fwrite(sub, 1, z, stderr);
		fputc('\n', stderr);
	}
	return 1;
}


#include "bitmex.yucc"

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
	wss = make_wssnarf("prices");
	add_wssnarf(wss, (wssnarf_param_t){API_URL, 0.0, 30.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* bitmex.c ends here */
