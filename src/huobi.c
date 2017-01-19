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

#define API_URL		"sio1://hq.huobi.com"


int
heartbeat(ws_t ws)
{
	ws_send(ws, "2::", 3U, 0);
	return 0;
}

int
auth_coin(ws_t UNUSED(ws))
{
	return 0;
}

int
join_coin(ws_t ws)
{
	static const char req[] = "5:::{\
\"name\":\"request\", \"args\":[{\
\"symbolList\":{\"marketDepthTop\":[\
	{\"symbolId\":\"btccny\",\"pushType\":\"websocket\"},\
	{\"symbolId\":\"ltccny\",\"pushType\":\"websocket\"},\
	{\"symbolId\":\"btcusd\",\"pushType\":\"websocket\"}\
], \"tradeDetail\":[\
	{\"symbolId\":\"btccny\",\"pushType\":\"websocket\"},\
	{\"symbolId\":\"ltccny\",\"pushType\":\"websocket\"},\
	{\"symbolId\":\"btcusd\",\"pushType\":\"websocket\"}\
]\
},\"version\":1,\"msgType\":\"reqMsgSubscribe\"\
}]}";

	ws_send(ws, req, strlenof(req), 0);
	fwrite(req, 1, strlenof(req), stderr);
	return 1;
}


#include "huobi.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	static char api_url[256U] = API_URL;
	wssnarf_t wss;
	int rc = EXIT_SUCCESS;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

	/* obtain a loop */
	wss = make_wssnarf("prices");
	add_wssnarf(wss, (wssnarf_param_t){api_url, 6.0, 30.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return rc;
}

/* huobi.c ends here */
