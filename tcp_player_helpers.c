/*
 * tcp_player_helpers.c
 *
 *  Ripped off: Early June (?), 2014
 *      Author: wibble
 *       Thief: SMH
 *
 */

#define _GNU_SOURCE

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
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
  o->num_ports = 1;
  //  o->oldsport = false;
  o->prefix = DEF_PREFIX;
  o->outdir = DEF_OUTDIR;
  
  o->digitizer_data = DEF_DIGITDATA;
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
  
  while (-1 != (c = getopt(argc, argv, "A:x:p:P:o:s:gR:m:d:a:vVh"))) {
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
	options->num_ports = i+1;
      }
      break;
    case 'P':
      options->prefix = optarg;
      break;
    case 'o':
      options->outdir = optarg;
      break;      
    case 's':
      options->sleeptime = strtoul(optarg, NULL, 0);
      break;
    case 'g':
      options->digitizer_data = true;
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
      printf("\t-s <#>\tSet microseconds to sleep between acquisitions [Default: %u]\n", DEF_SLEEPTIME);
      printf("\n");
      printf("\t-g Digitizer data (Real data, excludes search for Dartmouth headers) [Default: %i]\n", DEF_DIGITDATA);
      printf("\t-R <#>\tReal-time display output size (in words) [%i].\n", DEF_RTDSIZE);
      printf("\t-m <s>\tReal-time display file [%s].\n", DEF_RTDFILE);
      printf("\t-d <#>\tReal-time display output period [%i].\n", DEF_RTD_DT);
      printf("\t\t(Output period of \"0\" --> No real-time display.)\n");
      printf("\t-a <#>\tNumber of RTD blocks to average [%i].\n", DEF_RTDAVG);
      printf("\n");
      printf("RUN MODES:\n");
      printf("\tFOR STRIPPING PACKETS\n");
      printf("\t0: No stripping of data is done. Prints packet headers to stdout.\n");
      printf("\t1: The packet header and footer are stripped from the data for RTD, but left in the data file\n");
      printf("\t2: The packet header and footer are stripped from the data for RTD AND the saved data file\n");
      printf("\t3: Stripped data are saved and RTDed, and bad packets are output to an error file, badpack.data (NOT YET IMPLEMENTED)\n");
      printf("\n");
      printf("\tFOR DOING CHANNEL TRICKERY\n");
      printf("\t4: Channel information is parsed and printed to stdout, but no files are created.\n");
      printf("\t5: A data file is created for each channel.\n");
      printf("\t6: A data file is created for each channel, AND the first and second channel are combined with"
	     "\n\t   join_upper10_lower6() and outputted as joinupper10lower6.data.\n");
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


/* void *parse_tcp_header(struct tcp_header *header, char *search_bytes, size_t search_length) { */

/*   //  printf("tcp_header is %li bytes large\n", sizeof(struct tcp_header) ); */

/*   int header_length = 40; */

/*   char skip_str[16] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,  */
/*   			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };   */

/*   //  char start_str[8] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }; */

/*   void *check_addr = memmem(search_bytes, search_length, skip_str, 16); */

/*   if( check_addr != NULL ){ */
/*     memcpy(header, check_addr+8, header_length); */
/*     return check_addr + 8; */
/*   } */
/*   /\* else { *\/ */
/*   /\*   printf("check_addr is NULL\n"); *\/ */
/*   /\* } *\/ */

/*   //Return location of header, not footer that comes 8 bytes before it--that is, add 8. */
/*   return check_addr; */
/* } */

/* int print_tcp_header(struct tcp_header *header){ */

/*   printf("TCP header start string =\t\t"); */
/*   for (int i = 0; i < 8; i ++){ */
/*     printf("%x",header->start_str[i]); */
/*   } */
/*   printf("\n"); */
/*   printf("Packet size:\t\t%"PRIu32"\n", header->pack_sz); */
/*   printf("Packet type:\t\t%"PRIu32"\n", header->pack_type); */
/*   printf("Packet number of samples:\t%"PRIu32"\n", header->pack_numsamps); */
/*   printf("Total samples sent so far:\t%"PRIu64"\n", header->pack_totalsamps); */
/*   printf("Packet time:\t\t%f\n", header->pack_time); */
/*   printf("Sync channel num samples:\t%"PRIu32"\n", header->sync_numsamps); */
/*   return EXIT_SUCCESS; */
/* } */
