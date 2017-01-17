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

#define API_URL		"wss://ws.bitso.com"


int
join_coin(ws_t ws)
{
	static const char ord_btc[] = "{\
\"action\":\"subscribe\",\
\"book\":\"btc_mxn\",\
\"type\":\"orders\"\
}\r\n";
	static const char odd_btc[] = "{\
\"action\":\"subscribe\",\
\"book\":\"btc_mxn\",\
\"type\":\"diff-orders\"\
}\r\n";
	static const char tra_btc[] = "{\
\"action\":\"subscribe\",\
\"book\":\"btc_mxn\",\
\"type\":\"trades\"\
}\r\n";
	static const char ord_eth[] = "{\
\"action\":\"subscribe\",\
\"book\":\"eth_mxn\",\
\"type\":\"orders\"\
}\r\n";
	static const char odd_eth[] = "{\
\"action\":\"subscribe\",\
\"book\":\"eth_mxn\",\
\"type\":\"diff-orders\"\
}\r\n";
	static const char tra_eth[] = "{\
\"action\":\"subscribe\",\
\"book\":\"eth_mxn\",\
\"type\":\"trades\"\
}\r\n";

	/* just send him */
	ws_send(ws, tra_btc, strlenof(tra_btc), 0);
	fwrite(tra_btc, 1, strlenof(tra_btc), stderr);

	ws_send(ws, ord_btc, strlenof(ord_btc), 0);
	fwrite(ord_btc, 1, strlenof(ord_btc), stderr);

	ws_send(ws, odd_btc, strlenof(odd_btc), 0);
	fwrite(odd_btc, 1, strlenof(odd_btc), stderr);

	ws_send(ws, tra_eth, strlenof(tra_eth), 0);
	fwrite(tra_eth, 1, strlenof(tra_eth), stderr);

	ws_send(ws, ord_eth, strlenof(ord_eth), 0);
	fwrite(ord_eth, 1, strlenof(ord_eth), stderr);

	ws_send(ws, odd_eth, strlenof(odd_eth), 0);
	fwrite(odd_eth, 1, strlenof(odd_eth), stderr);
	return 1;
}


#include "bitso.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	wssnarf_t wss;

	if (yuck_parse(argi, argc, argv) < 0) {
		return 1;
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

/* bitso.c ends here */
