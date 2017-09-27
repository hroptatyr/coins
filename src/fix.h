#if !defined INCLUDED_fix_h_
#define INCLUDED_fix_h_
#include <stdint.h>

#define NUL	"\000"
#if !defined SOH
# define SOH	"\001"
#endif	/* !SOH */

#if !defined CHAR_BITS
# define CHAR_BITS	8U
#endif	/* CHAR_BITS */

typedef struct {
#define UINTIFY_TYP(x)	((uint8_t)(x))
	uint64_t typ:8U;
	uint64_t len:56U;
	const char *msg;
} fix_msg_t;


extern void fix_reset(void);
extern void fix_set_tcomp_id(const char *tcomp_id, size_t ntcomp_id);
extern void fix_set_scomp_id(const char *scomp_id, size_t nscomp_id);
extern size_t fix_render(char *restrict buf, size_t bsz, fix_msg_t msg);
extern size_t fix_render_tm(char *restrict buf, size_t bsz);
extern fix_msg_t fix_parse(char *restrict buf, size_t bsz);
 
#endif	/* INCLUDED_fix_h_ */
