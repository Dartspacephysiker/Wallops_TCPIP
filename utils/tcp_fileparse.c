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

#define DEFMAX_BUFSZ 32768

bool running;

int main(int argc, char **argv)
{
  char *filename;
  FILE *datafile;
  struct stat fstat;

  long int bufcount;
  char *buff;
  int max_bufsz = DEFMAX_BUFSZ;
  int bufsz;
  long int hprediction; //prediction for location of next header
  int num_badp;

  //TCP stuff
  struct tcp_header *tcp_hdr;
  struct tcp_parser *parser;
  bool parse_ok = false;
  bool careful_strip = true; //ensures things are where they say they are before junking them
  //  FILE *stripfile;
  char tcp_str[16] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
  for (int i = 0; i < STARTSTR_SZ; i ++){
    printf("%x",tcp_str[8+i]);
  }
  printf("\n");

  
  signal(SIGINT, do_depart);

  //tcp header stuff
  tcp_hdr = tcp_header_init();

  //tcp parser stuff
  parser = parser_init();

  parser->hdrsz = 32 + STARTSTR_SZ; //per DEWESoft NET interface docs

  //copy in start and tail string for use by parse_tcp_header() and strip_tcp_packet()
  for (int i = 0; i < STARTSTR_SZ; i ++){
    strncpy(&(parser->startstr[i]),&(tcp_str[8+i]),1);
    printf("%x",parser->startstr[i]);
  }
  printf("\n");
  parser->startstr_sz = STARTSTR_SZ;
  strncpy(parser->tlstr,tcp_str,STARTSTR_SZ); 
  for (int i = 0; i < STARTSTR_SZ; i ++){
    printf("%x",parser->tlstr[i]);
  }
  printf("\n");
  parser->tailsz = STARTSTR_SZ;

  parser->oldhpos = -(parser->hdrsz + parser->tailsz); //Needs to be initialized thusly so that 
                                                       //parse_tcp_header doesn't complain that 
                                                       //the first header isn't where predicted
  parser->do_predict = true;
  parser->isfile = true;

  //Handle command line  
  if(argc == 2) {
    filename = strdup(argv[1]);
  }
  else if ( argc == 3 ){
    filename = strdup(argv[1]);
    max_bufsz = atoi(argv[2]);
  }
  else if (argc == 4){
    filename = strdup(argv[1]);
    max_bufsz = atoi(argv[2]);
    parser->strip_packet = atoi(argv[3]);
    parser->strip_fname = malloc(sizeof(char) * 128);
    parser->strip_fname = "stripped.data";
    //    sprintf(parser->strip_fname,"stripped-%s",filename);
    parser->oldt_in_this_buff = 0;
    parser->t_in_this_buff = 0;
  }
  else {
    printf("%s <tcp data file> <optional max readbuffer size> <strip_packet (0/1/2)>\n",argv[0]);
    printf("If strip_packet = 0, no stripping of data is done.\n");
    printf("If strip_packet = 1, the packet header and footer are stripped from the data for RTD, but left in the data file\n");
    printf("If strip_packet = 2, the packet header and footer are stripped from the data for RTD AND the saved data file\n");
    printf("If strip_packet = 3, stripped data are saved and RTDed, and bad packets are output to an error file, badpack.data\n");
    return(EXIT_SUCCESS);
  }

  //Open data file
  printf("Opening TCP data file %s\n",filename);
  if( ( datafile = fopen(filename,"r") ) == NULL ){
    fprintf(stderr,"Gerrorg. Couldn't open file %s.\n",filename);
    return(EXIT_FAILURE);
  }
  stat(filename,&fstat);
  parser->filesize = fstat.st_size;

  //Prepare buffers
  buff = malloc(sizeof(char) * max_bufsz);
  printf("Max buffer size:\t%i\n", max_bufsz);
 
  //If stripping data, set up stripped file

  if(parser->strip_packet == 2){
    printf("stripfname: %s\n",parser->strip_fname);
    parser->stripfile = fopen(parser->strip_fname,"w");
    printf("Stripfile: %p\n",parser->stripfile);
    if (parser->stripfile == NULL) {
      fprintf(stderr,"Gerrorg. Couldn't open stripfile %s.\n",parser->strip_fname);
      return(EXIT_FAILURE);
    }
  }

  //  print_header_memberszinfo(tcp_hdr);

  //Prediction stuff
  if(parser->do_predict){
    hprediction = 0; //At the beginning, we predict that the header will be right at the beginning!
    num_badp = 0;
    /* if(parser->strip_packet){ */
    /*   hprediction = parser->hdrsz; */
    /* } */
  }

  //Start looping through file
  printf("Parsing TCP data file %s...\n",filename);
  running = true;
  int i = 0;
  bufsz = max_bufsz;
  int count = 0;
  long int wcount = 0;
  while( ( bufcount = fread(buff, 1, bufsz, datafile) ) > 0 && running ) {
    
    printf("\n***\nBuffer #%i\n***\n",i);

    parser->bufrem = bufcount;
    parser->bufpos = 0;
    parser->delbytes = 0;


    //might consider adding || parser->packetpos != header->pack_sz
    //no, definitely don't want to do this, actually. Will prematurely end loop
    while(  parser->bufpos < parser->bufrem ){

      //      if(!parser->strip_packet) usleep(10000);
      usleep(10000);

      //new header_addr here
      parse_tcp_header(parser, buff, tcp_hdr);

      //new hpos, packetpos, if applicable 
      update_after_parse_header(parser, buff, tcp_hdr);

      if( parser->strip_packet ){
	
	//determines whether there are footers to kill
	prep_for_strip(parser, buff, tcp_hdr);

	//do the deed
	strip_tcp_packet(parser, buff, tcp_hdr);

	//update our prediction accordingly
	if(parser->do_predict) {
	  
	  hprediction -= ( ( (int)parser->oldtkill + (int)parser->tkill) * parser->tailsz +
	  			   parser->hkill * parser->hdrsz );	  
	}	

	//finish the job
	post_strip(parser, buff, tcp_hdr);
	
      }

      if( parser->parse_ok ){
	
	print_tcp_header(tcp_hdr);
	//	print_raw_tcp_header(tcp_hdr);
	
	//Current header not where predicted?
	if( parser->do_predict) {

	  if( parser->hpos != hprediction ) {

	    num_badp +=1;
	    printf("***Header position not where predicted by previous packet size!\n");
	    printf("***Header position relative to beginning of buffer:\t%li\n",parser->hpos);
	    printf("***Predicted position relative to beginning of buffer:\t%li\n",hprediction);
	    printf("***Missed by %li bytes... Try again, Hank.\n",hprediction-parser->hpos);

	  }
	  else {

	    printf("***Header %i was found where predicted: %li\n",parser->hc,hprediction);
	    parser->tc +=1; //Since the prediction was correct, there must have been a tail

	  }
	  
	  //predicted position of the next header
	  hprediction = parser->hpos + tcp_hdr->pack_sz + parser->tailsz + parser->startstr_sz;

	} 	
      }	

      //new bufpos, packetpos happens here
      update_parser_addr_and_pos(parser, buff, tcp_hdr);
      
      
    } //end of current buffer

    //Update all stuff outside last buffer
    parser->total += bufcount;
    printf("Read %li bytes in total\n", parser->total);
    
    if(parser->strip_packet){

      parser->deltotal += parser->delbytes;
      printf("Killed %li bytes in total\n",parser->deltotal);
      printf("Writing %li bytes to %s\n",parser->bufrem, parser->strip_fname);

      if( parser->strip_packet == 2){//Up the ante

    	count = fwrite(buff, 1, parser->bufrem, parser->stripfile);
    	if( count == 0){
    	  printf("Gerrorg writing to %s\n",parser->stripfile);
    	}
    	wcount += count;

    	printf("count = %i\n",count);

      }
    }

    hprediction -= parser->bufrem;

    i++;

  }

  //print stats
  printf("\n\n**********STATS**********\n\n");

  printf("Total header count:\t%i\n",parser->hc);
  printf("Total footer count:\t%i\n",parser->tc);
  printf("\n");
  printf("Incorrect header predictions:\t%i\n",num_badp);
  printf("\n");
  printf("Total bytes read: %li\n",parser->total);
  printf("Actual file size: %li\n",parser->filesize);

  if( parser->strip_packet ){

    printf("\n");
    printf("Total headers killed: %i\n", parser->numhkill);
    printf("Total footers killed: %i\n", parser->numtkill);
    printf("Total bytes stripped: %i\n", parser->deltotal);
    if( parser->strip_packet == 2 ){

      printf("Wrote %li bytes to file %s\n", wcount, parser->strip_fname);

    }
  }
  printf("\n*************************\n\n");

  //Clean up shop
  if(datafile != NULL) fclose(datafile);
  free(filename);
  free(tcp_hdr);
  //  free_parser(parser);
  free(parser);
  free(buff);
 
  return(EXIT_SUCCESS);
}

static void do_depart(int signum) {
  running = false;
  fprintf(stderr,"\nStopping...");
  return;
}
