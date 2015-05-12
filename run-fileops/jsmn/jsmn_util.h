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
 * insufficient memory.  num_tokens is ignored.  Don't forget to reset
 * (*tokens)=NULL after freeing, before parsing again.
 *
 * Otherwise, (*tokens) must be a buffer from malloc and (*num_tokens)
 * shall be its size in sizeof(jsmntok_t).  If it is realloc(3)'d,
 * both (*tokens) and (*num_tokens) will be updated.
 *
 * Returns JSMN_ERROR_NOMEM only when malloc or realloc fail.
 */
extern jsmnerr_t jsmn_parse_realloc(jsmn_parser *parser, const char *js, size_t len,
				    jsmntok_t **tokens, size_t *num_tokens);

/**
 * Turn a parsed string fragment into a nul-terminated de-escaped C
 * string; in place or copying to a buffer.  Beware, this string could
 * have internal NUL bytes.
 *
 * @param src	The input JSON buffer.
 * @param t	Parsed token, for which we want text.
 * @param dst	Destination buffer; NULL to overwrite in-place at (src + t->start).
 * @param dstn	Size of dst; ignored in destructive mode.
 * @return Length of resulting string excluding terminating NUL, in bytes.
 *
 * If dst[dstn] is not big enough, an incomplete NUL-terminated string
 * is written.
 *
 * If there are somehow character escaping errors in the src[] text
 * which the parser allowed through, they will be passed on unchanged.
 *
 * It should decode wide characters to UTF8, but currently only
 * decodes single-byte \uNNNN values and passes wide characters
 * unchanged.
 */
extern size_t jsmn_nstr(char *src, jsmntok_t *t, char *dst, size_t dstn);

#ifdef __cplusplus
}
#endif

#endif /* __JSMN_UTIL_H_ */
