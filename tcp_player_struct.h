 /*
 * tcp_player_struct.h
 *
 *  Ripped off: Jun 3, 2014
 *      Author: wibble
 *      Thief: SMH
 *
 */

#ifndef EPP_STRUCT_H_
#define EPP_STRUCT_H_

#include "defaults.h"


/* Program options */
struct player_opt {
  int revbufsize;
  int maxacq;
  unsigned int ports[MAXPORTS];
  int num_ports;

  int nchan;
  bool chans_are_synchr;
  
  char *prefix;
  char *outdir;
  unsigned int sleeptime;
  
  bool digitizer_data;
  int rtdsize;
  char *rtdfile;
  double dt;
  int rtdavg;
  
  int runmode;

  bool debug;
  bool verbose;
  bool diag;
};

struct tcp_player_ptargs {
  struct player_opt o;

  time_t time;
  bool *running;
  int retval;
  unsigned int port;

  short int *rtdframe[MAX_NUMCHANS];
  pthread_mutex_t *rlock[MAX_NUMCHANS];
};

/* define the header structure for monitor file */
struct header_info {
  int hkey;
  char site_id[12];
  int num_channels;
  char channel_flags;
  unsigned int num_samples;
  unsigned int num_read;
  unsigned int averages;
  float sample_frequency;
  float time_between_acquisitions;
  int byte_packing;
  time_t start_time;
  struct timeval start_timeval;
  float code_version;
};

struct prtd_header_info {
	char site_id[12];
	int num_channels;
	char channel_flags;
	unsigned int num_samples;
	unsigned int num_read;
	float sample_frequency;
	float time_between_acquisitions;
	int byte_packing;
	time_t start_time;
	struct timeval start_timeval;
	float code_version;
};


union rtd_h_union {
  struct header_info cprtd;
  struct prtd_header_info prtd;
};

#endif /* EPP_STRUCT_H_ */
