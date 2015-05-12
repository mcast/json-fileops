#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include "jsmn/jsmn.h"

/*
 * fileops-run-jsonl --toffset 4564.23 --vfd  < foo.fileops.jsonl
 */

const char *PROGNAME = "fileops-run-jsonl";

#define MAX_VFD_FILTER 1024


void bail(char* msg) {
  fprintf(stderr, "%s: %s\n", PROGNAME, msg);
  exit(2);
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
    int r;

    /* fileop object props */
    int vfd = 0; // vfd=0 : strace2jsonl.pl leaves this default when err

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

    if (debug) printf("stdin:%d: %s", lnum, linebuf);
    for(i=0; i<r; i++) {
      char old = linebuf[ t[i].end ];
      linebuf[ t[i].end ] = 0;
      printf("%d: %d (%s) sz=%d parent=%d\n", i, t[i].type, linebuf + t[i].start, t[i].size, t[i].parent);
      linebuf[ t[i].end ] = old;
    }
    putchar(10);
  }

  return 0;
}
