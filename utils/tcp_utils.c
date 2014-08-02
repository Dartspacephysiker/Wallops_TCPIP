/*
 * tcp_utils.c
 *
 *  July 2014
 *  Author: Hammertime
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "tcp_utils.h"

#define DEBUG true
#define DEF_VERBOSE true

#define USE_CHAN(chan) c->dtype##chan

/****************/
/*INIT ROUTINES*/
/****************/
struct tcp_header *tcp_header_init(void){

  struct tcp_header *t;
  t = malloc( sizeof(struct tcp_header) );

  t->pack_sz =  0;
  t->pack_type = 0;
  t->pack_numsamps = 0;

  t->pack_totalsamps = 0;
  t->pack_time = 0;

  t->sync_numsamps = 0;

}

struct dewe_chan *chan_init(int chan_num, int dtype, bool is_asynchr, bool is_singleval){

  printf("tcp_utils.c [chan_init()] Init channel %i\n",chan_num);
  //init struct
  struct dewe_chan *c;
  c = malloc( sizeof(struct dewe_chan));
  
  c->num = chan_num;

  //init data
  if(is_singleval){
    c->is_singleval = true;
    c->dsize = chan_data_size[7]; //double float, 64-bit, as required by DEWESoft
    c->d.type7 = malloc( sizeof(double_t) );

    c->bufsize = chan_data_size[7];
    //    c->bufroom = c->bufsize;
  }
  else {
    c->dtype = dtype;
    c->dsize = chan_data_size[dtype];

    c->bufsize = chan_data_size[dtype] * MAXNUMSAMPS;

    /* //handle various channel types */
    /* switch ( dtype ) { */
    /*   case 0: */
    /* 	c->d.type0 = malloc( c->bufsize ); */
    /* 	//	USE_CHAN( dtype ) = malloc( c->bufsize ); */
    /* 	break; */
    /*   case 1: */
    /* 	c->d.type1 = malloc( c->bufsize ); */
    /* 	break; */
    /*   case 2: */
    /* 	c->d.type2 = malloc( c->bufsize ); */
    /* 	break; */
    /*   case 3: */
    /* 	c->d.type3 = malloc( c->bufsize ); */
    /* 	break; */
    /*   case 4: */
    /* 	c->d.type4 = malloc( c->bufsize ); */
    /* 	break; */
    /*   case 5: */
    /* 	c->d.type5 = malloc( c->bufsize ); */
    /* 	break; */
    /*   case 6: */
    /* 	c->d.type6 = malloc( c->bufsize ); */
    /* 	break; */
    /*   case 7: */
    /* 	c->d.type7 = malloc( c->bufsize ); */
    /* 	break;	 */
    /* } */

    if ( DEF_VERBOSE ) {
      printf("Channel data type %u: %u bytes per sample\n", dtype, chan_data_size[dtype]);
      printf("Malloc'ed %i bytes for channel %u buffer...\n", c->bufsize, c->num );
      printf("\n");
    }

    
    //    c->oldpackaddr = NULL;
    c->oldpackaddr = c->d.type3;
    c->oldnum_received = 0;
    c->oldnumsamps = 0;
    c->oldpack_ready = false;

    //    c->packaddr = NULL;
    c->packaddr = c->d.type3;
    c->num_received = 0;
    c->numsamps = 0;
    c->pack_ready = false;

    if(is_asynchr){
      c->is_asynchr = true;
      //      c->timestamps = malloc( MAXNUMSAMPS * 8); //Accommodate 64-bit timestamps
    }
    else {
      c->is_asynchr = false;
      c->timestamps = NULL; 
    }
  }
  //What kind of channel are you?
  
  if(is_singleval){
    c->is_singleval = true;
  }
  
  return c;

}

struct tcp_parser *parser_init(void){

  if( DEBUG ) printf("tcp_utils.c [parser_init()]\n");

  //  p = malloc( sizeof(struct tcp_parser) );
  struct tcp_parser *p;
  p = malloc( sizeof(struct tcp_parser));

  p->numpackets = 0;
  p->hc = 0; //tcp header count
  p->tc = 0; //tcp footer count
  p->hdrsz = 0;

  for(int i = 0; i < STARTSTR_SZ; i++){
    p->startstr[i] = '0';
  }
  p->startstr_sz = 0;

  for(int i = 0; i < STARTSTR_SZ; i++){
    p->tlstr[i] = '0';
  }
  p->tailsz = 0;

  p->oldhpos = 0; 
  p->hpos = 0;      
  p->tpos = 0;

  p->packetpos = 0;
  p->bufpos = 0;
  p->bufrem = 0;
  p->delbytes = 0;
  p->deltotal = 0;
  p->total = 0;

  p->parse_ok = false;

  p->do_chans = false;
  p->nchans = 0;
  p->npacks_combined = 0;

  p->do_predict = false;
  p->hprediction = 0;
  p->num_badp = 0;

  p->isfile = false;
  p->filesize = 0;
  p->wcount = 0;

  p->strip_packet = 0;
  p->strip_fname = NULL;
  //p->stripfile = NULL;
  p->hkill = false;
  p->oldtkill = false;
  p->tkill = false;
  p->numhkill = 0;
  p->numtkill = 0;
  p->t_in_this_buff = false;
  p->oldt_in_this_buff = false;

  p->oldheader_addr = NULL;
  p->header_addr = NULL;
  p->tail_addr = NULL;

  p->verbose = false;

}

/****************/
/*PARSE ROUTINES*/
/****************/
bool parse_tcp_header(struct tcp_parser *p, char *buf_addr, struct tcp_header *th) {

  if( DEBUG ) printf("tcp_utils.c [parse_tcp_header()]\n");

  //Look for header beginning at the current buffer position
  p->header_addr = memmem(buf_addr + p->bufpos, p->bufrem - p->bufpos, p->startstr, p->startstr_sz );

  if(p->header_addr != NULL) {

    //get new header
    //    memcpy(header, p->header_addr, p->hdrsz); //this doesn't work because of mem alignment issues with structs
    
    //start string
    memcpy(&th->start_str, p->header_addr, sizeof(th->start_str));

    //packet size
    memcpy(&th->pack_sz, (void *)((long int)p->header_addr + sizeof(th->start_str)),
	   sizeof(th->pack_sz));

    //packet type
    memcpy(&th->pack_type, 
	   (void *)((long int)p->header_addr + sizeof(th->start_str) + sizeof(th->pack_sz)), 
	   sizeof(th->pack_type));
    
    //packet number of samples
    memcpy(&th->pack_numsamps, 
	   (void *)((long int)p->header_addr + sizeof(th->start_str) + sizeof(th->pack_sz) + 
		    sizeof(th->pack_type)),
	   sizeof(th->pack_numsamps));

    //total samples
    memcpy(&th->pack_totalsamps, 
	   (void *)((long int)p->header_addr + sizeof(th->start_str) + sizeof(th->pack_sz) + 
		    sizeof(th->pack_type) + sizeof(th->pack_numsamps)),
	   sizeof(th->pack_totalsamps));

    //packet time
    memcpy(&th->pack_time, 
	   (void *)((long int)p->header_addr + sizeof(th->start_str) + sizeof(th->pack_sz) + 
		    sizeof(th->pack_type) + sizeof(th->pack_numsamps) + sizeof(th->pack_totalsamps)),
	   sizeof(th->pack_time));

    //sync number of channels
    memcpy(&th->sync_numsamps, 
	   (void *)((long int)p->header_addr + sizeof(th->start_str) + sizeof(th->pack_sz) + 
		    sizeof(th->pack_type) + sizeof(th->pack_numsamps) + sizeof(th->pack_totalsamps) +
		    sizeof(th->pack_time)),
	   sizeof(th->sync_numsamps));


    p-> numpackets +=1;
    p->hc += 1;
    p->parse_ok = true;
    
  }
  else { 
    
    p->parse_ok = false;
    
    return EXIT_FAILURE;

  }
  
  return EXIT_SUCCESS;
}

/****************/
/*PRINT ROUTINES*/
/****************/
int print_tcp_header(struct tcp_header *th){

  printf("TCP header start string =\t");
  for (int i = 0; i < STARTSTR_SZ; i ++){
    printf("%x",th->start_str[i]);
  }
  printf("\n");
  printf("Packet size:\t\t\t%"PRIi32"\n", th->pack_sz);
  printf("Packet type:\t\t\t%"PRIi32"\n", th->pack_type);
  printf("Packet number of samples:\t%"PRIi32"\n", th->pack_numsamps);
  //  printf("Total samples sent so far:\t%"PRIi64"\n", th->pack_totalsamps);
  printf("Total samples sent so far:\t%.06lli\n", th->pack_totalsamps);
  printf("Packet time:\t\t\t%f\n", th->pack_time);
  //  printf("Packet time in hex:\t%4.4LA\n", th->pack_time);
  printf("Sync channel num samples:\t%"PRIi32"\n", th->sync_numsamps);
  return EXIT_SUCCESS;
}


int print_raw_tcp_header(struct tcp_header *th){

  int i = 0;
  int j = 0;
  int rowmod = 2*STARTSTR_SZ;

  size_t hdrsz = sizeof(struct tcp_header);

  printf("\n*************\n");
  printf("Raw TCP header (%i bytes):\n",hdrsz);

  //print the whole header in a nice format
  for( i = 0; i < ( hdrsz / rowmod); i++){

    j = rowmod * i;
    //if(p->verbose){ printf("At address %p\n",header + j); }
    
    do {
      printf("%i\t",j);
      j++;      
      if (j == hdrsz){break;}

    } while( ( j % rowmod ) != 0 );

    j = rowmod * i;

    printf("\n");

    do {
      printf("%#hX\t",th->start_str[j] );
      j++;
      if (j == hdrsz)
	break;
    } while( ( j % rowmod ) != 0 );
    printf("\n");

  }
  printf("*************\n\n");

  return EXIT_SUCCESS;
}


int print_header_memberszinfo(struct tcp_header *th){

  printf("\n");
  printf("sizeof start_str\t%i\n", sizeof(th->start_str));
  printf("sizeof pack_sz:\t\t%i\n", sizeof(th->pack_sz));
  printf("sizeof pack_type:\t%i\n", sizeof(th->pack_type));
  printf("sizeof pack_numsamps:\t%i\n", sizeof(th->pack_numsamps));
  //  printf("Total samples sent so far:\t%"PRIi64"\n", sizeof(th->pack_totalsamps));
  printf("sizeof totalsamps:\t%i\n", sizeof(th->pack_totalsamps));
  printf("sizeof pack_time:\t%i\n", sizeof(th->pack_time));
  printf("sizeof sync_numsamps:\t%i\n", sizeof(th->sync_numsamps));
  return EXIT_SUCCESS;
 
}

int update_after_parse_header(struct tcp_parser *p, char * buf_addr, struct tcp_header *th){

  if( DEBUG ) printf("tcp_utils.c [update_after_parse_header()]\n");

  //update positions if new packet
  if( p->parse_ok ){
    p->hpos = (long int)p->header_addr - (long int)buf_addr;
    p->bufpos = p->hpos;
    p->packetpos = - p->startstr_sz;
  }

  if( DEBUG ) printf("tcp_utils.c [update_after_parse_header()] p->hpos:\t%li\n",p->hpos);
  if( DEBUG ) printf("tcp_utils.c [update_after_parse_header()] p->packetpos:\t%li\n",p->packetpos);
  return EXIT_SUCCESS;

}

/****************/
/*STRIP ROUTINES*/
/****************/
int prep_for_strip(struct tcp_parser *p, char * buf_addr, struct tcp_header *th){

  if( DEBUG ) printf("tcp_utils.c [prep_for_strip()]\n");

  if(p->parse_ok) {

   //if space between end of buffer and new header is less than the entire new TCP packet
    if( (p->bufrem - p->hpos ) < ( th->pack_sz + p->tailsz + p->startstr_sz ) ){

      p->t_in_this_buff = false; //expecting the tail to be in next buff

      if(p->verbose){ printf("tcp_utils.c [prep_for_strip()] WARN: Buffer runs out before packet\n"); }
      if(p->verbose){ printf("tcp_utils.c [prep_for_strip()] WARN: Current packet pos:\t%li\n", p->packetpos); }
      
    }
    else { //The buffer does contain a tail

      p->t_in_this_buff = true;

      if(p->verbose){ printf("tcp_utils.c [prep_for_strip()] Found header, and this buffer contains its corresponding footer.\n"); }
      if(p->verbose){ printf("tcp_utils.c [prep_for_strip()] Current packet pos:\t%li\n", p->packetpos); }
      if(p->verbose){ printf("tcp_utils.c [prep_for_strip()] Current headcount:\t%li\n", p->hc); }
      if(p->verbose){ printf("tcp_utils.c [prep_for_strip()] Current tailcount:\t%li\n", p->tc); }
      
    }
  }  //if header_addr is NOT null
  else { //if header_addr IS null

    //if the total left to read in the buffer is less than the total left to read in the packet (+ tail)
    if( ( p->bufrem - p->bufpos ) < ( th->pack_sz - p->packetpos + p->tailsz ) ) {
      
      p->t_in_this_buff = false;

      printf("tcp_utils.c [prep_for_strip()] Couldn't find header, and this buffer "
	     "does NOT contain the footer from the last header.\n");

    } 

    else { //what is left in the buffer DOES have a tail in it, if pack_sz isn't lying.
           //This is an extremely unlikely scenario, you know.

      p->t_in_this_buff = true; //The tail is in this buffer, children

      if(p->verbose) {
	printf("tcp_utils.c [prep_for_strip()] Couldn't find header, but this buffer "
	       "contains the footer from the last header.\n"); 
	printf("tcp_utils.c [prep_for_strip()] Current packet pos:\t%li\n", p->packetpos); 
	printf("tcp_utils.c [prep_for_strip()] Current headcount:\t%li\n", p->hc); 
	printf("tcp_utils.c [prep_for_strip()] Current tailcount:\t%li\n", p->tc); 
      }     

    }
  }

 return EXIT_SUCCESS;

}

int strip_tcp_packet(struct tcp_parser *p, char *buf_addr, struct tcp_header *th) {

  char *tmp_tail_addr;
  long int tmp_tail_pos;

  bool oldt_in_next_buff = false;

  if( DEBUG ) printf("tcp_utils.c [strip_tcp_packet()]\n");
  
  if( p->t_in_this_buff ){ //Time to get the tail
   
    //We'll just find it ourselves, since this tool needs to be independent of the program running it
    //    tmp_tail_addr = (char *)((long int)buf_addr + p->bufpos + p-> startstr_sz + th->pack_sz + 
    //			     p->tailsz - p->packetpos );
    tmp_tail_addr = (char *)((long int)buf_addr + p->bufpos + th->pack_sz - p->packetpos );

    tmp_tail_pos = (long int)tmp_tail_addr - (long int)buf_addr;
    
    if(p->verbose){ printf("tcp_utils.c [strip_tcp_packet()] p->t_in_this_buff = true\n"); }
    if(p->verbose){ printf("tcp_utils.c [strip_tcp_packet()] p->delbytes = %i\n", p->delbytes); }
    

    p->tail_addr = memmem( tmp_tail_addr, p->hdrsz + p->startstr_sz + p->tailsz, p->tlstr, p->tailsz);
    if( p->tail_addr != NULL ){

      //First, get tail pos relative to current buffer      

      p->tpos = (long int)p->tail_addr - (long int)buf_addr;
      if(p->verbose){ printf("tcp_utils.c [strip_tcp_packet()] tmp_tail_pos= %li\n", tmp_tail_pos); }
      if(p->verbose){ printf("tcp_utils.c [strip_tcp_packet()] tail_pos= %li\n", p->tpos); }
      

      //kill it
      memmove( p->tail_addr, (void *)((long int)p->tail_addr + p->tailsz), p->bufrem - p->tpos - p->tailsz);
      p->tkill = true;

      //      p->oldt_in_this_buff = false; //It WAS here, but not any more, the filthy animal

      //In principle, what is now the tail address should be the location of the next header
      /* if( strncmp( (char *)(p->tail_addr), th->start_str, p->startstr_sz) == 0){ */
      /* 	printf("tcp_utils.c [strip_tcp_packet()] Just killed tail in this buff, and the next header is RIGHT HERE\n"); */
      /* } */
    }
    else { //Where on earth is the blasted thing?
      if(p->verbose){ printf("tcp_utils.c [strip_tcp_packet()] Couldn't find tail!! What is the meaning of life?\n"); }
      oldt_in_next_buff = true; //I guess it's in the next buffer--how did this escape us?
    }

  }
  else { //See you in the next merry-go-round, tail
    oldt_in_next_buff = true;

  }

  //Now blast the obvious header (as well as the footer from the last buffer, if applicable)
  if( p->parse_ok ){ //Note, if !parse_ok then header_addr is NULL

    if(p->verbose){ printf("tcp_utils.c [strip_tcp_packet()] Blasting header...\n"); }
    //if(p->verbose){     printf("tcp_utils.c [strip_tcp_packet()] bufrem is %li\n", p->bufrem); }

    //if there is a tail nearby from the last header search, account for it here
    if( p->oldt_in_this_buff ) {

      //double check, just to be safe
      if( strncmp( (char *)((long int)p->header_addr - p->tailsz), p->tlstr, p->tailsz) == 0 ){

	p->oldtkill = true;
      	p->oldt_in_this_buff = false;

	if(p->verbose){ printf("tcp_utils.c [strip_tcp_packet()] found oldtailstring\n"); }
	for(int i = 0; i < p->tailsz; i++){
	  if(p->verbose){ printf("0x%X ",((char *)((long int)p->header_addr - p->tailsz))[i]); }
	}
	if(p->verbose){ printf("\n"); }
      
      }
      else {

	p->oldt_in_this_buff = true;

	if(p->verbose){ printf("tcp_utils.c [strip_tcp_packet()] Heard tell there was a tail here, but no sign...\n"); }

      } 
      
    }

    printf("Moving %li bytes to %p from %p\n",p->bufrem - p->hpos - p->hdrsz - (int)p->tkill * p->tailsz,
	   (void *)((long int)p->header_addr - (int)p->oldtkill * p->tailsz),
	   (void *)((long int)p->header_addr + p->hdrsz), p->bufrem - p->bufpos);
    //act in a devastating manner here
    memmove((void *)((long int)p->header_addr - (int)p->oldtkill * p->tailsz),
	    (void *)((long int)p->header_addr + p->hdrsz), 
	    p->bufrem - p->hpos - p->hdrsz - (int)p->tkill * p->tailsz);
    
    p->hkill = true;
    
  }
   
  p->oldt_in_this_buff = oldt_in_next_buff;
 
  return EXIT_SUCCESS;
}

int post_strip(struct tcp_parser *p, char *buf_addr, struct tcp_header *th){

  if( DEBUG ) printf("tcp_utils.c [post_strip()]\n");

  //  p->oldhpos -= p->tailsz * (int)p->oldtkill;
  p->hpos -= ( p->tailsz * ((int)p->oldtkill + (int)p->tkill) + p->hdrsz * p->hkill ); //p->tailsz * (int)p->oldtkill;
  p->packetpos += (int)p->hkill * (p->hdrsz) + p->tailsz * (int)p->oldtkill;
  p->bufpos -= ( p->tailsz * ((int)p->oldtkill + (int)p->tkill) + p->hdrsz * p->hkill ); 
  p->bufrem -= ( (int)p->hkill * (p->hdrsz) + p->tailsz * ((int)p->oldtkill + (int)p->tkill) );
   p->delbytes += (int)p->hkill * (p->hdrsz) + p->tailsz * ((int)p->oldtkill + (int)p->tkill);


  if( p->oldtkill ){

    if(p->verbose){ printf("tcp_utils.c [post_strip()] Killed an old tail\n"); }
    if(p->verbose){ printf("BEFORE header_addr: %p\n",p->header_addr); }
    p->header_addr -= p->oldtkill; //header_addr is a long int *, so this skips back 8 bytes
    if(p->verbose){ printf("AFTER header_addr: %p\n",p->header_addr); }

  }

  p->numhkill += (int)p->hkill;
  p->numtkill += (int)p->oldtkill + (int)p->tkill;
    
  p->hkill = false;
  p-> oldtkill = p->tkill = false;

}
/***************/
/*CHAN ROUTINES*/
/***************/
int update_chans_post_parse(struct dewe_chan *c, struct tcp_header *th, struct tcp_parser *p, char *buf_addr ){

  //NOTE, THIS WHOLE ROUTINE IS CONTIGENT UPON PARSE_OK

  if( DEBUG ) printf("tcp_utils.c [update_chans_post_parse()]\n");

  //Swap out old values
  c->oldnumsamps =  c->numsamps;
  c->oldpackaddr = c->packaddr;
  c->oldnum_received = c->num_received;
  c->oldpack_ready = c->pack_ready;

  //This is a new packet, after all
  c->numsamps = th->sync_numsamps;
  c->packaddr = c->oldpackaddr + ( c->oldnumsamps * c->dsize );  //packaddr is always oldnumsamps ahead of oldpackaddr
  c->num_received = 0;  
  c->pack_ready = false;

  if( c->is_asynchr ){
    c->oldtstamps_addr = c->tstamps_addr;
    c->tstamps_addr = c->oldpackaddr + ( c->oldnumsamps * 8 );
  }

  return EXIT_SUCCESS;
}

int get_chan_samples( struct dewe_chan *c , char *buf_addr, struct tcp_parser * p , struct tcp_header *th){

  if( DEBUG ) printf("tcp_utils.c [get_chan_samples()]\n");

  bool moresamps;
  int totdsize;

  if( c->is_asynchr ){
    totdsize = c->dsize + 8; //accommodate timestamps
  }
  else {
    totdsize = c->dsize;
  }

  if( p->parse_ok ){ //A fresh batch!

    if( ( c->oldnumsamps != 0 ) && ( c->oldnum_received != c->oldnumsamps ) ){   //handle_previous_chansamps

      int32_t oldsampsrem = c->oldnumsamps - c->oldnum_received;

      //make sure we can get all of the old samples from this buffer
      //these are all behind the new header
      if( ( p->bufrem - p->bufpos -p->hdrsz ) >= ( totdsize * ( c->oldnumsamps - c->oldnum_received ) ) ){ //we can get em all

	memcpy( (void *)((long int)c->oldpackaddr + (long int)( c->oldnum_received * c->dsize )), 
		(void *)((long int)buf_addr + p->bufpos ), oldsampsrem * c->dsize ); 

	if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH %i: bufpos before getting old samps: %li\n",
			   c->num, p->bufpos);

	p->bufpos += oldsampsrem * c->dsize;
	
	if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH %i: bufpos after getting old samps: %li\n",
			   c->num, p->bufpos);

	if( c->is_asynchr ){
	  memcpy( (void *)((long int)c->oldtstamps_addr + (long int)( c->oldnum_received * 8 )), 
		  (void *)((long int)buf_addr + p->bufpos ), oldsampsrem * 8 ); 
	  
	  if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH %i: bufpos before getting old timestamps: %li\n",
			     c->num, p->bufpos);
	  
	  p->bufpos += oldsampsrem * 8;
	  
	  if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH %i: bufpos after getting old timestamps: %li\n",
			     c->num, p->bufpos);
	  
	}
	
	//	p->bufpos += oldsampsrem * totdsize; //BETTER LINE, but use above for debug
	c->oldnum_received = c->oldnumsamps;
	c->oldpack_ready = true;
	
      }
      else { 
	printf("tcp_utils.c [get_chan_samples()] A logical contradiction. How can you have read a new header,"
	       " and yet still not have all the previous packet's samples?\n");
      } 

      if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i oldnumsamps\t=\t%i\n",c->num,c->oldnumsamps);
      if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i oldnumreceived\t=\t%i\n",c->num,c->oldnum_received);

      //skip ahead to new buffer location--doesn't matter if this isn't the last channel

    }

    //all right, now we're at bufpos
    if( p->parse_ok ){  //make sure numsamps matches with what people are telling us
      
      if( DEBUG ){
	printf("tcp_utils.c [get_chan_samples()] Numsamps in CH %i according to newparse:\t%i\n", c->num, c->numsamps);
	printf("tcp_utils.c [get_chan_samples()] Numsamps in CH %i according to bufpos:\t%i\n", c->num, *((int32_t *)(buf_addr+p->bufpos)));
	for( int i = 0; i < 10; i++){
	  printf("%i\t",i);
	}
	printf("\n");
	for( int i = 0; i < 10; i++){
	  printf("%#hX\t",(buf_addr+p->bufpos));
	}
      }
      p->bufpos += 4;
    }
    
    //if buff contains all samples for this channel
    if( ( p->bufrem - p->bufpos - p->hdrsz ) >= c->numsamps * totdsize ) { 

      if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i: This buff contains all samples\n", c->num );
      printf("tcp_utils.c [get_chan_samples()] buf_addr: %p\n", buf_addr );
      printf("tcp_utils.c [get_chan_samples()] buf_addr + p->bufpos: %p\n", buf_addr + p->bufpos );

      memcpy( c->packaddr, (void *)( (long int)p->header_addr + (long int)p->bufpos ), c->numsamps * c->dsize );
      if( c->is_asynchr ){
	memcpy( (void *)((long int)c->tstamps_addr + (long int)c->num_received), 
		(void *)((long int)buf_addr + p->bufpos ), c->numsamps * 8 ); 
      }      
      
      c->num_received = c->numsamps;
      c->pack_ready = true;
      p->bufpos += c->numsamps * totdsize;

      moresamps = true;
    } //end this buff contains all samps for this chan
    else {
      if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i: This buff DOESN'T contain all samples\n", c->num );

      memcpy( c->packaddr, (void *)((long int)buf_addr + (long int)p->bufpos ), p->bufrem - p->bufpos );
      if( c->is_asynchr ){
	memcpy( (void *)((long int)c->tstamps_addr + (long int)( c->num_received * 8 )), 
		(void *)((long int)buf_addr + p->bufpos ), c->numsamps * 8 ); 
      }      

      c->num_received += ( p->bufrem - p->bufpos ) / totdsize;
      c->pack_ready = false;
      p->bufpos = p->bufrem; //end of buffer      
      moresamps = false;
    }
  } //end parse_ok  
  else { 

    int32_t sampsrem = c->numsamps - c->num_received;

    if( ( p->bufrem - p->bufpos ) >= sampsrem * totdsize ) { //buff contains remaining samps
      memcpy( c->packaddr + ( c->num_received * c->dsize ), 
	      (void *)((long int)buf_addr + (long int)p->bufpos ), sampsrem * c->dsize );
      if( c->is_asynchr ){
	memcpy( (void *)((long int)c->tstamps_addr + (long int)( c->num_received * 8 )), 
		(void *)((long int)buf_addr + p->bufpos ), c->numsamps * 8 ); 
      }      

      c->num_received = c->numsamps;
      c->pack_ready = true;
      p->bufpos += sampsrem * totdsize;
      moresamps = true;
    }
    else {

      memcpy( c->packaddr + ( c->num_received * c->dsize ), (void *)((long int)buf_addr + p->bufpos ), p->bufrem - p->bufpos );

      c->num_received += ( p->bufrem - p->bufpos ) / c->dsize;
      c->pack_ready = false;
      p->bufpos = p->bufrem; //end of buffer
      moresamps = false;
    }
  } //end parse NOT OK

  if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i numsamps\t=\t%i\n",c->num,c->numsamps);
  if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i numreceived\t=\t%i\n",c->num,c->num_received);
  if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] Parser->bufpos\t\t=\t%i\n",c->num,p->bufpos);
  if( DEBUG  )printf("tcp_utils.c [get_chan_samples()] CH%i--moresamps\t=\t%i\n",c->num,moresamps);

  return moresamps;
}

int write_chan_samples(struct dewe_chan *c, struct tcp_parser *p, struct tcp_header *th){

  if( DEBUG ) printf("tcp_utils.c [write_chan_samples()] Channel %i\n", c->num );

  //  if


}

int combine_and_write_chandata( struct dewe_chan *c1 , struct dewe_chan *c2, int old, struct tcp_parser * p , struct tcp_header *th, FILE *outfile){

  if( DEBUG ) printf("tcp_utils.c [combine_and_write_chandata()]\n");

  unsigned int *c1_buffstart;
  unsigned int *c2_buffstart;
  long int numsamps;
  size_t dsize;
  
  if( old == 0 ){
    
    if( c1->numsamps != c2->numsamps ){
      printf("Unequal number of samples for each channel!\n");
      printf("Channel %i:\t%i\n", c1->num, c1->numsamps);
      printf("Channel %i:\t%i\n", c2->num, c2->numsamps);
      return EXIT_FAILURE;
    }
    numsamps = c1->numsamps;
    
    c1_buffstart = (unsigned int *)c1->packaddr;
    c2_buffstart = (unsigned int *)c2->packaddr;
  }
  else {
    
    if( c1->oldnumsamps != c2->oldnumsamps ){
      printf("Unequal number of old samples for each channel!\n");
      printf("(old)Channel %i:\t%i\n", c1->num, c1->oldnumsamps);
      printf("(old)Channel %i:\t%i\n", c2->num, c2->oldnumsamps);
      return EXIT_FAILURE;
    }
    numsamps = c1->oldnumsamps;
    
    c1_buffstart = (unsigned int *)c1->oldpackaddr;
    c2_buffstart = (unsigned int *)c2->oldpackaddr;
  }
  
  if( c1->dsize != c2->dsize ){
    printf("Unequal data sizes for each channel!\n");
    printf("Channel %i datasize:\t%i\n", c1->num, c1->dsize);
    printf("Channel %i datasize:\t%i\n", c2->num, c2->dsize);
    return EXIT_FAILURE;
  }
  dsize = c1->dsize;
  
  if( ( c1_buffstart != NULL ) && ( c2_buffstart != NULL ) ){
    uint16_t datum;
    for(int samp = 0; samp < numsamps; samp++){
      datum = join_upper10_lower6( *(c1_buffstart + samp), *(c2_buffstart + samp), 0);
      fwrite( &datum, dsize, 1, outfile);
    }
  }
  else {
    printf("tcp_utils.c [combine_and_write_chandata()] You just tried to slip me a null pointer! You code like a graduate student.\n");
    return EXIT_FAILURE;
  }

  if( DEF_VERBOSE ) printf("Combined and wrote %li samps (%li bytes) to file\n", numsamps, numsamps * dsize );

  return EXIT_SUCCESS;
}

//NOT FINISHED! Position reporting isn't clear yet
int print_chan_info(struct dewe_chan *c){
  
  if( DEBUG ) printf("tcp_utils.c [print_chan_info()]");
  
  printf("Channel %i info:\n",c->num);
  printf("New samples start address:\t\t%p\n", c->packaddr);
  printf("\tSample position in chan buff:\t%i\n", (long int)c->num_received );
  printf("\tNumber of samples:\t\t%i\n", c->numsamps );
  printf("\tNum received:\t\t%i\n", c->num_received );
  //  printf("\tNum not received:\t%i\n", c->num_not_received );
  printf("\n");
  printf("Old samples start address:\t%p\n", c->oldpackaddr);
  printf("\tOld sample position:\t%i\n", c->oldnum_received );
  printf("\tOld number of samples:\t%i\n", c->oldnumsamps );
  printf("\tOldnum received:\t%i\n", c->oldnum_received );
  //  printf("\tOldnum not received:\t%i\n", c->oldnum_not_received );

}

//This function assumes that sizeof(short) >= 2 bytes
int16_t join_chan_bits(char a, char b)
{
  return (a<<8) + b;
}


//This one is for combining the 10 upper bits we're getting from one DEWESoft channel
//and the 6 lower bits we're gettin from the other. Presumably they both come through as 
//16-bit data, and if they're in network order we need to take stock of that
//If so, use ntohs(var) ("network to host short"), assuming sizeof(short) >= 2. 
uint16_t join_upper10_lower6(uint16_t upper, uint16_t lower, bool from_network){

  if(from_network){
    //just in case lower has some garbage bits set above the lowest 6
    //lower &= ~(0b0000001111111111);
    return ( ntohs(upper) << 10) ^ ntohs(lower);
  } 
  else {
    //just in case lower has some garbage bits set above the lowest 6
    //    lower &= ~(0b1111111111 << 6);
    return (upper << 10) ^ lower;
  }
}


//Here's some code to test out the above functions:
  /* //test out this newfangled combine bit thing */
  /* unsigned char a = 0b11111111; */
  /* unsigned char b = 1; */
  /* printf("char a = 0X%hX\n",a); */
  /* printf("char b = 0X%hX\n",b); */
  /* printf("char a<<8 = 0X%hX\n",(a<<8)); */
  /* printf("return of join_chan_bits(a,b): 0X%hX\n",join_chan_bits(a,b)); */

  //also test this newfangled upper10_lower6
  /* unsigned short c = 0b1111111111111111; */
  /* unsigned short d = 0b110000000111111; */
  /* //result should be 0xFFFF for first case */
  /* printf("\n"); */
  /* printf("ushort c = 0X%hX\n",c); */
  /* printf("ushort d = 0X%hX\n",d); */
  /* printf("ushort c<<6 = 0X%hX\n",(c<<6)); */
  /* printf("d &= ~(0b1111111111 << 6): 0X%hX\n", (d &= ~(0b1111111111 << 6))); */
  /* printf("(c<<6) ^ d = 0X%hX\n",((c<<6) ^ d)); */
  /* printf("return of joinupper10_lower6(c,d,0): 0X%hX\n",join_upper10_lower6(c,d,0)); */
  /* printf("return of joinupper10_lower6(c,d,1): 0X%hX\n",join_upper10_lower6(c,d,1)); */

int clean_chan_buffer(struct dewe_chan *c){

  if( DEBUG ) printf("tcp_utils.c [clean_chan_buffer()] CH%i\n", c->num);

  //well, if the current packet is ready the old one must be too or must have already been handled, so it doesn't
  //hurt to reset oldpack stuff along the way
  if( c->oldpack_ready || c->pack_ready ){ 

    if( DEF_VERBOSE ) printf("Cleared old packet data for CH%i...", c->num);

    //now, if the new packet got handled as well
    if( c->pack_ready ){ //double bonus      

      c->num_received = 0; 
      c->numsamps = 0;
      c->pack_ready = false;

      if( DEF_VERBOSE ) printf("AND new packet data!\n");	  
    }
    else{ 
      memmove( c->d.type3, c->packaddr, c->num_received *  c->dsize ); //move new packet data to front of channel buff 
      if( c->is_asynchr ) memmove( c->timestamps, c->tstamps_addr, c->num_received * 8 ); //also timestamps, if applicable
      if( DEF_VERBOSE ) printf("but current packet isn't ready!\n");
      if( DEF_VERBOSE ) printf("\n");  
    }

    c->oldpackaddr = c->packaddr = c->d.type3; //beginning of buffer for everyone
    c->oldnum_received = 0;
    c->oldnumsamps = 0;
    c->oldpack_ready = false;

    if( c->is_asynchr ){
      c->oldtstamps_addr = c->tstamps_addr = c->timestamps;
    }


  }
  /* else if( c->pack_ready ){ //bogus situation */
  /*   if( DEF_VERBOSE ) printf("tcp_utils.c [clean_chan_buffer()] BOGUS!!\n"); */
  /* } */
}

int update_end_of_buff(struct tcp_parser *p, char *buf_addr, struct tcp_header *th){

  if( DEBUG ) printf("tcp_utils.c [update_end_of_buff()]\n");
	
  if( p->parse_ok ){

    if(p->verbose){ printf("tcp_utils.c [update_end_of_buff()] p->parse_ok. Updating oldheader.\n"); }
    //new header parsed OK, so its address is qualified to serve as the oldheader address
    p->oldheader_addr = p->header_addr;
    p->oldhpos = p->hpos;
    
    
    //If the current packet extends beyond the current buffer of data, 
    //let user know and appropriately set packetpos and bufpos for next readthrough
    if( (p->bufrem - p->bufpos ) < ( th->pack_sz - p->packetpos + p->tailsz ) ){

      p->packetpos += p->bufrem - p->bufpos;
      p->bufpos = p->bufrem;

      if(p->verbose){ printf("tcp_utils.c [update_end_of_buff()] WARN: Buffer runs out before packet\n"); }
      if(p->verbose){ printf("tcp_utils.c [update_end_of_buff()] WARN: Current packet pos:\t%li\n", p->packetpos); }
      
    }
    else { //The buffer does contain the beginning of the next packet

      p->packetpos = th->pack_sz;
      p->bufpos = p->hpos + th->pack_sz;

      if( p->strip_packet ){
	p->t_in_this_buff = true;
      }
      if(p->verbose){ printf("tcp_utils.c [update_end_of_buff()] Found header, and this buffer contains its corresponding footer.\n"); }
      if(p->verbose){ printf("tcp_utils.c [update_end_of_buff()] Current packet pos:\t%li\n", p->packetpos); }
      if(p->verbose){ printf("tcp_utils.c [update_end_of_buff()] Current headcount:\t%li\n", p->hc); }
      if(p->verbose){ printf("tcp_utils.c [update_end_of_buff()] Current tailcount:\t%li\n", p->tc); }
      
    } 
  }
  else { //No header found during this cycle
        
    //if the total left to read in the buffer is less than the total left to read in the packet (+ tail)
    //    if( ( p->bufrem - p->bufpos ) <= ( th->pack_sz - p->packetpos + p->tailsz ) ) {
    
    if( th->pack_sz - p->packetpos != 0 ){
      p->packetpos += p->bufrem - p->bufpos;
    }
    //    }
    
    if(p->verbose){ printf("header_addr search came up NULL\n"); }
    if(p->verbose){ printf("--->oldhpos is currently %li\n",p->oldhpos); }
    //Need to adjust oldhpos since its location was relative to an old buffer of data
    //Specifically, it needs to be a negative number to indicate it was found some number of bytes
    //BEFORE the current buffer of data
    //	p->
    //    p->oldhpos -= p->bufrem;
    if(p->verbose){ printf("--->oldhpos is NOW %li\n",p->oldhpos); }
    
    //if(p->verbose){ 	printf("bufrem is now %li\n",p->bufrem); }
    //if(p->verbose){ 	printf("bufpos is now %li\n",p->bufpos); }
    
    //skip these to end of buff, since we couldn't find a header and there's no way it's here
    p->bufpos = p->bufrem; //at end of buffer
    
    
  }
}

/**************/
/*FINISHER ROUTINES*/
/**************/
void print_stats(struct tcp_parser *p){

  printf("\n\n**********STATS**********\n\n");

  printf("Number of packets read:\t%u\n",p->numpackets);
  printf("Total header count:\t%u\n",p->hc);
  printf("Total footer count:\t%u\n",p->tc);
  printf("\n");
  printf("Incorrect header predictions:\t%u\n",p->num_badp);
  printf("\n");
  printf("Total bytes read: %li\n",p->total);
  printf("Actual file size: %li\n",p->filesize);

  if( p->strip_packet ){

    printf("\n");
    printf("Total headers killed: %u\n", p->numhkill);
    printf("Total footers killed: %u\n", p->numtkill);
    printf("Total bytes stripped: %u\n", p->deltotal);
    if( p->strip_packet == 2 ){
      printf("Wrote %li bytes to file %s\n", p->wcount, p->strip_fname);
    }
  }

  if( p->do_chans ){
    
    printf("\n");
    printf("Number of channels:\t%i\n", p->nchans);
    if( p->do_chans == 3 ) printf("Number of packets combined:\t%i\n", p->npacks_combined );
    printf("\n");
  }

  printf("\n*************************\n\n");
  
  
}

/* void print_chan_stats(struct dewe_chan **c_arr, struct tcp_parser *p){ */

/*   printf("\n******CH%i STATS******\n\n"); */

/*   if( p->do_chans == 3){ */
/*     printf("Num packets combined:\t%i\n",p->packs_combined); */
/*   } */

/*   printf("\n*************************\n\n"); */
/* } */

void free_chan(struct dewe_chan *c){

  //init data
  if(c->is_singleval){
    free( c->d.type7 );
  }
  else {
    //handle various channel types
    switch ( c->dtype ) {
      case 0:
	free( c->d.type0 );
	break;
      case 1:
	free( c->d.type1 );
	break;
      case 2:
	free( c->d.type2 );
	break;
      case 3:
	free( c->d.type3 );
	break;
      case 4:
	free( c->d.type4 );
	break;
      case 5:
	free( c->d.type5 );
	break;
      case 6:
	free( c->d.type6 );
	break;
      case 7:
	free( c->d.type7 );
	break;	
    }

    if(c->is_asynchr){
      free( c->timestamps );
    }
  
    free(c);

  }
}

void free_parser(struct tcp_parser *p){

  if (p->oldheader_addr != NULL )free(p->oldheader_addr);
  if (p->header_addr != NULL )free(p->header_addr);
  if (p->tail_addr != NULL )free(p->tail_addr);
  if (p->strip_packet = 2){
    if(p->stripfile != NULL)free(p->strip_fname);
    if(p->stripfile != NULL) free(p->stripfile);
  }

  free(p); //This doesn't work quite right because some pointers never get assigned anything but NULL
}
 
