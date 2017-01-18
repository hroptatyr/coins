#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include "wssnarf.h"
#include "nifty.h"

#define API_URL		"ws://aux1.forexfactory.com:3010/"

static const char *joinfile;


static __attribute__((format(printf, 1, 2))) void
serror(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


int
join_coin(ws_t ws)
{
	struct stat st;
	const char *jb;
	ssize_t fz;
	int fd;

	if ((fd = open(joinfile, O_RDONLY)) < 0) {
		serror("cannot open join file `%s'", joinfile);
		return -1;
	} else if (fstat(fd, &st) < 0) {
		serror("cannot stat join file `%s'", joinfile);
		goto clo_out;
	} else if ((fz = st.st_size) <= 0) {
		errno = 0, serror("join file `%s' has illegal size", joinfile);
		goto clo_out;
	} else if ((jb = mmap(NULL, fz,
			      PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		serror("cannot mmap join file `%s'", joinfile);
		goto clo_out;
	}

	for (const char *jp = jb, *const ep = jb + fz, *eol;
	     jp < ep && (eol = memchr(jp, '\n', ep - jb)); jp = eol + 1U) {
		switch (*jp) {
		case '#':
		case '\n':
			break;
		default:
			ws_send(ws, jp, eol - jp + 1U, 0);
			break;
		}
	}

	munmap(deconst(jb), fz);
	close(fd);
	return 0;

clo_out:
	close(fd);
	return -1;
}


#include "ff.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	wssnarf_t wss;

	if (yuck_parse(argi, argc, argv) < 0) {
		return EXIT_FAILURE;
	}

	/* populate join filename */
	joinfile = argi->config_arg ?: "ff.cnf";

	/* obtain a loop */
	wss = make_wssnarf("prices");
	add_wssnarf(wss, (wssnarf_param_t){API_URL, 6.0, 30.0});

	/* run the loop */
	run_wssnarf(wss);

	/* hm? */
	free_wssnarf(wss);

	/* that's it */
	yuck_free(argi);
	return EXIT_SUCCESS;
}

/* ff.c ends here */
