#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "tls.h"
#include "nifty.h"

static SSL_CTX *sslctx;


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

static int
put_sockaddr(struct sockaddr_in *sa, const char *name, uint16_t port)
{
	struct addrinfo hints;
	struct addrinfo *res;

	memset(sa, 0, sizeof(*sa));
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(name, NULL, &hints, &res)) {
		return -1;
	}
	*sa = *(struct sockaddr_in*)res->ai_addr;
	sa->sin_port = htons(port);
	freeaddrinfo(res);
	return 0;
}


/* public api */
ssl_ctx_t
conn_tls(int s, const char *host)
{
	ssl_ctx_t this;
	int rc;

	(void)SSL_library_init();
	if (UNLIKELY(sslctx == NULL &&
		     (sslctx = SSL_CTX_new(TLSv1_2_client_method())) == NULL)) {
		return NULL;
	} else if (UNLIKELY((this = SSL_new(sslctx)) == NULL)) {
		return NULL;
	}
	SSL_set_tlsext_host_name(this, host);
	SSL_set_fd(this, s);
	if (UNLIKELY((rc = SSL_connect(this)) < 1)) {
		serror("\
Error: SSL connection failed, code %d", SSL_get_error(this, rc));
		goto fre;
	}
	if (UNLIKELY((rc = SSL_do_handshake(this)) < 1)) {
		serror("\
Error: SSL handshake failed, code %d", SSL_get_error(this, rc));
		goto fre;
	}
	return this;

fre:
	SSL_free(this);
	return NULL;
}

ssl_ctx_t
open_tls(const char *host, short unsigned int port)
{
	struct sockaddr_in sa;
	ssl_ctx_t this;
	int s;

	if (UNLIKELY(put_sockaddr(&sa, host, port) < 0)) {
		return NULL;
	}
	if (UNLIKELY((s = socket(PF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)) {
		return NULL;
	}
	if (UNLIKELY(connect(s, (void*)&sa, sizeof(sa)) < 0)) {
		goto clo;
	}
	if (UNLIKELY((this = conn_tls(s, host)) == NULL)) {
		goto clo;
	}
	return this;

clo:
	close(s);
	return NULL;
}

int
close_tls(ssl_ctx_t c)
{
	if (LIKELY(c != NULL)) {
		int s = tls_fd(c);

		SSL_shutdown(c);
		SSL_free(c);

		if (LIKELY(s >= 0)) {
			close(s);
		}
	}
	return 0;
}

/* tls.c ends here */
