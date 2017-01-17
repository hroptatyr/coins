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
#include <curl/curl.h>
#include "wssnarf.h"
#include "nifty.h"

#define API_URL		"ws://hq.huobi.com/socket.io/1/"


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


static ssize_t
sid_cb(const void *data, const size_t nz, const size_t nm, void *clo)
{
	/* find colon */
	size_t z;

	for (z = 0U; z < nz * nm && ((const char*)data)[z] != ':'; z++);
	memcpy(clo, "websocket/", 10U);
	memcpy((char*)clo + 10U, data, z);
	return nz * nm;
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

	/* obtain a session handle first *sigh* */
	curl_global_init(CURL_GLOBAL_ALL);
	with (CURL *hndl = curl_easy_init()) {
		curl_easy_setopt(hndl, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(hndl, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(hndl, CURLOPT_WRITEFUNCTION, sid_cb);
		curl_easy_setopt(hndl, CURLOPT_WRITEDATA,
				 api_url + strlenof(API_URL));
		curl_easy_setopt(hndl, CURLOPT_USERAGENT,
				 "Dark Secret Ninja/1.0");

		curl_easy_setopt(hndl, CURLOPT_URL, api_url + 5U/*ws:...*/);
		curl_easy_perform(hndl);
		curl_easy_cleanup(hndl);
	}

	/* obtain a loop */
	wss = make_wssnarf((wssnarf_param_t){api_url, "prices"});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return rc;
}

/* huobi.c ends here */
