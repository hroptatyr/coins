#if !defined INCLUDED_wssnarf_h_
#define INCLUDED_wssnarf_h_
#include "ws.h"

typedef struct wssnarf_s *wssnarf_t;

typedef struct {
	const char *url;
	const char *ofn;
} wssnarf_param_t;


extern wssnarf_t make_wssnarf(wssnarf_param_t);
extern void free_wssnarf(wssnarf_t);
extern int run_wssnarf(wssnarf_t);


/* to be implemented by clients */
/* if <0 auth should be considered unsuccessful,
 * if 0 auth must wait for a response
 * if 1 auth was successful */
extern int auth_coin(ws_t);
/* if <0 join should be considered unsuccessful,
 * if 0 join must wait for a response
 * if 1 join was successful */
extern int join_coin(ws_t);

#endif	/* INCLUDED_wssnarf_h_ */
