#if !defined INCLUDED_ws_h_
#define INCLUDED_ws_h_
#include <stdlib.h>
#include <stdint.h>

typedef struct ws_s *ws_t;

typedef enum {
	WS_PROTO_RAW,
	WS_PROTO_WAMP,
	WS_PROTO_SIO1,
	WS_PROTO_SIO3,
	WS_PROTO_REST,
} ws_proto_t;


extern ws_t ws_open(const char *url);
extern int ws_close(ws_t);

extern ssize_t ws_recv(ws_t, void *restrict buf, size_t bsz, int flags);
extern ssize_t ws_send(ws_t, const void *buf, size_t bsz, int flags);

/* special methods */
extern int ws_ping(ws_t ws, const void *msg, size_t msz);
extern int ws_pong(ws_t ws, const void *msg, size_t msz);

extern ssize_t rest_recv(ws_t, void *restrict buv, size_t bsz, int flags);
extern ssize_t rest_send(ws_t, const char *rsrc, size_t rsrz, int flags);


/* inlines */
static inline int
ws_fd(ws_t ws)
{
	struct {
		unsigned int state;
		int sock;
	} *_s = (void*)ws;
	return _s->sock;
}

static inline ws_proto_t
ws_proto(ws_t ws)
{
	struct {
		ws_proto_t proto;
		int sock;
	} *_s = (void*)ws;
	return _s->proto;
}

#endif	/* !INCLUDED_ws_h_ */
