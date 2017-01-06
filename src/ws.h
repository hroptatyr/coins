#if !defined INCLUDED_ws_h_
#define INCLUDED_ws_h_
#include <stdlib.h>
#include <stdint.h>

typedef struct ws_s *ws_t;


extern ws_t ws_open(const char *url);
extern ws_t wamp_open(const char *url);
extern int ws_close(ws_t);

extern ssize_t ws_recv(ws_t, void *restrict buf, size_t bsz, int flags);
extern ssize_t ws_send(ws_t, const void *buf, size_t bsz, int flags);

/* special methods */
extern int ws_ping(ws_t ws);
extern int ws_pong(ws_t ws);


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

static inline int
ws_more_p(ws_t ws)
{
	struct {
		unsigned int more;
		int sock;
	} *_s = (void*)ws;
	return _s->more;
}

#endif	/* !INCLUDED_ws_h_ */
