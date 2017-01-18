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

#define API_URL		"wss://api.gemini.com/v1/marketdata/"
#define API_PAR		"xxxxxx?heartbeat=true"

static char logfile[] = "xxxxxx";


static __attribute__((format(printf, 1, 2))) void
serror(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputs(": ", stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


#include "gemini.yucc"

int
main(int argc, char *argv[])
{
	static char api_url[] = API_URL API_PAR;
	static yuck_t argi[1U];
	wssnarf_t wss;
	int rc = EXIT_SUCCESS;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	} else if (!argi->nargs) {
		errno = 0, serror("\
Error: need an instrument to subscribe to, one of btcusd, ethusd, ethbtc");
		rc = EXIT_FAILURE;
		goto out;
	} else if (strlen(*argi->args) != 6U) {
		errno = 0, serror("\
Error: instrument must be one of btcusd, ethusd, ethbtc");
		rc = EXIT_FAILURE;
		goto out;
	}
	/* otherwise memcpy him into logfile array */
	memcpy(logfile, *argi->args, 6U);
	/* and fill in the xxxx in the API_URL */
	memcpy(api_url + strlenof(API_URL), logfile, 6U);

	/* obtain a loop */
	wss = make_wssnarf(logfile);
	add_wssnarf(wss, (wssnarf_param_t){api_url, 6.0, 30.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

out:
	/* that's it */
	yuck_free(argi);
	return rc;
}

/* gemini.c ends here */
