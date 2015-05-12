#include <stdlib.h>

#include "jsmn.h"
#include "jsmn_util.h"


static const char *_jsmntype_str[] =
  { "JSMN_INVALID_TYPE", "JSMN_PRIMITIVE", "JSMN_OBJECT", "JSMN_ARRAY", "JSMN_STRING" };

const char *jsmntype_str(jsmntype_t t) {
  if (t == JSMN_PRIMITIVE || t == JSMN_OBJECT || t == JSMN_ARRAY || t == JSMN_STRING) {
    return _jsmntype_str[(int)t + 1];
  } else {
    return _jsmntype_str[0];
  }
}


static const char *_jsmnerr_str[] =
  { "JSMN_ERROR_OK", "JSMN_ERROR_UNKNOWN", "JSMN_ERROR_NOMEM", "JSMN_ERROR_INVAL", "JSMN_ERROR_PART" };

const char *jsmnerr_str(jsmnerr_t r) {
  if (r >= 0) {
    return _jsmnerr_str[0];
  } else if (r == JSMN_ERROR_NOMEM || r == JSMN_ERROR_INVAL || r == JSMN_ERROR_PART) {
    return _jsmnerr_str[1-r];
  } else {
    return _jsmnerr_str[1];
  }
}


jsmnerr_t jsmn_parse_realloc(jsmn_parser *parser, const char *js, size_t len,
			     jsmntok_t **tokens, size_t *num_tokens) {
  size_t tmp_numtok = 128;
  jsmnerr_t r;

  if (*tokens == NULL) {
    num_tokens = &tmp_numtok;
    *tokens = malloc((*num_tokens) * sizeof((*tokens)[0]));
    if (! *tokens) return JSMN_ERROR_NOMEM;
  }

  while(1) {  // break inside
    jsmn_init(parser);
    r = jsmn_parse(parser, js, len, *tokens, *num_tokens);
    if (r == JSMN_ERROR_NOMEM) {
      unsigned int want_num_tokens = 2 * (*num_tokens);
      jsmntok_t *newtok = realloc(*tokens, want_num_tokens * sizeof((*tokens)[0]));
      if (!newtok) break;
      *num_tokens = want_num_tokens;
      *tokens = newtok;
    } else {
      break;
    }
  }
  return r;
}
