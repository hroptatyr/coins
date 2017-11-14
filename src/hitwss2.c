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

#define API_URL		"wss://api.hitbtc.com/api/2/ws"


static size_t ninstr;
static char *const *instr;

int
heartbeat(ws_t ws)
{
	static size_t isub;
	char sub_bk[256U];
	char sub_tr[256U];

	if (UNLIKELY(isub >= ninstr)) {
		return 0;
	}
	int len_bk = snprintf(sub_bk, sizeof(sub_bk), "{\
\"method\":\"subscribeOrderbook\",\
\"params\": {\"symbol\":\"%s\"},\
\"id\":1\
}", instr[isub]);
	int len_tr = snprintf(sub_tr, sizeof(sub_tr), "{\
\"method\":\"subscribeTrades\",\
\"params\": {\"symbol\":\"%s\"},\
\"id\":2\
}", instr[isub]);

	ws_send(ws, sub_bk, len_bk, 0);
	ws_send(ws, sub_tr, len_tr, 0);
	isub++;
	return 1;
}


#include "hitwss.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	wssnarf_t wss;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

	ninstr = argi->nargs;
	instr = argi->args;

	/* obtain a loop */
	wss = make_wssnarf("prices");

	add_wssnarf(wss, (wssnarf_param_t){API_URL, 3.0, 30.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* hitwss2.c ends here */
