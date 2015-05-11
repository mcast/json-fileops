#ifndef __JSMN_UTIL_H_
#define __JSMN_UTIL_H_

#include <stddef.h>
#include "jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * JSON type identifier names.
 *
 *   char *got_type = jsmntype_str(tok[i].type);
 */
extern const char *jsmntype_str(jsmntype_t t);

/**
 * Jsmn error codes as text.
 *
 *   jsmnerr_t r = jsmn_parse(...);
 *   if (r < 0) {
 *     fprintf(stderr, "parse failed: %s\n", jsmnerr_str(r));
 *     break;
 *   }
 */
extern const char *jsmnerr_str(jsmnerr_t r);


#ifdef __cplusplus
}
#endif

#endif /* __JSMN_UTIL_H_ */
