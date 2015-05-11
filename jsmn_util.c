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
