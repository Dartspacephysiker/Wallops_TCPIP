/*
 * tcp_player_helpers.c
 *
 *  Ripped off: Early June (?), 2013
 *      Author: wibble
 *       Thief: SMH
 *
 */

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <time.h>

#include "tcp_player_helpers.h"
#include "defaults.h"

void printe(char *format, ...) {
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	fflush(stderr);
	va_end(args);
}

void init_opt(struct player_opt *o) {
  memset(o, 0, sizeof(struct player_opt));
  o->revbufsize = DEF_REVBUFSIZE;
  o->ports[0] = DEF_PORT;
  o->num_files = 1;
  o->oldsport = false;
  o->prefix = DEF_PREFIX;
  o->outdir = DEF_OUTDIR;
  
  o->rtdsize = DEF_RTDSIZE;
  o->rtdfile = DEF_RTDFILE;
  o->dt = DEF_RTD_DT;
  o->rtdavg = DEF_RTDAVG;

  o->maxacq = 0;
  
  o->debug = false;
  o->verbose = false;
}

int parse_opt(struct player_opt *options, int argc, char **argv) {
  unsigned int ports[MAXPORTS];
  char *pn;
  int c, i = 0;
  
  while (-1 != (c = getopt(argc, argv, "A:x:p:o:OP:S:C:R:m:rd:a:XvVh"))) {
    switch (c) {
    case 'A':
      options->revbufsize = strtoul(optarg, NULL, 0);
      break;
    case 'x':
      options->maxacq = strtoul(optarg, NULL, 0);
      break;
    case 'p':
      pn = strtok(optarg, ",");
      ports[i] = strtoul(pn, NULL, 0);
      while ((pn = strtok(NULL , ",")) != NULL) {
	i++;
	ports[i] = strtoul(pn, NULL, 0);
      }
      int j;
      for (j = 0; j <= i; j++) {
	options->ports[j] = ports[j];
	options->num_files = i+1;
      }
      break;
    case 'P':
      options->prefix = optarg;
      break;
    case 'o':
      options->outdir = optarg;
      break;      
    case 'R':
      options->rtdsize = strtoul(optarg, NULL, 0);
      break;
    case 'm':
      options->rtdfile = optarg;
      break;
    case 'd':
      options->dt = strtod(optarg, NULL);
      break;
    case 'a':
      options->rtdavg = strtoul(optarg, NULL, 0);
      break;
    case 'v':
      options->verbose = true;
      break;
    case 'V':
      options->debug = true;
      break;
    case 'h':
    default:
      printf("tcp_player: Acquire and optionally setup RTD for TCP/IP data.\n\n Options:\n");
      printf("\t-A <#>\tReceive buffer size [Default: %u].\n", DEF_REVBUFSIZE);
      printf("\t-x <#>\tMax <revbufsize>-byte acquisitions [Inf].\n");
      printf("\t-p <#>\tPort list (see below) [%u].\n", DEF_PORT);
      printf("\t\tCan either give a single port, or a comma-separated list.\n");
      printf("\t\ti.e., \"8000\", \"5000,7000\"\n");
      printf("\t-P <s>\tSet output filename prefix [%s].\n", DEF_PREFIX);
      printf("\t-o <s>\tSet output directory [%s].\n", DEF_OUTDIR);
      printf("\n");
      printf("\t-R <#>\tReal-time display output size (in words) [%i].\n", DEF_RTDSIZE);
      printf("\t-m <s>\tReal-time display file [%s].\n", DEF_RTDFILE);
      printf("\t-d <#>\tReal-time display output period [%i].\n", DEF_RTD_DT);
      printf("\t\t(Output period of \"0\" --> No real-time display.)\n");
      printf("\t-a <#>\tNumber of RTD blocks to average [%i].\n", DEF_RTDAVG);
      printf("\n");
      printf("\t-v Be verbose.\n");
      printf("\t-V Print debug-level messages.\n");
      printf("\t-h Display this message.\n");
      exit(1);
    }
    
  }
  
  return argc;
}

/* qsort int comparison function */
int int_cmp(const void *a, const void *b)
{
  const int *ia = (const int *)a; // casting pointer types
  const int *ib = (const int *)b;
  return *ia  - *ib;
  /* integer comparison: returns negative if b > a
     and positive if a > b */
}

