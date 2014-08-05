/* tcp_fileparse
 *  ->The only program you'll ever need for stripping TCP headers from DEWESoft interfaces
 *  ->at the NASA Wallops Flight Facility in Wallops Island, VA, GUARANTEED
 *  ->(unless you don't have a sense of humor)  
 *    
 *
 * se creó y encargó : Jul 25 (fifteen days post-Sarah's birthday), 2014
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

#define DEBUG true
#define DEF_VERBOSE true

#define DEFMAX_BUFSZ 32768

#define DEF_NUMCHANS 2
#define MAX_NUMCHANS 4

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

  //TCP stuff
  struct tcp_header *tcp_hdr;
  struct tcp_parser *parser;
  //  bool careful_strip = true; //ensures things are where they say they are before junking them
  //  FILE *stripfile;
  char tcp_str[16] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };

  struct dewe_chan *chan[MAX_NUMCHANS];
  char *chanbuff[MAX_NUMCHANS];
  char *chantimestamps[MAX_NUMCHANS];
  char combfname[] = "combined_chans.data";
  FILE *combfile; //File for combining upper 10, lower 6 bits of two different channels
                  //to accommodate the strange 10-bit TM data at Wallops
  
  signal(SIGINT, do_depart);

  //tcp header stuff
  tcp_hdr = tcp_header_init();

  //tcp parser stuff
  parser = parser_init();

  parser->hdrsz = 32 + STARTSTR_SZ; //per DEWESoft NET interface docs

  //copy in start and tail string for use by parse_tcp_header() and strip_tcp_packet()
  if( DEF_VERBOSE ) printf("tcp_fileparse.c [main()] Start string:\t");
  for (int i = 0; i < STARTSTR_SZ; i ++){
    strncpy(&(parser->startstr[i]),&(tcp_str[8+i]),1);
    if ( DEF_VERBOSE ) printf("%x",parser->startstr[i]);
  }
  if( DEF_VERBOSE ) printf("\n");
  parser->startstr_sz = STARTSTR_SZ;

  if( DEF_VERBOSE ) printf("tcp_fileparse.c [main()] Tail string:\t");
  strncpy(parser->tlstr,tcp_str,STARTSTR_SZ); 
  if (DEF_VERBOSE ){
    for (int i = 0; i < STARTSTR_SZ; i ++){
      printf("%x",parser->tlstr[i]);
    }
    printf("\n");
  }
  parser->tailsz = STARTSTR_SZ;

  parser->oldhpos = -(parser->hdrsz + parser->tailsz); //Needs to be initialized thusly so that 
                                                       //parse_tcp_header doesn't complain that 
                                                       //the first header isn't where predicted
  parser->do_predict = true;
  parser->isfile = true;

  parser->verbose = DEF_VERBOSE;

  //Handle command line  
  if(argc == 2) {
    filename = strdup(argv[1]);
  }
  else if ( argc == 3 ){
    filename = strdup(argv[1]);
    max_bufsz = atoi(argv[2]);
  }
  else if( ( argc == 4 ) || ( argc == 5) ){

    filename = strdup(argv[1]);
    max_bufsz = atoi(argv[2]);

    //Strip packet modes
    if( atoi(argv[3]) <= 2){ 
      parser->strip_packet = atoi(argv[3]);
      parser->strip_fname = malloc(sizeof(char) * 128);
      parser->strip_fname = "stripped.data";
      //    sprintf(parser->strip_fname,"stripped-%s",filename);
      parser->oldt_in_this_buff = 0;
      parser->t_in_this_buff = 0;
    }
    //Channel modes
    else if( ( atoi(argv[3]) == 4 ) ||  ( atoi(argv[3]) == 5 ) || ( atoi(argv[3]) == 6 ) ){ // only parse chan info 
      parser->do_chans = atoi(argv[3]) - 3;
      if( argc ==5 ){ //channel num provided
	parser->nchans = atoi(argv[4]);
      }
      else {
	parser->nchans = DEF_NUMCHANS;
      }
      if( DEF_VERBOSE ) printf("tcp_fileparse.c [main()] parser->nchans\t=\t%i\n",parser->nchans);
    }
    else {
      printf("You fool!!! You expect this program to clean your room as well?\n");
      return EXIT_FAILURE;
    }
  }
  else {
    printf("%s <tcp data file> <optional max readbuffer size> <run_mode> [for run_mode 4/5: NUM_CHANS]\n",argv[0]);
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
  if( DEF_VERBOSE ) printf("Max buffer size:\t%i\n", max_bufsz);
 
  //tcp chan stuff
  if( parser->do_chans ){
    for (int i = 0; i < parser->nchans; i ++) {

      chan[i] = chan_init( i, 3, true, false); //channel num, data type 3 (16-bit unsigned int), async, not singleval

      if( ( parser->do_chans == 2 ) || ( parser->do_chans == 3 ) ){ //open files for chandata
	sprintf(chan[i]->outfname,"chan%i.data",i);
	chan[i]->outfile = fopen(chan[i]->outfname,"w");
	if (chan[i]->outfile == NULL) {
	  fprintf(stderr,"Gerrorg. Couldn't open %s for channel %i.\n", chan[i]->outfname, i );
	  return(EXIT_FAILURE);
	}
	if( DEF_VERBOSE ){
	  printf("Channel %i file: %p\n", i, chan[i]->outfile );
	  printf("Channel %i filename: %s\n", i, chan[i]->outfname);
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
	if( DEF_VERBOSE ) printf("tcp_fileparse.c [main()] Malloc'ed %i bytes for channel %u timestamps buffer...\n", MAXNUMSAMPS * 8, chan[i]->num );
      }
    }
    if( parser->do_chans == 3){ //doing join_upper10_lower6
      combfile = fopen(combfname,"w");
      if (combfile == NULL) {
	fprintf(stderr,"Gerrorg. Couldn't open %s.\n", combfname );
	return(EXIT_FAILURE);
      }
      if( DEF_VERBOSE ){
	printf("Combined data filename: %s\n", combfname);
	printf("Combined data file: %p\n", combfile );
      }
    }
    else {
      combfile = NULL;
    }	
  }  

  //If stripping data, set up file for stripped data
  if(parser->strip_packet == 2){
    printf("stripfname: %s\n",parser->strip_fname);
    parser->stripfile = fopen(parser->strip_fname,"w");
    printf("Stripfile: %p\n",parser->stripfile);
    if (parser->stripfile == NULL) {
      fprintf(stderr,"Gerrorg. Couldn't open stripfile %s.\n",parser->strip_fname);
      return(EXIT_FAILURE);
    }
  }

  if( DEF_VERBOSE ) print_header_memberszinfo(tcp_hdr);

  //Prediction stuff
  if(parser->do_predict){
    parser->hprediction = 0; //At the beginning, we predict that the header will be right at the beginning!
    parser->num_badp = 0;
  }

  //Start looping through file
  printf("Parsing TCP data file %s...\n",filename);
  running = true;
  int i = 0;
  bufsz = max_bufsz;
  int count = 0;
  while( ( bufcount = fread(buff, 1, bufsz, datafile) ) > 0 && running ) {
    
    printf("\n***\nBuffer #%i\n***\n",i+1);

    parser->bufrem = bufcount;
    parser->bufpos = 0;
    parser->delbytes = 0;

    while(  parser->bufpos < parser->bufrem ){

      if(parser->strip_packet) { usleep(4000); }
	else { usleep(6000); }

      parse_tcp_header(parser, buff, tcp_hdr);       //get new header_addr here

      update_after_parse_header(parser, buff, tcp_hdr);       //new hpos, bufpos, packetpos, if applicable 

      if( parser->strip_packet ){

	prep_for_strip(parser, buff, tcp_hdr); 	//determines whether there are footers to kill
	strip_tcp_packet(parser, buff, tcp_hdr); 	//do the deed

	if( parser->do_predict ) { 	//update our prediction accordingly	  
	  parser->hprediction -= ( ( (int)parser->oldtkill + (int)parser->tkill) * parser->tailsz +
				   parser->hkill * parser->hdrsz );	  
	}	
	post_strip(parser, buff, tcp_hdr); 	//finish the job	
      } //end strip_packet

      //Channel stuff
      if( parser->do_chans ){

      	bool moresamps = true;      
      	long int tmp_buf_pos = parser->bufpos;

  	if( parser->parse_ok ){

	  //get new packet stuff, if applicable
	  for(int i = 0; i < parser->nchans; i++){
	    update_chans_post_parse( chan[i], tcp_hdr, parser, buff );
	  }
	  
	  parser->bufpos = 0; //temp set to zero because we want everything in the buffer BEHIND the new header
	  
	  //wrap up old channel data, which must be here since we got a new header
	  for(int i = 0; i < parser->nchans; i++){
	    if( parser->bufpos < parser->hpos ){
	      if( DEBUG ) {
		printf("tcp_fileparse.c [main()] CH%i: Doing old samples\n", i );
		printf("tcp_fileparse.c [main()] CH%i: Bufpos = %li\n", i, parser->bufpos );
		printf("tcp_fileparse.c [main()] CH%i: hpos = %li\n", i, parser->hpos );
	      }
	      get_chan_samples( chan[i], buff, parser, tcp_hdr, true);
	    }
	  }
	  if( DEBUG ) {
	    printf("tcp_fileparse.c [main()] Finished oldsamps. Bufpos should be right behind hpos!\n");
	    printf("tcp_fileparse.c [main()] hpos = %li, bufpos == %li\n", parser->hpos, parser->bufpos);
	  }
	  parser->bufpos =  parser->hpos + parser->hdrsz - 4; //skip header for next get_chan_samples
	}  
	  
	moresamps = true; //reset for next bit
      

	for(int j = 0; j < parser->nchans; j++ ){
	  if( moresamps ) {
	    moresamps = get_chan_samples( chan[j], buff, parser, tcp_hdr, false);
	    //	      parser->bufpos += 4; 
	  } else { break; }
	}
	
      	parser->bufpos = tmp_buf_pos; //set it to what it was before channels messed with it

	if( parser->do_chans > 1 ){ //time to write chan data
	  
	  int npacks_ready = 0;
	  int noldpacks_ready = 0;
	  
	  for(int i = 0; i < 2; i ++){
	    noldpacks_ready += (int) chan[i]->oldpack_ready;
	    npacks_ready += (int) chan[i]->pack_ready;
	    
	    if( DEF_VERBOSE ){ 
	      printf("Chan %i old packet ready to combine: %i\n", i, chan[i]->oldpack_ready);
	      printf("Chan %i new packet ready to combine: %i\n", i, chan[i]->pack_ready);
	    }
	  }
	  if( npacks_ready = 2 ) {
	    combine_and_write_chandata( chan[0], chan[1], 0, parser, tcp_hdr, combfile );
	  }
	  
	  if( noldpacks_ready = 2 ){
	    combine_and_write_chandata( chan[0], chan[1], 1, parser, tcp_hdr, combfile );
	  }
	  else {
	  printf ("Not all channels are prepared to do data combination!\n");
	  }
	} //end combine channel data

	//Get new packet info, if applicable
	/* if( parser->parse_ok ){ */
	/*   for(int i = 0; i < parser->nchans; i++){ */
	/*     update_chans_post_parse( chan[i], tcp_hdr, parser, buff ); */
	/*   } */
	/* } */

      } //end do_chans
    
      if( parser->parse_ok ){
	
	printf("*****Packet #%u*****\n",parser->numpackets);
	print_tcp_header(tcp_hdr);
	if( DEF_VERBOSE ) print_raw_tcp_header(tcp_hdr);
	
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

      
      if( parser ->do_chans ){                //clean up chans, reset buffs if possible
	for(int i = 0; i < parser->nchans; i++){
	  print_chan_info( chan[i] );
	  clean_chan_buffer( chan[i] );
	}
      }

      update_end_of_loop(parser, buff, tcp_hdr);       //new bufpos, packetpos happens here
      
    } //end of current buffer

    //Update all stuff outside last buffer
    parser->total += bufcount;
    printf("Read %li bytes so far\n", parser->total);
    
    if(parser->strip_packet){

      parser->deltotal += parser->delbytes;
      printf("Killed %li bytes so far\n",parser->deltotal);
      if( parser-> strip_packet ==2 ) { printf("Writing %li bytes to %s\n",parser->bufrem, parser->strip_fname); }

      if( parser->strip_packet == 2){//Up the ante

    	count = fwrite(buff, 1, parser->bufrem, parser->stripfile);
    	if( count == 0){
    	  printf("Gerrorg writing to %s\n",parser->strip_fname);
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
  /* if( parser->do_chans ) { for(int i = 0; i < p->nchans; i++ ){ print_chan_stats(chan, p); } } */

  //Clean up shop
  if(datafile != NULL) fclose(datafile);
  free(filename);
  free(tcp_hdr);
  if( parser->do_chans ){
    for(int i = 0; i < parser->nchans; i++ ){
      if( ( parser->do_chans == 2 ) || ( parser->do_chans == 3 ) ){ //open files for chandata
	free( chan[i]->outfile );
      }	
      //      free_chan( chan[i] );
      free( chan[i] );
    }
    //chan buffs
    for(int i = 0; i < parser->nchans; i++){
      free ( chanbuff[i] );
      if( chan[i]->is_asynchr ){
	free ( chantimestamps[i] );
      }
    }
    
    
    if( parser->do_chans == 3){ //doing join_upper10_lower6
      free( combfile );
    }
  }
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
