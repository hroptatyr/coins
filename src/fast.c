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
#define TAP_URL		"ws://tape.fastmatch.com/node/"

static const char *const *subs;
static size_t nsubs;


static int
join_quot(ws_t ws)
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
	return 1;
}

static int
join_tape(ws_t ws)
{
	static const char all[] = "\
{\"currencies\": [\
\"AUD/CAD\",\
\"AUD/CHF\",\
\"AUD/DKK\",\
\"AUD/HKD\",\
\"AUD/JPY\",\
\"AUD/NOK\",\
\"AUD/NZD\",\
\"AUD/SEK\",\
\"AUD/SGD\",\
\"AUD/USD\",\
\"AUD/ZAR\",\
\"CAD/CHF\",\
\"CAD/DKK\",\
\"CAD/HKD\",\
\"CAD/JPY\",\
\"CAD/MXN\",\
\"CAD/NOK\",\
\"CAD/PLN\",\
\"CAD/SEK\",\
\"CAD/SGD\",\
\"CAD/ZAR\",\
\"CHF/CZK\",\
\"CHF/DKK\",\
\"CHF/HKD\",\
\"CHF/HUF\",\
\"CHF/ILS\",\
\"CHF/JPY\",\
\"CHF/MXN\",\
\"CHF/NOK\",\
\"CHF/PLN\",\
\"CHF/SEK\",\
\"CHF/SGD\",\
\"CHF/TRY\",\
\"CHF/ZAR\",\
\"CZK/JPY\",\
\"DKK/HKD\",\
\"DKK/JPY\",\
\"DKK/NOK\",\
\"DKK/SEK\",\
\"EUR/AUD\",\
\"EUR/CAD\",\
\"EUR/CHF\",\
\"EUR/CZK\",\
\"EUR/DKK\",\
\"EUR/GBP\",\
\"EUR/HKD\",\
\"EUR/HUF\",\
\"EUR/ILS\",\
\"EUR/JPY\",\
\"EUR/MXN\",\
\"EUR/NOK\",\
\"EUR/NZD\",\
\"EUR/PLN\",\
\"EUR/RUB\",\
\"EUR/SEK\",\
\"EUR/SGD\",\
\"EUR/TRY\",\
\"EUR/USD\",\
\"EUR/ZAR\",\
\"GBP/AUD\",\
\"GBP/CAD\",\
\"GBP/CHF\",\
\"GBP/CZK\",\
\"GBP/DKK\",\
\"GBP/HKD\",\
\"GBP/HUF\",\
\"GBP/ILS\",\
\"GBP/JPY\",\
\"GBP/MXN\",\
\"GBP/NOK\",\
\"GBP/NZD\",\
\"GBP/PLN\",\
\"GBP/SEK\",\
\"GBP/SGD\",\
\"GBP/TRY\",\
\"GBP/USD\",\
\"GBP/ZAR\",\
\"HKD/JPY\",\
\"HUF/JPY\",\
\"MXN/JPY\",\
\"NOK/HKD\",\
\"NOK/JPY\",\
\"NOK/SEK\",\
\"NZD/CAD\",\
\"NZD/CHF\",\
\"NZD/DKK\",\
\"NZD/HKD\",\
\"NZD/JPY\",\
\"NZD/NOK\",\
\"NZD/PLN\",\
\"NZD/SEK\",\
\"NZD/SGD\",\
\"NZD/USD\",\
\"NZD/ZAR\",\
\"PLN/HUF\",\
\"PLN/JPY\",\
\"SEK/HKD\",\
\"SEK/JPY\",\
\"SGD/DKK\",\
\"SGD/HKD\",\
\"SGD/JPY\",\
\"SGD/MXN\",\
\"SGD/NOK\",\
\"SGD/SEK\",\
\"TRY/JPY\",\
\"USD/CAD\",\
\"USD/CHF\",\
\"USD/CNH\",\
\"USD/CZK\",\
\"USD/DKK\",\
\"USD/GHS\",\
\"USD/HKD\",\
\"USD/HUF\",\
\"USD/ILS\",\
\"USD/JPY\",\
\"USD/KES\",\
\"USD/MXN\",\
\"USD/NOK\",\
\"USD/PLN\",\
\"USD/RON\",\
\"USD/RUB\",\
\"USD/SEK\",\
\"USD/SGD\",\
\"USD/TRY\",\
\"USD/ZAR\",\
\"XAG/USD\",\
\"XAU/USD\",\
\"XPD/USD\",\
\"XPT/USD\",\
\"ZAR/JPY\",\
\"ZAR/MXN\" \
]}";
	ws_send(ws, all, strlenof(all), 0);
	return 1;
}

int
join_coin(ws_t ws)
{
	static size_t i;

	switch (i++) {
	case 0U:
		return join_quot(ws);
	case 1U:
		return join_tape(ws);
	default:
		break;
	}
	fputs("\
Error: more join requests than sockets\n", stderr);
	return -1;
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
	wss = make_wssnarf("prices");
	add_wssnarf(wss, (wssnarf_param_t){API_URL, 6.0, 30.0});
	add_wssnarf(wss, (wssnarf_param_t){TAP_URL, -1.0, 300.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* fast.c ends here */
