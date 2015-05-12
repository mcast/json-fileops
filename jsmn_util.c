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


size_t jsmn_nstr(char *src, jsmntok_t *t, char *dst, size_t dstn) {
  char *src_end = src + t->end;
  char *dst_start, *dst_end;
  src += t->start;

  if (!dst) {
    /* overwrite in place */
    dst = src;
    dstn = t->end - t->start + 1;
  }
  dst_end = dst + dstn - 1;
  dst_start = dst;

  for ( ; src < src_end && dst < dst_end; src++, dst++) {
    if (*src == '\\') {
      src++;
      switch (*src) {
	int chr; // signed; neg = fail
	int i;
	/* Allowed escaped symbols */
      case '\"': case '/' : case '\\' :
	*dst = *src;
	break;
      case 'b' : *dst = '\b'; break;
      case 'f' : *dst = '\f'; break;
      case 'r' : *dst = '\r'; break;
      case 'n' : *dst = '\n'; break;
      case 't' : *dst = '\t'; break;

	/* Allows escaped symbol \uXXXX */
      case 'u':
	src++;
	chr = 0;
	for(i = 0; i < 4 && src + i < src_end; i++) {
	  int val = 0;
	  /* If it isn't a hex character we have an error */
	  if      (src[i] >= 48 && src[i] <=  57) val = src[i] - '0'; /* 0-9 */
	  else if (src[i] >= 65 && src[i] <=  70) val = src[i] - 55; /* A-F */
	  else if (src[i] >= 97 && src[i] <= 102) val = src[i] - 87; /* a-f */
	  else {
	    /* Bad, but the parser passed it.  Pass through. */
	    chr = -1;
	    break;
	  }
	  chr = (chr << 4) + val;
	}
	if (chr < 0) {
	  /* Fail */
	  *dst = '\\';
	  src-=2; /* 'u' again */
	  continue;
	} else if (chr < 256) {
	  /* Plain char */
	  src += 3; /* 4 hex digits, one covered by loop incr */
	  *dst = chr;
	} else {
	  /* Wide character */
	  /* UTF8 not yet implemented.  Pretend we didn't see it. */
	  *dst = '\\';
	  src-=2; /* 'u' again */
	  continue;
	}
	break;
      default:
	/* Well the parser already passed on it, so ...? */
	*dst = '\\';
	src --;
      }
    } else {
      *dst = *src;
    }
  }
  *dst = 0;
  return dst - dst_start;
}
