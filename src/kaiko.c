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

#define API_URL		"wss://markets.kaiko.com:8080/v1"


int
join_coin(ws_t ws)
{
	static const char req[] = "\
{\
  \"type\": \"subscribe\",\
  \"exchanges\": [\
    { \"name\": \"indices\", \"channels\": [\"ticker\"] },\
    { \"name\": \"bitmex\", \"channels\": [\"ticker\"] },\
    { \"name\": \"bitfinex\", \"channels\": [\"trades#btcusd\", \"orderbook#btcusd\"] },\
    { \"name\": \"bitstamp\", \"channels\": [\"trades#btcusd\", \"orderbook#btcusd\"] },\
    { \"name\": \"btcchina\", \"channels\": [\"trades#btccny\", \"orderbook#btccny\"] },\
    { \"name\": \"huobi\", \"channels\": [\"trades#btcusd,btccny\", \"orderbook#btcusd,btccny\"] }\
  ]\
}\n";

	/* just send him */
	ws_send(ws, req, strlenof(req), 0);
	fwrite(req, 1, strlenof(req), stderr);
	return 0;
}


#include "kaiko.yucc"

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

/* kaiko.c ends here */
