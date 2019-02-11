#if !defined INCLUDED_wssnarf_h_
#define INCLUDED_wssnarf_h_
#include <stdlib.h>
#include "ws.h"

#define TIMEOUT		6.0

typedef struct wssnarf_s *wssnarf_t;

typedef struct {
	const char *url;
	double heartbeat;
	double max_inact;
	unsigned int inflate:1U;
} wssnarf_param_t;


extern wssnarf_t make_wssnarf(const char *logfile);
extern void free_wssnarf(wssnarf_t);
extern int run_wssnarf(wssnarf_t);

extern int wssnarf_log(wssnarf_t, const char *msg, size_t msz);

/* to add another socket */
extern int add_wssnarf(wssnarf_t, wssnarf_param_t);


/* to be implemented by clients */
/* if <0 connection should be considered unsuccessful,
 * if 0 connection must wait for a response
 * if >0 connection was successful */
extern int conn_coin(ws_t);

/* to be implemented by clients */
/* if <0 auth should be considered unsuccessful,
 * if 0 auth must wait for a response
 * if >0 auth was successful */
extern int auth_coin(ws_t);

/* if <0 join should be considered unsuccessful,
 * if 0 join must wait for a response
 * if >0 join was successful */
extern int join_coin(ws_t);

/* will be called when in state CONN
 * if <0 connection is considered a failure
 * if 0 connection will stay in state CONN
 * if >0 connection will be deemed connected */
extern int connd_coin(const char *rsp, size_t rsz);

/* will be called when in state AUTH
 * if <0 authentication is considered a failure
 * if 0 connection will stay in state AUTH
 * if >0 connection will be deemed authenticated */
extern int authd_coin(const char *rsp, size_t rsz);

/* will be called when in state JOIN
 * if <0 joining is considered a failure
 * if 0 connection will stay in state JOIN
 * if >0 joining will be deemed successful */
extern int joind_coin(const char *rsp, size_t rsz);

/* will be called every TIMEOUT seconds */
extern int heartbeat(ws_t);

/* will be called with every complete message buffer */
extern size_t massage(char *restrict buf, size_t bsz);

#endif	/* INCLUDED_wssnarf_h_ */
