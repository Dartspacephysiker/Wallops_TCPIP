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
#define DEF_NUMCHANS 2

bool running;

int main(int argc, char **argv)
{
  int numchans;
  struct dewe_chan *chan[DEF_NUMCHANS];

  char *filename;
  FILE *datafile;
  struct stat fstat;

  long int bufcount;
  char *buff;
  int max_bufsz = DEFMAX_BUFSZ;
  int bufsz;
  //  long int hprediction; //prediction for location of next header
  //  unsigned int num_badp;

  //TCP stuff
  struct tcp_header *tcp_hdr;
  struct tcp_parser *parser;
  bool parse_ok = false;
  bool careful_strip = true; //ensures things are where they say they are before junking them
  //  FILE *stripfile;
  char tcp_str[16] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
  
  signal(SIGINT, do_depart);

  //tcp header stuff
  tcp_hdr = tcp_header_init();

  //tcp parser stuff
  parser = parser_init();

  parser->hdrsz = 32 + STARTSTR_SZ; //per DEWESoft NET interface docs

  //copy in start and tail string for use by parse_tcp_header() and strip_tcp_packet()
  for (int i = 0; i < STARTSTR_SZ; i ++){
    strncpy(&(parser->startstr[i]),&(tcp_str[8+i]),1);
    //    printf("%x",parser->startstr[i]);
  }
  //  printf("\n");
  parser->startstr_sz = STARTSTR_SZ;
  strncpy(parser->tlstr,tcp_str,STARTSTR_SZ); 
  /* for (int i = 0; i < STARTSTR_SZ; i ++){ */
  /*   printf("%x",parser->tlstr[i]); */
  /* } */
  /* printf("\n"); */
  parser->tailsz = STARTSTR_SZ;

  parser->oldhpos = -(parser->hdrsz + parser->tailsz); //Needs to be initialized thusly so that 
                                                       //parse_tcp_header doesn't complain that 
                                                       //the first header isn't where predicted
  parser->do_predict = true;
  parser->isfile = true;

  parser->verbose = true;

  //Handle command line  
  if(argc == 2) {
    filename = strdup(argv[1]);
  }
  else if ( argc == 3 ){
    filename = strdup(argv[1]);
    max_bufsz = atoi(argv[2]);
  }
  else if (argc == 4){
    if( atoi(argv[3]) <= 2){ //doing strip packet stuff
	filename = strdup(argv[1]);
	max_bufsz = atoi(argv[2]);
	parser->strip_packet = atoi(argv[3]);
	parser->strip_fname = malloc(sizeof(char) * 128);
	parser->strip_fname = "stripped.data";
	//    sprintf(parser->strip_fname,"stripped-%s",filename);
	parser->oldt_in_this_buff = 0;
	parser->t_in_this_buff = 0;
      }
      else if( atoi(argv[3]) == 3 ){
	parser->do_chans = true;
      }
      else {
	printf("You fool!!! You expect this program to clean your room as well?\n");
	return EXIT_FAILURE;
      }
  }
  else {
    printf("%s <tcp data file> <optional max readbuffer size> <strip_packet (0/1/2)>\n",argv[0]);
    printf("If strip_packet = 0, no stripping of data is done.\n");
    printf("If strip_packet = 1, the packet header and footer are stripped from the data for RTD, but left in the data file\n");
    printf("If strip_packet = 2, the packet header and footer are stripped from the data for RTD AND the saved data file\n");
    printf("If strip_packet = 3, stripped data are saved and RTDed, and bad packets are output to an error file, badpack.data\n");
    return(EXIT_SUCCESS);
  }

  //tcp chan stuff
  if( parser->do_chans ){
    for (int i = 0; i < numchans; i ++) {
      chan[i] = chan_init(i,3,true,false); //channel number, data type 3 (16-bit unsigned int), async, not singleval
    }
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

  print_header_memberszinfo(tcp_hdr);

  //Prediction stuff
  if(parser->do_predict){
    parser->hprediction = 0; //At the beginning, we predict that the header will be right at the beginning!
    parser->num_badp = 0;
    /* if(parser->strip_packet){ */
    /*   parser->hprediction = parser->hdrsz; */
    /* } */
  }

  //Start looping through file
  printf("Parsing TCP data file %s...\n",filename);
  running = true;
  int i = 0;
  bufsz = max_bufsz;
  int count = 0;
  while( ( bufcount = fread(buff, 1, bufsz, datafile) ) > 0 && running ) {
    
    printf("\n***\nBuffer #%i\n***\n",i);

    parser->bufrem = bufcount;
    parser->bufpos = 0;
    parser->delbytes = 0;


    //might consider adding || parser->packetpos != header->pack_sz
    //no, definitely don't want to do this, actually. Will prematurely end loop
    while(  parser->bufpos < parser->bufrem ){

      if(parser->strip_packet) { usleep(4000); }
	else { usleep(10000); }


      //new header_addr here
      parse_tcp_header(parser, buff, tcp_hdr);

      if( parser->do_chans ){
	if( parser->parse_ok ){
	  //use this to find out if we need to make way for a new buffer of channel samples
	  get_chan_info(chan,numchans);
	}
      
	long int tmp_buf_pos = parser->bufpos;
	
	bool moresamps = true;
	int hasallsamps = 0;

	while( moresamps ){
	  
	  for(int j = 0; j < numchans; j++ ){
	    moresamps = get_chan_samples( chan[i] , parser, tcp_hdr );
	    if( moresamps ) hasallsamps++;
	  } 
	  if( hasallsamps == numchans ){
	    //
	  }
	}

	parser->bufpos = tmp_buf_pos; //set it to what it was before channels messed with it

      } //end do_chans

      //new hpos, packetpos, if applicable 
      update_after_parse_header(parser, buff, tcp_hdr);

      if( parser->strip_packet ){

	prep_for_strip(parser, buff, tcp_hdr); 	//determines whether there are footers to kill
	strip_tcp_packet(parser, buff, tcp_hdr); 	//do the deed

	if( parser->do_predict ) { 	//update our prediction accordingly	  
	  parser->hprediction -= ( ( (int)parser->oldtkill + (int)parser->tkill) * parser->tailsz +
				   parser->hkill * parser->hdrsz );	  
	}	

	post_strip(parser, buff, tcp_hdr); 	//finish the job	
      } //end strip_packet



      if( parser->parse_ok ){
	
	printf("*****Packet #%u*****\n",parser->numpackets);
	print_tcp_header(tcp_hdr);
	print_raw_tcp_header(tcp_hdr);
	
	//Current header not where predicted?
	if( parser->do_predict) {
	  
	  if( parser->hpos != parser->hprediction ) {
	    parser->num_badp +=1;
	    printf("***Header position not where predicted by previous packet size!\n");
	    printf("***Header position relative to beginning of buffer:\t%li\n",parser->hpos);
	    printf("***Predicted position relative to beginning of buffer:\t%li\n",parser->hprediction);
	    printf("***Missed by %li bytes... Try again, Hank.\n",parser->hprediction-parser->hpos);
	  }
	  else {

	    printf("***Header %i was found where predicted: %li\n",parser->hc,parser->hprediction);
	    parser->tc +=1; //Since the prediction was correct, there must have been a tail

	  }

	  //predicted position of the next header
	  parser->hprediction = parser->hpos + tcp_hdr->pack_sz + parser->tailsz + parser->startstr_sz;

	} 	
      }	

      update_parser_addr_and_pos(parser, buff, tcp_hdr);       //new bufpos, packetpos happens here
      
    } //end of current buffer

    //Update all stuff outside last buffer
    parser->total += bufcount;
    printf("Read %li bytes in total\n", parser->total);
    
    if(parser->strip_packet){

      parser->deltotal += parser->delbytes;
      printf("Killed %li bytes in total\n",parser->deltotal);
      if( parser-> strip_packet ==2 ) { printf("Writing %li bytes to %s\n",parser->bufrem, parser->strip_fname); }

      if( parser->strip_packet == 2){//Up the ante

    	count = fwrite(buff, 1, parser->bufrem, parser->stripfile);
    	if( count == 0){
    	  printf("Gerrorg writing to %s\n",parser->stripfile);
    	}
    	parser->wcount += count;

    	printf("count = %i\n",count);

      }
    }

    parser->hprediction -= parser->bufrem;

    i++;

  }

  //print stats
  print_stats(parser);

  //Clean up shop
  if(datafile != NULL) fclose(datafile);
  free(filename);
  free(tcp_hdr);
  //  free_parser(parser);

  if( parser->do_chans ){
    for(int i = 0; i < numchans; i++ ){
      free_chan( chan[i] );
    }
  }
  free(parser);
  free(buff);
 
  return(EXIT_SUCCESS);
}

static void do_depart(int signum) {
  running = false;
  fprintf(stderr,"\nStopping...");
  return;
}
