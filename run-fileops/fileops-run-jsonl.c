#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include "jsmn/jsmn.h"
#include "jsmn/jsmn_util.h"

/*
 * fileops-run-jsonl --toffset 4564.23 --vfd  < foo.fileops.jsonl
 */

const char *PROGNAME = "fileops-run-jsonl";

#define MAX_VFD_FILTER 1024


void bail(char* msg) {
  fprintf(stderr, "%s: %s\n", PROGNAME, msg);
  exit(2);
}

void jsmn_typebail(int lnum, jsmntype_t want, jsmntype_t got, char *field) {
  if (want != got) {
    const char *w = jsmntype_str(want), *g = jsmntype_str(got);
    fprintf(stderr, "%s: stdin:%d: field %s wanted %s, got %s\n",
	    PROGNAME, lnum, field, w, g);
    exit(6);
  }
}

double _atod(char *a, char **endptr) {
  char *endloc = NULL;
  double val;
  errno = 0;
  val = strtod(a, &endloc);
  if (endloc == a || errno != 0) {
    fprintf(stderr, "%s: bad double %s\n", PROGNAME, a);
    exit(4);
  }
  if (endptr) {
    *endptr = endloc;
  } else {
    if (*endloc) {
      fprintf(stderr, "%s: junk left after double %s\n", PROGNAME, a);
      exit(4);
    }
  }
  return val;
}

long _atoi(char *a, char **endptr) {
  char *endloc = NULL;
  long val;
  errno = 0;
  val = strtol(a, &endloc, 10);
  if (errno) {
    fprintf(stderr, "%s: bad int %s\n", PROGNAME, a);
    exit(4);
  }
  if (endptr) {
    *endptr = endloc;
  } else {
    if (*endloc) {
      fprintf(stderr, "%s: junk left after int %s\n", PROGNAME, a);
      exit(4);
    }
  }
  return val;
}

bool vfd_filter_or(unsigned char *f, int offset, int or) {
  int idx = offset / 8;
  int mask = 1 << (offset % 8);
  if (or) f[idx] = f[idx] | (or * mask);
  return f[idx] & mask ? 1 : 0;
}


int main(int argc, char **argv) {
  double toffset = 0;
  unsigned char vfd_filter[MAX_VFD_FILTER/8]; // an array of bits
  int max_vfd = 0;
  int i;
  int debug = 0;

  size_t linebuf_sz = 2048;  // may grow with a realloc
  char *linebuf = malloc(linebuf_sz);
  int lnum = 0;

  if (!linebuf) bail("out of memory (linebuf)");

  /* Parse argv */
  for (i=1; i<argc; i++) {
    if (!strcmp(argv[i], "--toffset") || !strcmp(argv[i], "-T")) {
      i++;
      if (i == argc) bail("--toffset requires an offset, in fractions of a second");
      toffset = _atod(argv[i], NULL);

    } else if (!strcmp(argv[i], "--vfd") || !strcmp(argv[i], "-f")) {
      char *p;
      i++;
      if (i == argc) bail("--vfd requires int(s), comma-separated");
      p = argv[i];
      while (*p) {
	int vfd = _atoi(p, &p);
	if (vfd < 0) bail("--vfd requires non-negative numbers");
	if (vfd >= MAX_VFD_FILTER) bail("--vfd exceeds MAX_VFD_FILTER");
	if (vfd > max_vfd) max_vfd = vfd;
	vfd_filter_or(vfd_filter, vfd, 1);
	if (*p == ',') p++;
      }

    } else if (!strcmp(argv[i], "--debug") || !strcmp(argv[i], "-d")) {
      debug ++;

    } else {
      bail("bad option");
    }
  }

  while (!feof(stdin)) {
    jsmn_parser p;
    jsmntok_t t[128];
    int r, k;

    /* fileop object props */
    char *Op = NULL, *fn = NULL, *operr = NULL;
    int vfd = 0; // vfd=0 : strace2jsonl.pl leaves this default when err
    ssize_t byte_from = -1, byte_to = -1;
    double elapsed = -1, T = -1;

    /* Read a jsonl line */
    lnum ++;
    ssize_t llen = getline(&linebuf, &linebuf_sz, stdin);
    if (llen < 0) bail("incomplete line");

    /* Tokenise, expecting an object */
    jsmn_init(&p);
    r = jsmn_parse(&p, linebuf, llen, t, sizeof(t)/sizeof(t[0]));
    if (r < 0) {
      fprintf(stderr, "%s: stdin:%d: Failed to parse JSON (err %d): %s",
	      PROGNAME, lnum, r, linebuf);
      exit(6);
    }
    if (r < 1 || t[0].type != JSMN_OBJECT) {
      fprintf(stderr, "%s: stdin:%d: Expected object, got %s",
	      PROGNAME, lnum, linebuf);
      exit(6);
    }

    /* Deal with a fileops objeect */
    if (debug) printf("stdin:%d: %s", lnum, linebuf);
    for(i=1, k=t[0].size; i<r && k>0; i += t[i].size + t[i+1].size + 1, k--) {
      char *keyname = linebuf + t[i].start, *val = linebuf + t[i+1].start;
      linebuf[ t[i].end ] = 0;
      linebuf[ t[i+1].end ] = 0;

      if (debug) {
	printf("i=%d, k=%d: %d (%s) sz=%d parent=%d\n\t%d (%s) sz=%d parent=%d\n",
	       i, k,
	       t[i].type,   keyname, t[i].size,   t[i].parent,
	       t[i+1].type, val,     t[i+1].size, t[i+1].parent);
      }

      jsmn_typebail(lnum, JSMN_STRING, t[i].type, "(key)");
      if (!strcmp(keyname, "vfd")) {
	jsmn_typebail(lnum, JSMN_PRIMITIVE, t[i+1].type, "vfd");
	vfd = _atoi(val, NULL);
	if (max_vfd) {
	  if (vfd > max_vfd || !vfd_filter_or(vfd_filter, vfd, 0)) {
	    if (debug) printf("skip (vfd=%d)\n", vfd);
	    vfd = -1;
	    break;
	  }
	}
      } else if (!strcmp(keyname, "Op")) {
	jsmn_typebail(lnum, JSMN_STRING, t[i+1].type, "Op");
	Op = val;
      } else if (!strcmp(keyname, "fn")) {
	jsmn_typebail(lnum, JSMN_STRING, t[i+1].type, "fn");
	fn = val;
      } else if (!strcmp(keyname, "err")) {
	jsmn_typebail(lnum, JSMN_STRING, t[i+1].type, "err");
	operr = val;
      } else if (!strcmp(keyname, "elapsed")) {
	jsmn_typebail(lnum, JSMN_PRIMITIVE, t[i+1].type, "elapsed");
	elapsed = _atod(val, NULL);
      } else if (!strcmp(keyname, "T")) {
	jsmn_typebail(lnum, JSMN_PRIMITIVE, t[i+1].type, "T");
	T = _atod(val, NULL);
      } else if (!strcmp(keyname, "_op")) {
	// ignore
      } else if (!strcmp(keyname, "bytes")) {
	jsmn_typebail(lnum, JSMN_ARRAY, t[i+1].type, "bytes");
	if (t[i+1].size != 2) {
	  fprintf(stderr, "%s: stdin:%d: bytes field array, expected size 2 got size %d\n",
		  PROGNAME, lnum, t[i+1].size);
	  exit(6);
	}
	jsmn_typebail(lnum, JSMN_PRIMITIVE, t[i+2].type, "bytes[0]");
	jsmn_typebail(lnum, JSMN_PRIMITIVE, t[i+3].type, "bytes[1]");
	linebuf[ t[i+2].end ] = 0;
	linebuf[ t[i+3].end ] = 0;
	byte_from = _atoi(linebuf + t[i+2].start, NULL);
	byte_to   = _atoi(linebuf + t[i+3].start, NULL);
      } else {
	    fprintf(stderr, "%s: stdin:%d: unexpected key %s\n", PROGNAME, lnum, keyname);
	exit(6);
      }
    }
    if (vfd < 0) continue; // excluded by vfd_filter

    T -= toffset;

    printf("Op = %s\n  T = %f\n  elapsed = %f\n  vfd = %d\n", Op, T, elapsed, vfd);
    if (fn) printf("  fn=%s\n", fn);
    if (operr) printf("  err=%s\n", operr);
    if (byte_from >= 0) printf("  bytes = [%ld .. %ld]\n", byte_from, byte_to);
    putchar(10);
  }

  return 0;
}
