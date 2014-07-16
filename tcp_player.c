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
  long int skip_loc;
  long int oldskip_loc;
  char *fifo_outbytes;
  

  /**************************************/
  //TCP port stuff
  int sockfd; // Socket file descriptor
  int nsockfd; // New Socket file descriptor
  int optval = 1;
  int sin_size; // to store struct size

  /* For keeping track of how much gets sent */
  int imod = 10;
  int packet_hcount = 0;

  struct sockaddr_in addr_local;
  struct sockaddr_in addr_remote;
  /**************************************/

  //data reading, accounting stuff
  struct data_packet packet = { .start_str = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7 }, 
				.pack_sz = 0, .pack_numsamps = 0, .pack_totalsamps = 0,
				.pack_time = 0.0 };
  long int count = 0, ret = 0;
  long long unsigned int i = 0;
  long long unsigned int frames, wcount;
  char *dataz;
  //  void *hptr; //pointer to header

  printf("Here comes packet start string:\n");
  for (int j = 0; j < 8; j++) {
    printf("%x",packet.start_str[j]);

  }

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
  //  char skip_str[8] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
  char skip_str[16] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 
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
   
  //!!!Make sleeptime some sort of command-line arg
  printf("Sleeping %i us.\n", 10000);
  
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

      fifo_write(fifo, dataz, count);

      if( fifo_avail(fifo) > 2*rtdbytes ) {
	packet_hcount = 0;

	if( (fifo_loc = fifo_search(fifo, "aDtromtu hoCllge", 2*rtdbytes) ) != EXIT_FAILURE ) {
	  
	  bool desirable = true;
	  if(desirable){
	    //Junk all TCP packet headers
	    oldskip_loc = fifo_loc;
	    while( ( skip_loc = fifo_skip(skip_str, 16, oldskip_loc, 48, 
					  rtdbytes - (oldskip_loc - fifo_loc), fifo) ) != EXIT_FAILURE ) {
	      printf("Found footer/header string!!\n");

	      packet_hcount++;
	      printf("Killed a packet header\n");
	      oldskip_loc = skip_loc;
	    }
	    //	    if( i % imod == 0 ) {
	    //	      printf("Killed %i packet headers\n", packet_hcount);
	      //	    }
	    fifo_loc = oldskip_loc;
	  }	  

	  fifo_kill(fifo, fifo_loc);
	  fifo_read(fifo_outbytes, fifo, rtdbytes);
	  
	  pthread_mutex_lock(arg.rlock);
	  if (arg.o.debug)
	    printf("Port %u rtd moving rtdbytes %i from cfb %p to rtdb %p with %lu avail.\n",
		   arg.port, rtdbytes, dataz, arg.rtdframe, count);
	  memmove(arg.rtdframe, fifo_outbytes, rtdbytes);
	  pthread_mutex_unlock(arg.rlock);
	}
	else {
	  fprintf(stderr,"Search for \"aDtromtu hoCllge\" failed!!\nNo rtd output...\n");
	}
      } 
	  
    }

    //if (arg.port == 1) { printf("r"); fflush(stdout); }
    //        check_acq_seq(dev_handle, arg.port, &fifo_acqseq);
      
      // Good read
	
      // copy and write
	
      // Check DSP header position within FIFO
      // "Dartmouth College "
      // "aDtromtu hoCllge e"
      //            if (arg.o.debug) {
	
      //            printf("p%i: %i\n",arg.port,hptr[16]*65536+hptr[17]); fflush(stdout);
      //            printf("%li.",hptr-dataz);
      //			rtd_log("Bad Colonel Frame Header Shift on module %i, frame %llu: %i.\n", arg.port, hptr-cframe->base, frames);
      //    			printe("CFHS on module %i, seq %i: %i.\n", arg.port, frames, hptr-dataz);
      //            }
	
      // Check alternating LSB position within Colonel Frame
      /*            if (arg.o.debug) {
		    for (int i = 0; i < 175; i++) {
		    ret = cfshort[31+i]&0b1;
		    if (ret) {
		    ret = i;
		    break;
		    }
		    }
		    if (ret != 3) {
 		    rtd_log("Bad LSB Pattern Shift on module %i, frame %llu: %i.\n", arg.port, hptr-dataz, frames);
		    printe("Bad LSBPS on module %i, frame %llu: %i.\n", arg.port, hptr-dataz, frames);
		    }
		    } // if debug*/
	
      // Build add-on frame header
      /* memset(&sync, 0, 16); */
      /* strncpy(sync.pattern, "\xFE\x6B\x28\x40", 4); */
      /* sync.t_sec = then.tv_sec-TIME_OFFSET; */
      /* sync.t_usec = then.tv_usec; */
      /* sync.size = count; */
	
      //if (arg.port == 1) { printf("b"); fflush(stdout); }
      //	        check_acq_seq(dev_handle, arg.port, &fifo_acqseq);
      //if (arg.port == 1) { printf("sc: %lu\n", (unsigned long) size_commit); fflush(stdout); }
      // Write header and frame to disk
      //      ret = fwrite(&sync, 1, sizeof(struct frame_sync), ofile);
      //      if (ret != sizeof(struct frame_sync))
      //	rtd_log("Failed to write sync, Port %u: %i.", arg.port, ret);
      //printf("foo"); fflush(stdout);
      //            fflush(ofile);
	
      //if (arg.port == 1) { printf("w"); fflush(stdout); }
      //	        check_acq_seq(dev_handle, arg.port, &fifo_acqseq);
	
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
