#if !defined INCLUDED_tls_h_
#define INCLUDED_tls_h_
#include <stdlib.h>
#include <sys/types.h>
#include <openssl/ssl.h>

typedef void *ssl_ctx_t;


extern ssl_ctx_t open_tls(const char *host, short unsigned int port);
extern int close_tls(ssl_ctx_t);

extern size_t hmac(char *restrict buf, size_t bsz,
		   const char *msg, size_t len,
		   const char *key, size_t ksz);

extern ssl_ctx_t conn_tls(int s);


static inline ssize_t
tls_send(ssl_ctx_t c, const void *buf, size_t len, int flags)
{
	(void)flags;
	return SSL_write(c, buf, len);
}

static inline ssize_t
tls_recv(ssl_ctx_t c, void *restrict buf, size_t len, int flags)
{
	(void)flags;
	return SSL_read(c, buf, len);
}

static inline int
tls_fd(ssl_ctx_t c)
{
	return SSL_get_fd(c);
}

static inline int
tls_errno(ssl_ctx_t c, ssize_t rw)
{
	return SSL_get_error(c, rw);
}

#endif	/* INCLUDED_tls_h_ */
