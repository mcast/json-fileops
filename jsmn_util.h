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

/**
 * Reallocating jsmn_parse wrapper; like getline(3).  This currently
 * operates in a naive double-on-fail way, and so it calls
 * jsmn_init(parser) for you.
 *
 * If (*tokens) is NULL, jsmn_parse_realloc will malloc a buffer which
 * should be freed by the caller; unless it remains NULL due to
 * insufficient memory.  num_tokens is ignored.
 *
 * Otherwise, (*tokens) must be a buffer from malloc and (*num_tokens)
 * shall be its size in sizeof(jsmntok_t).  If it is realloc(3)'d,
 * both (*tokens) and (*num_tokens) will be updated.
 *
 * Returns JSMN_ERROR_NOMEM only when malloc or realloc fail.
 */
extern jsmnerr_t jsmn_parse_realloc(jsmn_parser *parser, const char *js, size_t len,
				    jsmntok_t **tokens, size_t *num_tokens);


#ifdef __cplusplus
}
#endif

#endif /* __JSMN_UTIL_H_ */
