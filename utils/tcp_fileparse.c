/* tcp_fileparse
 *  ->The only program you'll ever need for stripping TCP headers from DEWESoft interfaces
 *  ->at the NASA Wallops Flight Facility in Wallops Island, VA, GUARANTEED
 *    
 *    
 *
 * se creó y encargó : Jul 14 (four days post-Sarah's birthday), 2014
 *
 *
 */

#define _GNU_SOURCE

#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>

#include "tcp_utils.h"
#include "tcp_fileparse.h"

#define DEBUG 0

#define MAX_BUFSZ 32768

bool running;

int main(int argc, char **argv)
{
  char *filename;
  FILE *datafile;
  struct stat fstat;

  long int bufcount;
  char *buff;
  int bufsz = MAX_BUFSZ;
  long int hprediction; //prediction for location of next header
  int num_badp;

  //TCP stuff
  struct tcp_header *tcp_hdr;
  struct tcp_parser *parser;
  bool strip_packet = true;
  bool parse_ok = false;

  signal(SIGINT, do_depart);


  //Handle command line  
  if(argc == 2) {
    filename = strdup(argv[1]);
  }
  else if ( argc == 3 ){
    filename = strdup(argv[1]);
    bufsz = atoi(argv[2]);
  }
  else {
    printf("%s <tcp data file> <optional max readbuffer size>\n",argv[0]);
    return(EXIT_SUCCESS);
  }

  //Open data file
  printf("Opening TCP data file %s\n",filename);
  if( ( datafile = fopen(filename,"r") ) == NULL ){
    fprintf(stderr,"Gerrorg. Couldn't open file %s.\n",filename);
    return(EXIT_FAILURE);
  }
  stat(filename,&fstat);

  //Prepare buffers
  buff = malloc(sizeof(char) * bufsz);
  printf("Buffer size:\t%i\n", bufsz);
 
  hprediction = 0; //At the beginning, we predict that the header will be right at the beginning!
  num_badp = 0;

  //tcp header stuff
  tcp_hdr = malloc( sizeof(struct tcp_header) );
  tcp_hdr->pack_sz =  0;
  tcp_hdr->pack_type = 0;
  tcp_hdr->pack_numsamps = 0;

  tcp_hdr->pack_totalsamps = 0;
  tcp_hdr->pack_time = 0;

  tcp_hdr->sync_numsamps = 0;

  //tcp parser stuff
  parser = malloc( sizeof(struct tcp_parser) );

  parser->hc = 0; //tcp header count
  parser->tc = 0; //tcp footer count
  parser->startstr_sz = STARTSTR_SZ;
  parser->hdrsz = 40;
  parser->tailsz = STARTSTR_SZ;

  parser->oldheader_addr = NULL;
  parser->header_addr = NULL;
  parser->tail_addr = NULL;

  parser->oldhpos = -(parser->hdrsz + parser->tailsz); //Needs to be initialized thusly so that 
  parser->hpos = 0;      //parse_tcp_header doesn't complain that the first header isn't where predicted
  parser->tpos = 0;
  //  parser->thdiff = 0; //Keep track of diff between tail and header in case anything wacky happens

  parser->bufpos = 0; //position of reading in current buffer of data
  parser->keep = 0;
  parser->total = 0;
  
  parser->isfile = true;
  parser->filesize = fstat.st_size;

  char tcp_str[16] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };


  //  print_header_sizeinfo(tcp_hdr);

  //Start looping through file
  printf("Parsing TCP data file %s...\n",filename);
  running = true;
  int i = 0;
  while( ( bufcount = fread(buff, 1, bufsz, datafile) ) > 0 && running ) {
    
    printf("\n***\nBuffer #%i\n***\n",i);
    //reset variables for fresh buffer of data
    /* parser->hc = 0; */
    /* parser->tc = 0; */
    /* if( (parser->hc - parser->tc) == 1 ){ */
      
    /* } */
    
    parser->bufpos = 0;
    parser->keep = bufcount;

    /* tail_addr = memmem(buff, bufcount, tcp_str, 8); */
    /* oldheader_addr = memmem(buff, keep, &tcp_str[8], 8); */

    while(  parser->bufpos < bufcount ){
      
      usleep(10000);

      parse_ok = parse_tcp_header(parser, buff, bufcount, tcp_hdr);

      if( parse_ok ){

	print_tcp_header(tcp_hdr);
	//	print_raw_tcp_header(tcp_hdr);
	if( strip_packet ){
	  strip_tcp_packet(parser, buff, bufcount, tcp_hdr);
	}

	//Current header not where predicted?
	if( parser->hpos != hprediction ){
	  num_badp +=1;
	  printf("***Header position not where predicted by previous packet size!\n");
	  printf("***Header position relative to beginning of buffer:\t%li\n",parser->hpos);
	  printf("***Predicted position relative to beginning of buffer:\t%li\n",hprediction);
	  printf("***Missed by %li bytes... Try again, Hank.\n",hprediction-parser->hpos);
	} 
	else {
	  printf("***Header %i was found where predicted: %li\n",parser->hc,hprediction);
	  //      tail_addr = (void *)(header_addr + 
	  parser->tc +=1; //Since the prediction was correct, there must have been a tail
	}

	//new header parsed OK, so its address is qualified to serve as the oldheader address
	parser->oldheader_addr = parser->header_addr;
	parser->oldhpos = parser->hpos;

      }
      else{
	printf("header_addr search came up NULL\n");
	printf("oldhpos is currently %li\n",parser->oldhpos);
	//Need to adjust oldhpos since its location was relative to an old buffer of data
	//Specifically, it needs to be a negative number to indicate it was found some number of bytes
	//BEFORE the current buffer of data
	parser->oldhpos -= bufcount;
	printf("oldhpos is NOW %li\n",parser->oldhpos);
      }

      //predicted position of the current header
      hprediction = parser->oldhpos + tcp_hdr->pack_sz + parser->tailsz + parser->startstr_sz; 

    }

    parser->total += bufcount;
    printf("Read %li bytes in total\n",parser->total);

    if(tcp_hdr->pack_sz < MAX_BUFSZ) {
      bufsz = tcp_hdr->pack_sz;
    }
    else {
      printf("TCP packet size is larger than max buffer size: %i\n",tcp_hdr->pack_sz);
      bufsz = MAX_BUFSZ;
    }
    i++;
  }

  //print stats
  printf("\n\n**********STATS**********\n\n");

  printf("Total header count:\t%i\n",parser->hc);
  printf("Total footer count:\t%i\n",parser->tc);
  printf("\n");
  printf("Incorrect header predictions:\t%i\n",num_badp);

  //  if(strip_packet){
    //    printf(

  //  }
  printf("\n*************************\n\n");

  //Clean up shop
  free(filename);
  free(tcp_hdr);
  free(parser);
  free(buff);
  fclose(datafile);
 
  return(EXIT_SUCCESS);
}

static void do_depart(int signum) {
  running = false;
  fprintf(stderr,"\nStopping...");
  return;
}
