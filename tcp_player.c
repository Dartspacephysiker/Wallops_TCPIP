/* tcp_player
 *  ->The latest episode in what has become a saga of bastardizing qusb_acq 
 *  
 *    
 *    
 *
 * se creó y encargó : Jul 14 (four days post-Sarah's birthday), 2014
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

#include "simple_fifo.h"
#include "tcp_player_helpers.h"
#include "tcp_player.h"

#define EEPP_FILE 8
#define EEPP_THREAD 9
#define MIN_BYTES_READ 100
#define BACKLOG 10

static bool running = true;

int main(int argc, char **argv)
{
  struct player_opt o;  

  init_opt(&o);
  parse_opt(&o, argc, argv);

  signal(SIGINT, do_depart);

  tcp_play(o);

}

void tcp_play(struct player_opt o) {
  
  time_t pg_time;
  int tret, ret, rtdsize = 0;
  struct tcp_player_ptargs *thread_args;
  pthread_t *data_threads;
    
  short int **rtdframe, *rtdout = NULL;
  struct header_info header;
  int rfd, active_threads = 0;
  char *rmap = NULL;
  struct stat sb;
  pthread_mutex_t *rtdlocks;
  double telapsed;
  struct timeval now, then;

  pg_time = time(NULL);


  data_threads = malloc(o.num_files * sizeof(pthread_t));
  printf("o.num_files is currently %i\n",o.num_files);
  rtdlocks = malloc(o.num_files * sizeof(pthread_mutex_t));
  thread_args = malloc(o.num_files * sizeof(struct tcp_player_ptargs));
  rtdframe = malloc(o.num_files * sizeof(short int *));
  
  if (o.dt > 0) {
    printf("RTD");
    
    rtdsize = o.rtdsize * sizeof(short int);
    if (rtdsize > 2*o.revbufsize) printf("RTD Total Size too big!\n");
    else printf(" (%i", o.rtdsize);
    if (1024*o.rtdavg > rtdsize) printf("Too many averages for given RTD size.\n");
    else printf("/%iavg)", o.rtdavg);
    printf("...");
    
    rtdout = malloc(o.num_files * rtdsize);
    
    if ((rtdframe == NULL) || (rtdout == NULL)) {
      printe("RTD mallocs failed.\n");
    }
    
    for (int i = 0; i < o.num_files; i++) {
      rtdframe[i] = malloc(rtdsize);
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
    int mapsize = o.num_files*rtdsize + 100;
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
    header.num_read = o.rtdsize*o.num_files;
    sprintf(header.site_id,"%s","RxDSP Woot?");
    header.hkey = 0xF00FABBA;
    header.num_channels=o.num_files;
    header.channel_flags=0x0F;
    header.num_samples=o.rtdsize;
//!!! DOES THIS NEED TO BE CHANGED TO 960000?
    header.sample_frequency=960000;
    header.time_between_acquisitions=o.dt;
    header.byte_packing=0;
    header.code_version=0.1;
  }
    
  /*
   * Set up and create the write thread for each file.
   */
  for (int i = 0; i < o.num_files; i++) {
    thread_args[i].port = o.ports[i];
    ret = pthread_mutex_init(&rtdlocks[i], NULL);
    if (ret) {
      printe("RTD mutex init failed: %i.\n", ret); exit(EEPP_THREAD);
    }
    

    printf("Port %u...", o.ports[i]); fflush(stdout);
    thread_args[i].o = o;
    thread_args[i].retval = 0;
    thread_args[i].running = &running;
    thread_args[i].rtdframe = rtdframe[i];
    thread_args[i].rlock = &rtdlocks[i];
    thread_args[i].time = pg_time;
    
    ret = pthread_create(&data_threads[i], NULL, tcp_player_data_pt, (void *) &thread_args[i]);
    
    if (ret) {
      printe("Thread %i failed!: %i.\n", i, ret); exit(EEPP_THREAD);
    } else active_threads++;
  }
  
  if (o.debug) printf("Size of header: %li, rtdsize: %i, o.num_files: %i.\n", sizeof(header), rtdsize, o.num_files);


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
	for (int i = 0; i < o.num_files; i++) {
	  pthread_mutex_lock(&rtdlocks[i]);
	  memmove(&rtdout[i*o.rtdsize], rtdframe[i], rtdsize);
	  pthread_mutex_unlock(&rtdlocks[i]);
	}

	header.start_time = time(NULL);
	header.start_timeval = now;
	header.averages = o.rtdavg;

	memmove(rmap, &header, sizeof(struct header_info));
	memmove(rmap+102, rtdout, rtdsize*o.num_files);

	then = now;
      }

    }

    /*
     * Check for any threads that are joinable (i.e. that have quit).
     */
    for (int i = 0; i < o.num_files; i++) {
      ret = pthread_tryjoin_np(data_threads[i], (void *) &tret);

      tret = thread_args[i].retval;
      if (ret == 0) {
	active_threads--;
	if (tret) printf("Port %u error: %i...", o.ports[i], tret);
	if(active_threads == 0) {
	  running = false;
	}
      } // if (ret == 0) (thread died)
    } // for (; i < o.num_files ;)
    usleep(5000); // Zzzz...
  }

  /*
   * Free.  FREE!!!
   */
  if (o.dt > 0) {
    for (int i = 0; i < o.num_files; i++) {
      if (rtdframe[i] != NULL) free(rtdframe[i]);
    }
    free(rtdframe); free(rtdlocks);
    free(thread_args); free(data_threads);
    if (rtdout != NULL) free(rtdout);
  }

  printf("All done!\n");

  pthread_exit(NULL);
  
  
}

void *tcp_player_data_pt(void *threadarg) {

  struct tcp_player_ptargs arg;
  arg = *(struct tcp_player_ptargs *) threadarg;

  //FIFO stuff
  struct simple_fifo *fifo;
  long int fifo_loc;
  //  long int skip_loc;
  //  long int oldskip_loc;
  char *fifo_outbytes;
  

  /**************************************/
  //TCP port stuff
  int sockfd; // Socket file descriptor
  int nsockfd; // New Socket file descriptor
  int optval = 1;
  int sin_size; // to store struct size

  /* For keeping track of how much gets sent */
  int imod = 10;

  struct sockaddr_in addr_local;
  struct sockaddr_in addr_remote;

  
  struct tcp_header *tcp_hdr;
  int tcp_hc = 0; //tcp header count
  int tcp_tc = 0; //tcp footer count
  int tcp_hdrsz = 40;
  int tcp_tailsz = 8;
  void *oldheader_loc;
  void *header_loc;
  void *tail_loc;
  long int tail_diff = 0;
  long int header_diff = 0;
  long int keep;
  /**************************************/

  //  struct tcp_header packet = { .start_str = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7 }, 
  //				.pack_sz = 0, .pack_numsamps = 0, .pack_totalsamps = 0,
  //				.pack_time = 0.0 };

  char *dataz;
  long int count = 0, ret = 0;
  long long unsigned int i = 0;
  long long unsigned int frames, wcount;
  //  void *hptr; //pointer to header

  /* printf("Here comes packet start string:\n"); */
  /* for (int j = 0; j < 8; j++) { */
  /*   printf("%x",tcp_hdr.start_str[j]); */

  /* } */

  //time, rtd stuff
  int rtdbytes;
  struct tm ct;
  struct timeval start, now, then;
  double telapsed;

  //output file stuff
  FILE *ofile;
  char ostr[1024];

  if (arg.o.debug) { printf("Port %u thread init.\n", arg.port); fflush(stdout); }

  //rtd setup
  rtdbytes = arg.o.rtdsize*sizeof(short int);
  gettimeofday(&start, NULL);
  then = start;

  //fifo setup
  fifo = malloc( sizeof(*fifo) );
  fifo_init(fifo, 4*rtdbytes);  
  fifo_outbytes = malloc(rtdbytes);
  long int fifo_count;

  tcp_hdr = malloc( sizeof(struct tcp_header) );
  char tcp_str[16] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };

  //data setup
  dataz = malloc(arg.o.revbufsize);
  frames = count = wcount = 0;

  //outfile setup
  gmtime_r(&arg.time, &ct);
  sprintf(ostr, "%s/%s-%04i%02i%02i-%02i%02i%02i-p%u.data", arg.o.outdir, arg.o.prefix,
	  ct.tm_year+1900, ct.tm_mon+1, ct.tm_mday, ct.tm_hour, ct.tm_min, ct.tm_sec, arg.port);
  ofile = fopen(ostr, "a");
  if (ofile == NULL) {
    fprintf(stderr, "Failed to open output file %s.\n", ostr);
    arg.retval = EEPP_FILE; pthread_exit((void *) &arg.retval);
  }
   
  // I don't think we want to sleep while acquiring TCP/IP data streams at high rates...
  //  printf("Sleeping %i us.\n", 10000);
  
  /*
   * Set up port for listening
   */

  /* Get the Socket file descriptor */
  if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
    {
      printf ("ERROR: Failed to obtain Socket Descriptor.\n");
      return (0);
    }
  else printf ("[server] obtain socket descriptor successfully.\n");
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  
  /* Fill the local socket address struct */
  addr_local.sin_family = AF_INET; // Protocol Family
  addr_local.sin_port = htons(arg.port); // Port number
  addr_local.sin_addr.s_addr = INADDR_ANY; // AutoFill local address
  bzero(&(addr_local.sin_zero), 8); // Flush the rest of struct

  /* Bind a special Port */
  if( bind(sockfd, (struct sockaddr*)&addr_local, sizeof(struct sockaddr)) == -1 )
    {
      printf ("ERROR: Failed to bind Port %d.\n",arg.port);
      *arg.running = false;
      arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
    }
  else printf("[server] bind tcp port %d in addr 0.0.0.0 sucessfully.\n",arg.port);

  /* Listen remote connect/calling */
  sin_size = sizeof(struct sockaddr_in);  
  if(listen(sockfd,BACKLOG) == -1)
    {
      printf ("ERROR: Failed to listen Port %d.\n", arg.port);
      *arg.running = false;
      arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
    }
  else printf ("[server] listening the port %d sucessfully.\n", arg.port);

  /* Wait for a connection, and obtain a new socket file despriptor for single connection */
  if ((nsockfd = accept(sockfd, (struct sockaddr *)&addr_remote, &sin_size)) == -1) {
    printf ("ERROR: obtain new socket descriptor error.\n");
    *arg.running = false;
    arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
  }
  else {
    printf ("[server] server has got connect from %s.\n", inet_ntoa(addr_remote.sin_addr));
  }
  
  /*
   * Main data loop
   */
  while ( *arg.running ) {
    if (arg.o.debug) { printf("Port %u debug.\n", arg.port); fflush(stdout); }
    //    usleep(sleeptime);
      
    if (arg.o.debug) { printf("Port %u read data.\n", arg.port); fflush(stdout); }
    
    memset(dataz, 0, arg.o.revbufsize);

    /* Read me */
    count = recv(nsockfd, dataz, arg.o.revbufsize, 0);
    if(count < 0)
      {
	fprintf(stderr,"Couldn't read tcp port %u!!\n",arg.port);
	*arg.running = false;
	arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
      }
    else if(count == 0)
      {
	printf("[server] connection lost.\n");
	*arg.running = false;
	arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
      }

    ret = fwrite(dataz, sizeof(char), count, ofile);
    if(ret < count)
      {
	printf("File write failed.\n");
	*arg.running = false;
	arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
      }
    //    bzero(dataz, arg.o.revbufsize);
    wcount += ret;
  
    gettimeofday(&then, NULL);

      // Copy into RTD memory if we're running the display

    if(i % imod == 0)
      {
	printf("Received %li bytes\n", ret);
      }


    if (arg.o.dt > 0) {
      
      tcp_hc = 0;
      tail_diff = 0;
      header_diff = 0;
      keep = (long int)count;

      tail_loc = memmem(dataz, count, tcp_str, 8);
      oldheader_loc = memmem(dataz, keep, &tcp_str[8], 8);

      /*In general, reads will finish before we find the tail, so we want to kill them right here*/
      if( (tail_loc != NULL ) && ( oldheader_loc != NULL ) && (tail_loc < oldheader_loc) ){
	
	if(arg.o.debug){ printf("**\ntail %i loc:%p\n**\n",tcp_hc, tail_loc); }
	

	//get diff between start of dataz and where tail was found so we don't move more than warranted
	tail_diff = (long int)tail_loc - (long int)dataz;
	if(arg.o.debug){ printf("tail diff:\t%li\n",tail_diff); }

	keep -= ( tcp_tailsz + tail_diff );

	if(arg.o.debug) {printf("Copying %li bytes from %p to %p\n",keep,
				tail_loc+tcp_tailsz, tail_loc); }
	memmove(tail_loc, tail_loc+tcp_tailsz, keep); 
	tcp_tc++; //Got 'im!
	
	oldheader_loc -= tcp_tailsz; 	  //We killed the tail string, so we need to update the header location

      }

      /*First go, in case dataz starts with a header*/
      //&tcp_str[8] is address for start string
      if( oldheader_loc != NULL ){
	tcp_hc++;
	if(arg.o.debug){printf("**\nheader %i loc:%p\n**\n",tcp_hc, oldheader_loc); }
	
	//Get tcp packet header data
	memcpy(tcp_hdr, oldheader_loc, tcp_hdrsz);
	
	if((i-1) % imod == 0) print_tcp_header(tcp_hdr);
	
	//get diff between start of dataz and where header was found so we don't move more than is warranted
	header_diff = (long int)oldheader_loc - (long int)dataz;
	if(arg.o.debug){ printf("header diff:\t%li\n",header_diff); }

	keep =  count - tcp_hdrsz - header_diff; //Only deal with bytes that haven't been searched or discarded

	if(arg.o.debug) {printf("Copying %li bytes from %p to %p\n",keep,
				oldheader_loc+tcp_hdrsz, oldheader_loc); }
	memmove(oldheader_loc, oldheader_loc+tcp_hdrsz, keep); 
	//	}	

	//Now data are moved, and oldheader_loc has no header there!
	//loop while we can still find a footer immediately followed by a header
	while( ( keep  > 0 )  &&
	       ( ( header_loc = parse_tcp_header(tcp_hdr, oldheader_loc, keep ) )  != NULL ) ){
	  tcp_hc++;
	  tcp_tc++;

	  //	  printf("**\nheader %i loc:%p\n**\n",tcp_hc,header_loc);
	  //	  if((i-1) % imod == 0) pack_err = print_tcp_header(tcp_hdr);
	  //	  pack_err = print_tcp_header(tcp_hdr);
	
	  //JUNK HEADER RIGHT HERE
	  //	  if(junk_tcpheader){

	  header_diff = (long int)header_loc - (long int)oldheader_loc;
	    
	  if(arg.o.debug) {printf("header diff:\t%li\n",header_diff); }

	  keep = keep -  header_diff - tcp_hdrsz - tcp_tailsz; //only keep bytes not searched or discarded

	  if(arg.o.debug){ printf("Keep+totaldiff=\t%li\n",keep+(long int)header_loc-(long int)dataz); }

	  //Notice that the following memmove looks at header_loc MINUS tcp_tailsz, which is because we want 
	  //to kill the footer of the TCPIP packet (i.e., {0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00})
	  memmove(header_loc-tcp_tailsz,header_loc+tcp_hdrsz, keep ); 
	  if(arg.o.debug){ printf("Kept %li bytes\n",keep); }
	  //	  }

	  oldheader_loc = header_loc;	

	}//while(we can find headers to kill)
      } //if(oldheader_loc != NULL)


      //Want to write all data, excluding headers and footers we've killed
      fifo_count = count - tcp_tc * tcp_tailsz - tcp_hc * tcp_hdrsz; 
      fifo_write(fifo, dataz, fifo_count);

      //If we missed either a header or a footer, it will get FFTed and make display a little messy
      if ( (  parse_tcp_header(tcp_hdr, fifo->head, fifo_avail(fifo) ) ) != NULL ) {
	printf("Missed a tcp header!!!\n"); }
      if( ( memmem(fifo->head, fifo_avail(fifo), tcp_str, tcp_tailsz) ) != NULL ){
	printf("Missed a footer!!!\n"); }

      if( fifo_avail(fifo) > 2*rtdbytes ) {
	if( (fifo_loc = fifo_search(fifo, "aDtromtu hoCllge", 2*rtdbytes) ) != EXIT_FAILURE ) {
	  
	  //Junk all TCP packet headers
	    /* oldskip_loc = fifo_loc; */
	    /* while( ( skip_loc = fifo_skip(skip_str, 16, oldskip_loc, 48,  */
	    /* 				  rtdbytes - (oldskip_loc - fifo_loc), fifo) ) != EXIT_FAILURE ) { */
	    /*   printf("Found footer/header string!!\n"); */

	    /*   packet_hcount++; */
	    /*   printf("Killed a packet header\n"); */
	    /*   oldskip_loc = skip_loc; */
	      
	    //	    if( i % imod == 0 ) {
	    //	      printf("Killed %i packet headers\n", packet_hcount);
	      //	    }
	    /* fifo_loc = oldskip_loc; */
	    /* }	   */

	  fifo_kill(fifo, fifo_loc);
	  fifo_read(fifo_outbytes, fifo, rtdbytes);
	  
	  pthread_mutex_lock(arg.rlock);

	  if (arg.o.debug) {
	    printf("Port %u rtd moving rtdbytes %i from cfb %p to rtdb %p with %lu avail.\n",
		   arg.port, rtdbytes, dataz, arg.rtdframe, count);
	  }

	  memmove(arg.rtdframe, fifo_outbytes, rtdbytes);
	  pthread_mutex_unlock(arg.rlock);

	}
	else {
	  fprintf(stderr,"Search for \"aDtromtu hoCllge\" failed!!\nNo rtd output...\n");
	  fprintf(stderr,"Total bytes read so far:\t%lli\n",wcount);
	}
      } 
	  
    }
	
    frames++;
    
    if ((arg.o.maxacq > 0) && (frames > arg.o.maxacq)) {
      *arg.running = false;
    }
    
  }
    
  gettimeofday(&now, NULL);
    
  /* Close the file */
  
  if ( (arg.retval = fclose(ofile) ) != 0) {
    printe("Couldn't close file %s!\n", ostr);
  }
    
  telapsed = now.tv_sec-start.tv_sec + 1E-6*(now.tv_usec-start.tv_usec);
    
  printf("Read %lli bytes from port %u in %.4f s: %.4f KBps.\n", wcount, arg.port, telapsed, (wcount/1024.0)/telapsed);

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
