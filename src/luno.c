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
#include "../luno_cred.h"

#define API_URL		"wss://ws.bitx.co/api/1/stream/"


int
join_coin(ws_t ws)
{
	static const char req[] = "{\
\"api_key_id\":\"" API_KEY "\",\
\"api_key_secret\":\"" API_SEC "\"\
}";
	ws_send(ws, req, strlenof(req), 0);
	return 0;
}

int
heartbeat(ws_t ws)
{
	ws_send(ws, "{}", 2U, 0);
	return 0;
}


#include "luno.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	static char api_url[64U] = API_URL;
	wssnarf_t wss;
	int rc = EXIT_SUCCESS;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

	if (!argi->nargs) {
		fputs("\
Error: need exactly one pair\n", stderr);
		rc = EXIT_FAILURE;
		goto out;
	}

	/* copy pair to api url */
	size_t z = strlenof(API_URL);
	z += memncpy(api_url + z, *argi->args, strlen(*argi->args));
	api_url[z] = '\0';

	/* obtain a loop */
	wss = make_wssnarf("prices");

	add_wssnarf(wss, (wssnarf_param_t){api_url, 3.0, 30.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

out:
	/* that's it */
	yuck_free(argi);
	return rc;
}

/* cexwss.c ends here */
