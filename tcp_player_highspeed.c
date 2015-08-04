/* tcp_player_highspeed
 *
 * 2015/08/03 
 * -> The high-speed solution to our TCP/IP woes. 
 * -> In more detail, this program is being created because tcp_player is so boggy,
 *     and I have no idea how much of that code is just kruft. Time to sweep all that
 *     away and do a clean rewrite—hence, tcp_player_highspeed. On va voire Qu'est
 *     ce qui se passe.
 *
 *
 *    Auteur: SMH
 *
 *
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>
#include <termios.h>

#include "tcp_player_highspeed.h"
#include "tcp_player_helpers.h"
#include "simple_fifo.h"

#define EEPP_FILE 8
#define EEPP_THREAD 9
#define MIN_BYTES_READ 100
#define TCP_BACKLOG 10

static bool running = true;


int main (int argc, char **argv)
{
  
  struct player_opt o;
  
  init_opt_hs(&o);
  parse_opt_hs(&o, argc, argv);
  
  /* This makes C-c work as an interrupt to quit the program */
  signal(SIGINT, do_depart);
  tcp_play_hs(o);
  
}

void tcp_play_hs(struct player_opt o)
{
  
  time_t pg_time;
  int tret, ret, rtdsize = 0;
  struct tcp_player_ptargs *thread_args;
  pthread_t *data_threads;
    
  int num_rtds = o.num_ports * o.nchan;
  short int **rtdframe_pp = NULL, *rtdout = NULL;
  pthread_mutex_t *rtdlocks;

  
  union rtd_h_union rtdh;
  //  struct header_info header;

  int rfd, active_threads = 0;
  char *rmap = NULL;
  struct stat sb;

  double telapsed;
  struct timeval now, then;

  pg_time = time(NULL);
  
  if( (o.verbose) || (o.debug) ) printf("Number of ports: %i\n",o.num_ports);
  if(o.sleeptime) printf("Sleeping %u microsec between acquisitions...\n",o.sleeptime);

  data_threads = malloc(o.num_ports * sizeof(pthread_t));
  thread_args = malloc(o.num_ports * sizeof(struct tcp_player_ptargs));

  rtdlocks = malloc(num_rtds*sizeof(pthread_mutex_t));
  rtdframe_pp = malloc(num_rtds*sizeof(short int *));
  //  rtdframe_pp = malloc(o.num_ports * sizeof(short int *));

  if (o.dt > 0) {
    printf("RTD");
    
    rtdsize = o.rtdsize * sizeof(short int);
    if (rtdsize > 2*o.revbufsize) printf("RTD Total Size too big!\n");
    else printf(" (%i", o.rtdsize);
    if (1024*o.rtdavg > rtdsize) printf("Too many averages for given RTD size.\n");
    else printf("/%iavg)", o.rtdavg);
    printf("...");
    
    rtdout = malloc(num_rtds * rtdsize);
    
    if ((rtdframe_pp == NULL) || (rtdout == NULL)) {
      printe("RTD mallocs failed.\n");
    }
    
    /* for (int i = 0; i < o.num_ports; i++) { */
    /*   rtdframe_pp[i] = malloc(rtdsize); */
    /* } */
    for (int i = 0; i < num_rtds; i++) {
      rtdframe_pp[i] = malloc(rtdsize);
      if ( o.debug ) printf("Malloc'ed stuff for rtdframe %i\n",i);
    }
    
    /*
     * Create/truncate the real-time display file, fill it
     * with zeros to the desired size, then mmap it.
     */
    rfd = open(o.rtdfile,
	       O_RDWR|O_CREAT|O_TRUNC,
	       S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (rfd == -1) {
      printe("Failed to open rtd file.\n"); return;
    }
    if ((fstat(rfd, &sb) == -1) || (!S_ISREG(sb.st_mode))) {
      printe("Improper rtd file.\n"); return;
    }    
    int mapsize;
    if(o.digitizer_data) {
      mapsize = num_rtds*rtdsize + 72;
    }
    else {
      mapsize = num_rtds*rtdsize + 100;
    }
    char *zeroes = malloc(mapsize);
    memset(zeroes, 0, mapsize);
    ret = write(rfd, zeroes, mapsize);
    free(zeroes);
    //		printf("mmap, %i",rfd);fflush(stdout);
    rmap = mmap(0, mapsize, PROT_WRITE|PROT_READ, MAP_SHARED, rfd, 0);
    if (rmap == MAP_FAILED) {
      printe("mmap() of rtd file failed.\n"); return;
    }
    ret = close(rfd);
    madvise(rmap, mapsize, MADV_WILLNEED);
    
    /*
     * Set up basic RTD header
     */
    if(!o.digitizer_data){
      rtdh.cprtd.num_read = o.rtdsize*num_rtds;
      sprintf(rtdh.cprtd.site_id,"%s","RxDSP Woot?");
      rtdh.cprtd.hkey = 0xF00FABBA;
      rtdh.cprtd.num_channels=num_rtds;
      rtdh.cprtd.channel_flags=0x0F;
      rtdh.cprtd.num_samples=o.rtdsize;
      rtdh.cprtd.sample_frequency=960000;
      rtdh.cprtd.time_between_acquisitions=o.dt;
      rtdh.cprtd.byte_packing=0;
      rtdh.cprtd.code_version=0.1;
    }
    else {
      rtdh.prtd.num_read = o.rtdsize*num_rtds;
      sprintf(rtdh.prtd.site_id,"%s","RxDSP Woot?");
      rtdh.prtd.num_channels=num_rtds;
      rtdh.prtd.channel_flags=0x0F;
      rtdh.prtd.num_samples=o.rtdsize;
      rtdh.prtd.sample_frequency=960000;
      rtdh.prtd.time_between_acquisitions=o.dt;
      rtdh.prtd.byte_packing=0;
      rtdh.prtd.code_version=0.1;
    }
  }
  if ( o.debug ) printf("Finished setting up RTD header...\n");
  /*
   * Set up and create the write thread for each file.
   */
  for (int i = 0; i < o.num_ports; i++) {

    thread_args[i].port = o.ports[i];

    printf("Port %u...", o.ports[i]); fflush(stdout);
    thread_args[i].o = o;
    thread_args[i].retval = 0;
    thread_args[i].running = &running;
    //    thread_args[i].rtdframe = rtdframe_pp[i];
    for(int j = 0; j < o.nchan; j++) {

      ret = pthread_mutex_init(&rtdlocks[i*o.num_ports+j], NULL);
      if (ret) {
	printe("RTD mutex init failed: %i.\n", ret); exit(EEPP_THREAD);
      }
      
      thread_args[i].rtdframe[j] = rtdframe_pp[i*o.num_ports+j];
      thread_args[i].rlock[j] = &rtdlocks[i*o.num_ports+j];
      thread_args[i].time[j] = pg_time;
    }

    
    ret = pthread_create(&data_threads[i], NULL, tcp_play_hs_data_pt, (void *) &thread_args[i]);
    
    if (ret) {
      printe("Thread %i failed!: %i.\n", i, ret); exit(EEPP_THREAD);
    } else active_threads++;
  }
  
  if (o.debug) {
    if (!o.digitizer_data){
      printf("Size of header: %lu, rtdsize: %i, o.num_ports: %i.\n", 
	     sizeof( struct header_info ), rtdsize, o.num_ports);
    }
    else {
      printf("Size of header: %lu, rtdsize: %i, o.num_ports: %i.\n", 
	     sizeof( struct prtd_header_info ), rtdsize, o.num_ports);
    }
  }
  /*
   * Now we sit back and update RTD data until all files quit reading.
   */
  gettimeofday(&then, NULL);
  while ((active_threads > 0) || running) {
    if (o.dt > 0) {
      gettimeofday(&now, NULL); // Check time
      telapsed = now.tv_sec-then.tv_sec + 1E-6*(now.tv_usec-then.tv_usec);

      if (telapsed > o.dt) {
	/*
	 * Lock every rtd mutex, then just copy in the lock, for speed.
	 */
	for (int i = 0; i < o.num_ports; i++) {
	  for(int j = 0; j < o.nchan; j++) {

	    pthread_mutex_lock(&rtdlocks[i*o.num_ports + j]); //lock 'er up

	    //move it in
	    //PROBLEM LINE BELOW
	    memmove(&rtdout[(i*o.num_ports+j)*o.rtdsize], rtdframe_pp[i*o.num_ports+j], rtdsize);

	    //open again
	    pthread_mutex_unlock(&rtdlocks[i*o.num_ports + j]);
	  }
	}

	if(!o.digitizer_data){
	  rtdh.cprtd.start_time = time(NULL);
	  rtdh.cprtd.start_timeval = now;
	  rtdh.cprtd.averages = o.rtdavg;

	  memmove(rmap, &rtdh.cprtd, sizeof(struct header_info));
	  memmove(rmap+102, rtdout, rtdsize*o.num_ports);
	}
	else {
	  rtdh.prtd.start_time = time(NULL);
	  rtdh.prtd.start_timeval = now;

	  memmove(rmap, &rtdh.prtd, sizeof(struct prtd_header_info));
	  memmove(rmap+74, rtdout, rtdsize*o.num_ports);
	}
	then = now;
      }

    }

    /*
     * Check for any threads that are joinable (i.e. that have quit).
     */
    for (int i = 0; i < o.num_ports; i++) {
      ret = pthread_tryjoin_np(data_threads[i], (void *) &tret);

      tret = thread_args[i].retval;
      if (ret == 0) {
	active_threads--;
	if (tret) printf("Port %u error: %i...", o.ports[i], tret);
	if(active_threads == 0) {
	  running = false;
	}
      } // if (ret == 0) (thread died)
    } // for (; i < o.num_ports ;)
    usleep(5000); // Zzzz...
  }

  /*
   * Free.  FREE!!!
   */
  if (o.dt > 0) {
    for (int i = 0; i < o.num_ports; i++) {
      if (rtdframe_pp[i] != NULL) free(rtdframe_pp[i]);
    }
    free(rtdframe_pp); free(rtdlocks);
    free(thread_args); free(data_threads);
    if (rtdout != NULL) free(rtdout);
  }

  printf("All done!\n");

  pthread_exit(NULL);
    
  
}

void *tcp_play_hs_data_pt(void *threadarg)
{

  struct tcp_player_ptargs arg;
  arg = *(struct tcp_player_ptargs *) threadarg;

  //FIFO stuff
  struct simple_fifo *pafifo[MAX_NUMCHANS];
  long int fifo_loc;
  //  long int skip_loc;
  //  long int oldskip_loc;
  char *fifo_outbytes[MAX_NUMCHANS];
  char fifo_srch[18];
  
  /**************************************/
  //TCP port stuff
  int sockfd; // Socket file descriptor
  int nsockfd; // New Socket file descriptor
  int optval = 1;
  int sin_size; // to store struct size

  struct sockaddr_in addr_local;
  struct sockaddr_in addr_remote;
  
  struct tcp_header *tcp_hdr; //, *tcp_hdr_old; //maybe we need this?
  struct tcp_parser_hs *parser;
  char tcp_str[16] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };

  //Channel stuff
  struct dewe_chan *chan[MAX_NUMCHANS];
  char *chanbuff[MAX_NUMCHANS];
  char *chantimestamps[MAX_NUMCHANS];

  /**************************************/

  char *packet_buff, *hdr_buff, *tail_buff;
  long int count = 0, packet_bytes_remaining = 0; 
  long long unsigned int i = 0;
  long long unsigned int frames; //num rtd frames passed

  //time, rtd stuff
  int rtdbytes;
  struct tm ct;
  struct timeval start, now, then;
  double telapsed;

  //output file stuff
  FILE *ofile;
  char ostr[1024];

  if (arg.o.debug) { printf("Port %u thread init.\n", arg.port); fflush(stdout); }

  //tcp header stuff
  tcp_hdr = tcp_header_init();
  //  tcp_hdr_old = tcp_header_init();

  //tcp parser stuff
  parser = parser_init_hs();
  parser->hdrsz = 36; //per DEWESoft NET interface docs
  parser->tailsz = STARTSTR_SZ;

  parser->verbose = arg.o.verbose;                              

  parser->nchans = arg.o.nchan;

  //copy in start and tail string for use by parse_tcp_header() and strip_tcp_packet()
  if( arg.o.verbose ) printf("tcp_player_highspeed.c [tcp_play_hs_data_pt()] Start string:\t");
  for (int i = 0; i < STARTSTR_SZ; i ++){
    strncpy(&(parser->startstr[i]),&(tcp_str[8+i]),1);
    if ( arg.o.verbose ) printf("%x",parser->startstr[i]);
  }
  if( arg.o.verbose ) printf("\n");
  parser->startstr_sz = STARTSTR_SZ;

  if( arg.o.verbose ) printf("tcp_player_highspeed.c [tcp_play_hs_data_pt()] Tail string:\t");
  strncpy(parser->tlstr,tcp_str,STARTSTR_SZ); 
  if ( arg.o.verbose ){
    for (int i = 0; i < STARTSTR_SZ; i ++){
      printf("%x",parser->tlstr[i]);
    }
    printf("\n");
  }

  //data setup
  packet_buff = malloc(arg.o.revbufsize);
  hdr_buff = malloc(parser->hdrsz);
  tail_buff = malloc(parser->tailsz);
  
  frames = 0; //frames is num RTD frames passed

  //rtd setup
  rtdbytes = arg.o.rtdsize*sizeof(short int);
  gettimeofday(&start, NULL);
  then = start;

  //fifo setup
  if ( arg.o.verbose ) {
    printf("size of fifo struct: %lu\n",sizeof(*pafifo[0]));
    printf("arg.o.runmode: %i\n",arg.o.runmode);
  }

  if ( arg.o.runmode == 1 ) {
    pafifo[0] = malloc( sizeof(*pafifo[0]) );
    fifo_init(pafifo[0], 4*rtdbytes);  
    fifo_outbytes[0] = malloc(rtdbytes);
  }
  else if ( arg.o.runmode >= 2 ){
    for(int i = 0; i < parser->nchans; i++){
      if ( arg.o.verbose ) {
	printf("Setting up fifo #%i\n",i);
      }
      pafifo[i] = malloc( arg.o.nchan*sizeof(*pafifo[0]) );
      fifo_init(pafifo[i], 4*rtdbytes);  
      fifo_outbytes[i] = malloc(rtdbytes);
    }
  }
  strcpy(fifo_srch, "aDtromtu hoCllge");
  //  strcpy(fifo_srch,"Dartmouth College");

  //outfile string
  gmtime_r(&arg.time[0], &ct);
  sprintf(ostr, "%s/%s-%04i%02i%02i-%02i%02i%02i-p%u", arg.o.outdir, arg.o.prefix,
	  ct.tm_year+1900, ct.tm_mon+1, ct.tm_mday, ct.tm_hour, ct.tm_min, ct.tm_sec, arg.port);

  /*
   *  Which mode are we doing?
   */
  if (arg.o.runmode == 1){  //we're just doing a raw stream
    
    //outfile setup
    strncat( ostr, ".data", 5 ); //and finally...
    ofile = fopen(ostr, "a+");
    if (ofile == NULL) {
      fprintf(stderr, "Failed to open output file %s.\n", ostr);
      arg.retval = EEPP_FILE; pthread_exit((void *) &arg.retval);
    }
  }
  else if( arg.o.runmode > 1 ){ //If we're doing anything besides saving a raw stream, do chan stuff
    
    if( arg.o.verbose ) printf("tcp_player_highspeed.c [tcp_play_hs_data_pt()] parser->nchans\t=\t%i\n",parser->nchans);
    
    for (int i = 0; i < parser->nchans; i ++) {
      
      chan[i] = chan_init( i, 3, !arg.o.chans_are_synchr, false); //channel num, data type 3 (16-bit unsigned int), async, not singleval
      
      snprintf(chan[i]->outfname, 256, "%s-CH%i.data", ostr, chan[i]->num);
      chan[i]->outfile = fopen(chan[i]->outfname,"w");
      if (chan[i]->outfile == NULL) {
	fprintf(stderr,"Gerrorg. Couldn't open %s for channel %i.\n", chan[i]->outfname, chan[i]->num );
	arg.retval = EEPP_FILE; pthread_exit((void *) &arg.retval);
	//return(EXIT_FAILURE);
      }
      if( arg.o.verbose ){
	printf("Channel %i file:\t\t%p\n", i, chan[i]->outfile );
	printf("Channel %i filename:\t\t%s\n", i, chan[i]->outfname);
      }
      
      //tstamp files
      if( chan[i]->is_asynchr ){
	if( arg.o.runmode == 2 ){
	  snprintf(chan[i]->ts_fname, 256, "%s-CH%i_tstamps.data", ostr, chan[i]->num );}
	else {
	  snprintf(chan[i]->ts_fname, 256, "%s-CH%i_Dartmouth_header_tstamps.txt", ostr, chan[i]->num );
	}
	chan[i]->ts_file = fopen(chan[i]->ts_fname,"w");
	if (chan[i]->ts_file == NULL) {
	  fprintf(stderr,"Gerrorg. Couldn't open %s for channel %i.\n", chan[i]->ts_fname, chan[i]->num );
	  arg.retval = EEPP_FILE; pthread_exit((void *) &arg.retval);
	  //	    return(EXIT_FAILURE);
	}
	if( arg.o.verbose ){
	  printf("Channel %i timestamp file:\t\t%p\n", i, chan[i]->ts_file );
	  printf("Channel %i timestamp filename:\t\t%s\n", i, chan[i]->ts_fname);
	}
      }
    }
    
    //chan buffs
    for(int i = 0; i < parser->nchans; i++){
      chanbuff[i] = malloc( chan[i]->bufsize );
      chan[i]->d.type3 = chanbuff[i];
      chan[i]->packaddr = chan[i]->d.type3;
      chan[i]->oldpackaddr = chan[i]->d.type3;
      if( chan[i]->is_asynchr ){
	chantimestamps[i] = malloc( MAXNUMSAMPS * 8);
	chan[i]->oldtstamps_addr = chan[i]->tstamps_addr = chan[i]->timestamps = chantimestamps[i];
	if( arg.o.debug ) printf("tcp_player_highspeed.c [tcp_play_hs_data_pt()] Malloc'ed %i bytes for channel %u"
				 " timestamps buffer...\n", MAXNUMSAMPS * 8, chan[i]->num );
      }
    }
  }

  /*
   * Set up port for listening
   */
  /* Get the Socket file descriptor */
  if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {
    printf ("ERROR: Failed to obtain socket descriptor.\n");
    arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
    //      return (0);
  }
  else printf ("tcp_player_highspeed.c [tcp_play_hs_data_pt()] Obtained socket descriptor successfully.\n");
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  
  /* Fill the local socket address struct */
  addr_local.sin_family = AF_INET; // Protocol Family
  addr_local.sin_port = htons(arg.port); // Port number
  addr_local.sin_addr.s_addr = INADDR_ANY; // AutoFill local address
  bzero(&(addr_local.sin_zero), 8); // Flush the rest of struct

  /* Bind that port */
  if( bind(sockfd, (struct sockaddr*)&addr_local, sizeof(struct sockaddr)) == -1 ) {
    printf ("ERROR: Failed to bind Port %d.\n",arg.port);
    *arg.running = false;
    arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
  }
  else printf("tcp_player_highspeed.c [tcp_play_hs_data_pt()] bind tcp port %d in addr 0.0.0.0 sucessfully.\n",arg.port);
  
  /* Listen remote connect/calling */
  sin_size = sizeof(struct sockaddr_in);  
  if ( listen(sockfd,TCP_BACKLOG) == -1 ) {
    printf ("[tcp_player_highspeed.c [tcp_play_hs_data_pt()] ERROR: Failed to listen to port %d.\n", arg.port);
    *arg.running = false;
    arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
  }
  else printf ("tcp_player_highspeed.c [tcp_play_hs_data_pt()] Listening to port %d.\n", arg.port);
  
  /* Wait for a connection, and obtain a new socket file despriptor for single connection */
  if ((nsockfd = accept(sockfd, (struct sockaddr *)&addr_remote, (unsigned int *)&sin_size)) == -1 ) {
    printf ("ERROR: obtain new socket descriptor error.\n");
    *arg.running = false;
    arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
  }
  else {
    printf ("tcp_player_highspeed.c [tcp_play_hs_data_pt()] Server connection from %s.\n", inet_ntoa(addr_remote.sin_addr));
  }
  
  /*
   * Main data loop
   */
  while ( *arg.running ) {
    
    memset(packet_buff, 0, arg.o.revbufsize); //reset the data buff
    memset(hdr_buff,0,parser->hdrsz);
    parser->bufpos = 0; //reset our position in the buffer
    // tcp_hdr_old = tcp_hdr; //Save the old header, just in case
    
    count = recv(nsockfd, hdr_buff, parser->hdrsz, MSG_WAITALL);
    if(count != parser->hdrsz) {
      fprintf(stderr,"Couldn't get packet header from tcp port %u!!\n",arg.port);
      *arg.running = false;
      //      arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
    }
    else if(count == 0) {
      fprintf(stderr,"tcp_player_highspeed.c [tcp_play_hs_data_pt()] connection lost.\n");
      *arg.running = false;
      //	arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
    }
    if ( arg.o.debug ){
      printf("First 8 bytes of buffer:\n");
      for (int i = 0; i < STARTSTR_SZ; i ++){
    	printf("%x",hdr_buff[i]);
      }
      printf("\n");
    }

    //Show me the monies on this packet
    parse_tcp_header_hs(parser, hdr_buff, tcp_hdr);       //get new header_addr here

    if(arg.o.sleeptime) usleep(arg.o.sleeptime);
    
    if (parser->parse_ok) {
      
      printf("*****Packet #%u*****\n",parser->numpackets);
      print_tcp_header(tcp_hdr);
      if( arg.o.debug ) print_raw_tcp_header(tcp_hdr);
      
      //This is how much we need...
      packet_bytes_remaining = tcp_hdr->pack_sz - parser->hdrsz + parser->startstr_sz;
      if ( packet_bytes_remaining > arg.o.revbufsize ) {
	fprintf(stderr,"Packet is larger than the receive buffer! Can't handle it!"
		"\nPacket size:\t%i"
		"\nBuffer size:\t%i"
		"\nQuitting...",tcp_hdr->pack_sz,arg.o.revbufsize);
	*arg.running = false;
      }
      
      //Get the whole packet!
      while ( packet_bytes_remaining > 0 ) {
	
	usleep(100); //give 'em a chance, Spence
	count = recv(nsockfd,
		     (void *)((long int)packet_buff+parser->bufpos),
		     packet_bytes_remaining,
		     MSG_WAITALL);
	
	if ( count == 0 ){
	  fprintf(stderr,"Received zero bytes! What's the deal?\n"
		  "Should be %li bytes remaining to be received...\n",packet_bytes_remaining);
	  int blank_fire = 0;
	  while ( count == 0 ) {
	    usleep(100); //give 'em a chance, Spence
	    count = recv(nsockfd,
			 (void *)((long int)packet_buff+parser->bufpos),
			 packet_bytes_remaining,
			 MSG_WAITALL);
	    if( count == 0 ){
	      blank_fire++;
	      if( blank_fire == 5 ){
		fprintf(stderr,"Got %i receives of length zero...\n"
			"Rolling out!\n",blank_fire);
		*arg.running = false;
	      }
	    }
	  }
	}
	packet_bytes_remaining -= count;
	parser->bufpos += count;
	if( arg.o.verbose ) printf("%li bytes remaining...\n",packet_bytes_remaining);
      }
      
      if( arg.o.verbose ) printf("\n***\nBuffer #%lli\n***\n",i++);
      if( arg.o.verbose ) printf("\n***Received %li packet bytes***\n",parser->bufpos);
      
      //get the footer
      count = recv(nsockfd,
		   (void *)tail_buff,
		   parser->tailsz,
		   MSG_WAITALL);
      if ( arg.o.debug ){
      	printf("First 8 bytes of tail buffer:\n");
      	for (int i = 0; i < parser->tailsz; i ++){
      	  printf("%x",tail_buff[i]);
      	}
      	printf("\n");
      }
      
      
      if (arg.o.runmode == 1){ //just write en masse, monseigneur
	
	printf("Writing %li bytes\n",parser->bufpos + parser->hdrsz + parser->tailsz );
	count = fwrite(hdr_buff,1,parser->hdrsz, ofile);
	count += fwrite(packet_buff, 1, parser->bufpos, ofile);
	count += fwrite(tail_buff,1, parser->tailsz, ofile);
	
	if( arg.o.dt > 0 ) fifo_write( pafifo[0], packet_buff, parser->bufpos );
	if( !arg.o.diag && count == 0 && parser->bufpos != 0 ){
	  printf("Gerrorg writing to %s\n", ostr);
	  *arg.running = false; 
	  arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
	}
	parser->wcount += count;
      }
      else if (arg.o.runmode > 1){ //channel stuff
	
	//Now we know how big the packet is, and we've already gotten the first chan header
	parser->bufpos = 0; //Start bufpos over, since we have the whole packet
	for (int i = 0; i < parser->nchans; i ++) {
	  
	  //!!Might want to write a check right here to make sure we don't overrun
	  // the actual size of the packet

	  //get chan header
	  memcpy(&chan[i]->numsamps,(void *)((long int)packet_buff+parser->bufpos),sizeof(chan[i]->numsamps));
	  
	  //get number of sample bytes, move buffer forward 4 bytes to skip chan header
	  parser->bufpos += sizeof(chan[i]->numsamps);
	  chan[i]->numsampbytes = chan[i]->numsamps*chan[i]->dsize;
	  
	  if( arg.o.verbose ){
	    printf("Chan %i num samples: %i\n",chan[i]->num,chan[i]->numsamps);
	  }
	  
	  //write chan bytes
	  count = fwrite((void *)((long int)packet_buff+parser->bufpos),1,
			 chan[i]->numsampbytes,chan[i]->outfile);
	  parser->wcount += count;
	  if( chan[i]->numsampbytes == count ){
	    chan[i]->numbytes_received += count;
	  } else {
	    fprintf(stderr,"Whoa! There should be %i bytes for this chan,\n"
		    "but I only got %li!\n",chan[i]->numsampbytes,count);
	  }

	  //Shove data into RTD fifo, if that's what it's all about
	  if( arg.o.dt > 0 ) fifo_write( pafifo[i],(void *)((long int)packet_buff+parser->bufpos), chan[i]->numsampbytes );


	  //handle timestamp stuff if these chans are asynchronous
	  if (chan[i]->is_asynchr) {
	    chan[i]->numtbytes = chan[i]->numsamps*8;

	    if( arg.o.verbose ){
	      printf("Chan %i num tstamp bytes: %i\n",
		     chan[i]->num,chan[i]->numtbytes);
	    }
	    
	    if ( arg.o.runmode == 3 ){ //just get the dartmouth tstamp
	      long long int single_tstamp;
	      long int samp_num;

	      //try to find it!
	      if ( ( chan[i]->tstamps_addr = memmem((void *)((long int)packet_buff+parser->bufpos), chan[i]->numsampbytes, fifo_srch, 16 ) ) != NULL ) {
		
		//calculate which sample this is
		samp_num = ((long int)chan[i]->tstamps_addr-(long int)packet_buff-parser->bufpos)/chan[i]->dsize;
		
		//now get the corresponding timestamp
		//		single_tstamp = (long long int)((long int)packet_buff+parser->bufpos+chan[i]->numsampbytes+samp_num*8);
		memcpy((void *)&single_tstamp,(void *)((long int)packet_buff+parser->bufpos+chan[i]->numsampbytes+samp_num*8),8);
		
		if( arg.o.verbose ) {
		  printf("Found RxDSP header! Sample #%li\n",samp_num);
		  printf("Timestamp: %lli\n",single_tstamp);
		}
		//write to file, keep it moving
		fprintf(chan[i]->ts_file,"%i\t\t%lli\n",
			chan[i]->tbytes_received/8,single_tstamp);
		chan[i]->tbytes_received += 8;
		
	      } else {
		if (arg.o.verbose || arg.o.debug ) printf("Couldn't find RxDSP header!\n");
	      }
	    } else if( arg.o.runmode == 2 ){
	      
	      count = fwrite((void *)((long int)packet_buff+
				      parser->bufpos+chan[i]->numsampbytes),1,
			     chan[i]->numtbytes,chan[i]->ts_file);
	      parser->wcount += count;
	    }

	    //update buffer position
	    parser->bufpos += chan[i]->numsampbytes;
	    if( arg.o.verbose ){
	      printf("Buffer position is now %li\n",parser->bufpos);
	    }
	    parser->bufpos += chan[i]->numtbytes;
	    if( arg.o.verbose ){
	      printf("With tbytes,buffer position is now %li\n",parser->bufpos);
	    }

	  }
	  
	  
	  //Do RTD stuff
	  //write all chan bytes to the appropriate rtd frame
	  
	}

	
	
      }
      gettimeofday(&then, NULL);
      
      //RTD stuff here!
      // Copy into RTD memory if we're running the display
    } else { //parse was NOT okay
      fprintf(stderr,"Hold the phone! We definitely should have gotten a packet header here.\n"
	      "Crashing and burning...\n");
      *arg.running = false;
    }
    
    
    if (arg.o.dt > 0) {
	
      if( arg.o.runmode == 1 ){
	if( !arg.o.digitizer_data ) {
	
	  if( fifo_avail(pafifo[0]) > 2*rtdbytes ) {
	    if( (fifo_loc = fifo_search(pafifo[0], 2*rtdbytes, fifo_srch, 16 ) ) != EXIT_FAILURE ) {
	      
	      fifo_kill(pafifo[0], fifo_loc);
	      fifo_read(fifo_outbytes[0], pafifo[0], rtdbytes);
	      
	      pthread_mutex_lock(arg.rlock[0]);
	      
	      if (arg.o.debug) {
		printf("Port %u rtd moving rtdbytes %i from cfb %p to rtdb %p with %lu avail.\n",
		       arg.port, rtdbytes, packet_buff, arg.rtdframe[0], count);
	      }
	      memmove(arg.rtdframe[0], fifo_outbytes[0], rtdbytes);
	      pthread_mutex_unlock(arg.rlock[0]);
	    }
	    else {
	      fprintf(stderr,"Search for \"aDtromtu hoCllge\" failed!!\nNo rtd output...\n");
	      fprintf(stderr,"Total bytes read so far:\t%li\n",parser->wcount);
	    }
	  } 
	} 
	else { //it IS digitizer data
	  if( fifo_avail(pafifo[0]) >= rtdbytes ) {
	    //read it
	    fifo_read(fifo_outbytes[0], pafifo[0], rtdbytes);
	    
	    pthread_mutex_lock(arg.rlock[0]);
	    if (arg.o.debug) {
	      printf("Port %i rtd moving rtdbytes %i from cfb %p to rtdb %p with %li avail.\n",
		     arg.port, rtdbytes, packet_buff, arg.rtdframe[0], fifo_avail(pafifo[0]) );
	    }
	    memmove(arg.rtdframe[0], fifo_outbytes[0], rtdbytes);
	    pthread_mutex_unlock(arg.rlock[0]);
	  }
	}
      }
      else { //we're looping over channels!
	for(int i = 0; i < parser->nchans; i++){
	  
	  if( !arg.o.digitizer_data ) {
	    
	    if( fifo_avail(pafifo[i]) > 2*rtdbytes ) {
	      if( (fifo_loc = fifo_search(pafifo[i], 2*rtdbytes, fifo_srch, 16 ) ) != EXIT_FAILURE ) {
		
		fifo_kill(pafifo[i], fifo_loc);
		fifo_read(fifo_outbytes[i], pafifo[i], rtdbytes);
		
		pthread_mutex_lock(arg.rlock[i]);
		
		if (arg.o.debug) {
		  printf("Port %u rtd moving rtdbytes %i from cfb %p to rtdb %p with %lu avail.\n",
			 arg.port, rtdbytes, packet_buff, arg.rtdframe[i], count);
		}
		memmove(arg.rtdframe[i], fifo_outbytes[i], rtdbytes);
		pthread_mutex_unlock(arg.rlock[i]);
	      }
	      else {
		fprintf(stderr,"Search for \"aDtromtu hoCllge\" failed!!\nNo rtd output...\n");
		fprintf(stderr,"Total bytes read so far:\t%li\n",parser->wcount);
	      }
	    } 
	  } 
	  else { //it IS digitizer data
	    if( fifo_avail(pafifo[i]) >= rtdbytes ) {
	      //read it
	      fifo_read(fifo_outbytes[i], pafifo[i], rtdbytes);
	      
	      pthread_mutex_lock(arg.rlock[i]);
	      if (arg.o.debug) {
		printf("Port %i rtd moving rtdbytes %i from cfb %p to rtdb %p with %li avail.\n",
		       arg.port, rtdbytes, packet_buff, arg.rtdframe[i], fifo_avail(pafifo[i]) );
	      }
	      memmove(arg.rtdframe[i], fifo_outbytes[i], rtdbytes);
	      pthread_mutex_unlock(arg.rlock[i]);
	    } 
	  } //end digitizer_data
	} //end loop over channels
      } //end rtd stuff for channel modes

      frames++;
      
      if ((arg.o.maxacq > 0) && (frames > arg.o.maxacq)) {
	*arg.running = false;
      }
    
    } // end arg.o.dt > 0
    
  }

  /*
   * Wrap up the show
   */
  gettimeofday(&now, NULL);
  
  /* Close the file */
  if( arg.o.runmode == 1) {
    if ( (arg.retval = fclose(ofile) ) != 0) {
      printe("Couldn't close file %s!\n", ostr);
    }
  }
  
  //print stats
  print_stats_hs(parser);
  
  telapsed = now.tv_sec-start.tv_sec + 1E-6*(now.tv_usec-start.tv_usec);
  printf("Read %li bytes from port %u in %.4f s: %.4f KBps.\n", parser->wcount, arg.port, telapsed, (parser->wcount/1024.0)/telapsed);
  
  printf("OK!\n");
  close(nsockfd);
  printf("[server] connection to port %u closed.\n", arg.port);
  
  arg.retval = EXIT_SUCCESS; pthread_exit((void *) &arg.retval);
}


static void do_depart(int signum) {
  running = false;
  fprintf(stderr,"\nStopping...");
  
  return;
}
