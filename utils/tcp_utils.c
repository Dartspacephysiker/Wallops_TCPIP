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

  return t;

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
      printf("tcp_utils.c [chan_init()] Channel data type %u: %u bytes per sample\n", dtype, chan_data_size[dtype]);
      printf("tcp_utils.c [chan_init()] Malloc'ed %li bytes for channel %u buffer...\n", c->bufsize, c->num );
      printf("\n");
    }

    
    //    c->oldpackaddr = NULL;
    c->oldpackaddr = c->d.type3;
    c->oldnumsampbytes = 0;
    c->oldnumbytes_received = 0;
    c->oldnumsamps = 0;
    c->oldnum_received = 0;
    c->oldpack_ready = false;

    //    c->packaddr = NULL;
    c->packaddr = c->d.type3;
    c->numsampbytes = 0;
    c->numbytes_received = 0;
    c->numsamps = 0;
    c->num_received = 0;
    c->pack_ready = false;

    if(is_asynchr){
      c->is_asynchr = true;
      //      c->timestamps = malloc( MAXNUMSAMPS * 8); //Accommodate 64-bit timestamps

      c->oldnumtbytes = 0;
      c->oldtbytes_received = 0;
      
      c->numtbytes = 0;
      c->tbytes_received = 0;
      
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
  
  c->packs_tofile = 0;

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
  p->bufpos = 0; //at first bit
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
  
  return p;

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
  printf("Total samples sent so far:\t%"PRIi64"\n", th->pack_totalsamps);
  //printf("Total samples sent so far:\t%.06lli\n", th->pack_totalsamps);
  printf("Packet time:\t\t\t%f\n", th->pack_time);
  //  printf("Packet time in hex:\t%4.4LA\n", th->pack_time);
  printf("Sync channel num samples:\t%"PRIi32"\n", th->sync_numsamps);
  return EXIT_SUCCESS;
}


int print_raw_tcp_header(struct tcp_header *th){

  int i = 0;
  int j = 0;
  int rowmod = 16;

  //  size_t hdrsz = sizeof(struct tcp_header);
  int hdrsz = sizeof(th->start_str) + sizeof(th->pack_sz) + sizeof(th->pack_type) + 
    sizeof(th->pack_numsamps) + sizeof(th->pack_totalsamps) + sizeof(th->pack_time) + sizeof(th->sync_numsamps);
  printf("\n*************\n");
  printf("Raw TCP header (%i bytes):\n", hdrsz );

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
  printf("tcp_utils.c [print_header_memberszinfo()]\n");
  printf("tcp_utils.c [print_header_memberszinfo()] sizeof start_str\t%lu\n", sizeof(th->start_str));
  printf("tcp_utils.c [print_header_memberszinfo()] sizeof pack_sz:\t\t%lu\n", sizeof(th->pack_sz));
  printf("tcp_utils.c [print_header_memberszinfo()] sizeof pack_type:\t%lu\n", sizeof(th->pack_type));
  printf("tcp_utils.c [print_header_memberszinfo()] sizeof pack_numsamps:\t%lu\n", sizeof(th->pack_numsamps));
  printf("tcp_utils.c [print_header_memberszinfo()] sizeof totalsamps:\t%lu\n", sizeof(th->pack_totalsamps));
  printf("tcp_utils.c [print_header_memberszinfo()] sizeof pack_time:\t%lu\n", sizeof(th->pack_time));
  printf("tcp_utils.c [print_header_memberszinfo()] sizeof sync_numsamps:\t%lu\n", sizeof(th->sync_numsamps));
  printf("tcp_utils.c [print_header_memberszinfo()] total header size: %lu\n", sizeof(th->start_str) + 
	 sizeof(th->pack_sz) + sizeof(th->pack_type) + sizeof(th->pack_numsamps) + 
	 sizeof(th->pack_totalsamps) + sizeof(th->pack_time) + sizeof(th->sync_numsamps));
  return EXIT_SUCCESS;
 
}

int update_after_parse_header(struct tcp_parser *p, char * buf_addr, struct tcp_header *th){

  if( DEBUG ) printf("tcp_utils.c [update_after_parse_header()]\n");

  //update positions if new packet
  if( p->parse_ok ){
    p->hpos = (long int)p->header_addr - (long int)buf_addr;
    p->bufpos = p->hpos;
    p->packetpos = 0;
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
      if(p->verbose){ printf("tcp_utils.c [prep_for_strip()] Current headcount:\t%i\n", p->hc); }
      if(p->verbose){ printf("tcp_utils.c [prep_for_strip()] Current tailcount:\t%i\n", p->tc); }
      
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
	printf("tcp_utils.c [prep_for_strip()] Current buffer pos:\t%li\n", p->bufpos); 
	printf("tcp_utils.c [prep_for_strip()] Current headcount:\t%i\n", p->hc); 
	printf("tcp_utils.c [prep_for_strip()] Current tailcount:\t%i\n", p->tc); 
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
    tmp_tail_addr = (char *)((long int)buf_addr + p->bufpos + th->pack_sz - p->packetpos - p->tailsz );
    tmp_tail_pos = (long int)tmp_tail_addr - (long int)buf_addr;
    
    if(p->verbose){ printf("tcp_utils.c [strip_tcp_packet()] p->t_in_this_buff = true\n"); }
    if(p->verbose){ printf("tcp_utils.c [strip_tcp_packet()] p->delbytes = %li\n", p->delbytes); }
    

    p->tail_addr = memmem( tmp_tail_addr, p->hdrsz + p->startstr_sz, p->tlstr, p->tailsz);
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
      if( DEBUG ){
	//	long int hdrspace = 
	//	p->tail_addr = memmem( p->oldheader_addr, p->bufrem - p->oldhpos, p->tlstr, p->tailsz);
	p->tail_addr = memmem( buf_addr, p->bufrem, p->tlstr, p->tailsz);
	if( p->tail_addr != NULL ){
	  p->tpos = (long int)p->tail_addr - (long int)buf_addr;
	  printf("tcp_utils.c [strip_tcp_packet()] Missed tail by %li bytes\n", 
		 (long int)tmp_tail_addr - (long int)p->tail_addr);
	  printf("tcp_utils.c [strip_tcp_packet()] tmp_tail_pos=\t\t%li\n", tmp_tail_pos);
	  printf("tcp_utils.c [strip_tcp_packet()] tail_pos=\t\t\t%li\n", p->tpos);
	  printf("tcp_utils.c [strip_tcp_packet()] Bytes after tailpos:\t%li\n", p->bufrem - p->tpos);
	}
      }
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
	   (void *)((long int)p->header_addr + p->hdrsz));
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

  int killedbytes = (int)p->hkill * (p->hdrsz) + p->tailsz * ((int)p->oldtkill + (int)p->tkill);

  if( DEBUG ) printf("tcp_utils.c [post_strip()]\n");

  //  p->oldhpos -= p->tailsz * (int)p->oldtkill;
 
  p->hpos -= ( p->tailsz * ((int)p->oldtkill + (int)p->tkill) + p->hdrsz * p->hkill ); //p->tailsz * (int)p->oldtkill;
  //  p->packetpos += (int)p->hkill * (p->hdrsz) + p->tailsz * (int)p->oldtkill;
  //  p->bufpos -= ( p->tailsz * ((int)p->oldtkill + (int)p->tkill) + p->hdrsz * p->hkill ); 
  //  p->packetpos += killedbytes;
  p->bufpos -= killedbytes;
  p->bufrem -= killedbytes;
  p->delbytes += killedbytes;
  if( DEBUG ) printf("tcp_utils.c [post_strip()] Killing %i bytes...\n", killedbytes);

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

  return EXIT_SUCCESS;
}
/***************/
/*CHAN ROUTINES*/
/***************/
int update_chans_post_parse(struct dewe_chan *c, struct tcp_header *th, struct tcp_parser *p, char *buf_addr ){

  //NOTE, THIS WHOLE ROUTINE IS CONTIGENT UPON PARSE_OK

  if( DEBUG ) printf("tcp_utils.c [update_chans_post_parse()]\n");

  //Swap out old values
  c->oldpackaddr = c->packaddr;
  c->oldnumbytes_received = c->numbytes_received;
  c->oldnumsampbytes = c->numsampbytes;
  c->oldnum_received = c->num_received;
  c->oldnumsamps =  c->numsamps;
  c->oldpack_ready = c->pack_ready;

  //This is a new packet, after all
  c->packaddr = c->oldpackaddr + c->oldnumsampbytes;  //packaddr is always oldnumsamps ahead of oldpackaddr
  c->numsampbytes = th->sync_numsamps *  c->dsize;
  c->numbytes_received = 0;
  c->numsamps = th->sync_numsamps;
  c->num_received = 0;  
  c->pack_ready = false;

  if( c->is_asynchr ){
    c->oldnumtbytes = c-> numtbytes;
    c->oldtbytes_received = c->tbytes_received;
    c->oldtstamps_addr = c->tstamps_addr;

    c->numtbytes = th->sync_numsamps * 8;
    c->tbytes_received = 0;
    c->tstamps_addr = c->oldtstamps_addr + c->oldnumtbytes;
    //    c->tstamps_addr = c->oldpackaddr + ( c->oldnumsamps * 8 ); //THIS WAS WRONG IN ANY CASE
  }

  return EXIT_SUCCESS;
}

int get_chan_samples( struct dewe_chan *c, char *buf_addr, struct tcp_parser * p , struct tcp_header *th, bool old){

  bool moresamps;

  if( DEBUG ) {
    printf("tcp_utils.c [get_chan_samples()]\n");
    printf("tcp_utils.c [get_chan_samples()] CH%i looking for old samples: %i\n", c->num, old);
    printf("tcp_utils.c [get_chan_samples()] buf_addr:\t\t\t\t\t%p\n", buf_addr );
    printf("tcp_utils.c [get_chan_samples()] buf_addr + p->bufpos:\t\t\t\t%p\n", buf_addr + p->bufpos );
  }

  //handle old samples for this channel
  //only happens when we get a new packet
  if( old ){

      //      int32_t oldsampsrem = c->oldnumsamps - c->oldnum_received;
      int32_t oldbytesrem = c->oldnumsampbytes - c->oldnumbytes_received;
      int32_t oldtbytesrem = c->oldnumtbytes - c->oldtbytes_received; 
      int32_t totoldbytesrem = oldbytesrem + oldtbytesrem;

    if( totoldbytesrem != 0 ){   //handle_previous_chansamps      
      if( c->oldnumbytes_received == 0 ){ //we're sitting on a gold mine; should be numsamps right here
	if( DEBUG ){
	  printf("tcp_utils.c [get_chan_samples()] CH %i Oldnumsamps according to oldparse:\t%i\n", 
		 c->num, c->oldnumsamps);
	  printf("tcp_utils.c [get_chan_samples()] CH %i Oldnumsamps according to bufpos:\t%i\n", c->num, *((int32_t *)(buf_addr+p->bufpos)));
	}
	p->bufpos += 4; //increment to skip numsamp info in buffer

      }
      
      //this check shouldn't even be necessary. I'm using it to check my math.
      if( ( p->hpos - p->bufpos ) >=  totoldbytesrem ){ //we can get em all
	memcpy( (void *)(long int)c->oldpackaddr + (long int)c->oldnumbytes_received, 
		(void *)((long int)buf_addr + p->bufpos ), oldbytesrem ); 
	
	//	p->bufpos += oldsampsrem * c->dsize;
	p->bufpos += oldbytesrem;
	
	if( c->is_asynchr ){
	  memcpy( (void *)(long int)c->oldtstamps_addr + (long int)c->oldtbytes_received , 
		  (void *)((long int)buf_addr + p->bufpos ), oldtbytesrem ); 
	  
	  p->bufpos += oldtbytesrem;
	  c->oldtbytes_received = c->oldnumtbytes;	  
	}
	
	c->oldnumbytes_received = c->oldnumsampbytes;
	c->oldnum_received = c->oldnumsamps;
	c->oldpack_ready = true;
    
      }
      else { 
	printf("tcp_utils.c [get_chan_samples()] A logical contradiction. How can you have read a new header"
	       " and yet still not have all the previous packet's samples?\n");
      } 

      if( DEBUG ) {
	printf("tcp_utils.c [get_chan_samples()] CH%i Got all %"PRIi32" old samp bytes\n",c->num, oldbytesrem); 
	if( c->is_asynchr ){
	  printf("tcp_utils.c [get_chan_samples()] CH%i Got all %"PRIi32" old tstamp bytes\n",c->num, oldtbytesrem); 
	}
      }
    }
    moresamps = true;
  }
  else { //current samples
    
    //    int32_t sampsrem = c->oldnumsamps - c->oldnum_received;
    int32_t bytesrem = c->numsampbytes - c->numbytes_received;
    int32_t tbytesrem = c->numtbytes - c->tbytes_received; 
    int32_t totbytesrem = bytesrem + tbytesrem;
    
    if( c->numbytes_received == 0 ){ //at the beginning of new set of samples for this channel
      if( DEBUG ){
	printf("tcp_utils.c [get_chan_samples()] New samples for CH %i!\n", c->num );
	printf("tcp_utils.c [get_chan_samples()] Numsamps in CH %i according to newparse:\t%i\n", c->num, c->numsamps);
	printf("tcp_utils.c [get_chan_samples()] Numsamps in CH %i according to bufpos:\t\t%i\n", c->num, *((int32_t *)(buf_addr+p->bufpos)));
      }
      p->bufpos += 4; //skip ahead of channel header
    }
    
    //if buff contains all samples for this channel
    if( ( p->bufrem - p->bufpos ) >= totbytesrem ) { 
      if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i: This buff contains all samples\n", c->num );
      
      memcpy( c->packaddr + c->numbytes_received, 
	      (void *)( (long int)buf_addr + (long int)p->bufpos ), bytesrem );
      
      p->bufpos += bytesrem;
      c->numbytes_received = c->numsampbytes;
      c->num_received = c->numsamps;
      
      if( c->is_asynchr ){
	memcpy( (void *)((long int)c->tstamps_addr + (long int)c->tbytes_received ), 
		(void *)((long int)buf_addr + p->bufpos ), tbytesrem); 
	
	p->bufpos += tbytesrem;
	c->tbytes_received = c->numtbytes;
	
      }      
      
      c->pack_ready = true;
      moresamps = true;
    } 
    else {
      
      int32_t bufbytes = p->bufrem - p->bufpos;
      int32_t to_chanbuff;
      if( bufbytes >= bytesrem ) to_chanbuff = bytesrem;
      else to_chanbuff = bufbytes;
      
      if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i: This buff DOESN'T contain all channel bytes\n", c->num );
      if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i: Reportedly receiving %f samples\n", c->num,
			 (float)( to_chanbuff ) / (float) c->dsize ); //should be an int
      memcpy( c->packaddr, (void *)((long int)buf_addr + (long int)p->bufpos ), to_chanbuff );

      c->numbytes_received += to_chanbuff;
      c->num_received += to_chanbuff / c->dsize;

      p->bufpos += to_chanbuff;
      
      if ( p->bufpos  < p->bufrem ){ //still got some juice left
	
	if( DEBUG ) {
	  printf("tcp_utils.c [get_chan_samples()] CH%i: p->bufpos =\t\t\t\t%li\n", c->num, p->bufpos);
	  printf("tcp_utils.c [get_chan_samples()] CH%i: p->bufrem =\t\t\t\t%li\n", c->num, p->bufrem);
	}
	
	bufbytes = p->bufrem - p->bufpos;

	if( c->is_asynchr ){
	  
	  if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i: Reportedly receiving %f timestamps\n", c->num,
			     (float)( bufbytes ) / (float)8.0 ); //should be an int

	  memcpy( (void *)((long int)c->tstamps_addr + (long int)c->tbytes_received), 
		  (void *)((long int)buf_addr + p->bufpos ), bufbytes ); 

	  c->tbytes_received += bufbytes;
	  
	  if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i: %"PRIi32" tbytes to go...\n", c->num,tbytesrem-bufbytes);
	}      
	else { 
	  if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i: Strange--you should never make it here\n", c->num);	  
	}
      }
      else if( p->bufpos > p->bufrem ){
	if( DEBUG ) {
	  //	  printf("tcp_utils.c [get_chan_samples()] CH%i: ..but bufpos said it didn't have any more...\n", c->num);
	  printf("tcp_utils.c [get_chan_samples()] CH%i: ...Bufpos GT bufrem!?!?\n", c->num);
	  printf("tcp_utils.c [get_chan_samples()] CH%i: p->bufpos =\t\t\t\t%li\n", c->num, p->bufpos);
	  printf("tcp_utils.c [get_chan_samples()] CH%i: p->bufrem =\t\t\t\t%li\n", c->num, p->bufrem);
	}
      }
      c->pack_ready = false;
      p->bufpos = p->bufrem; //end of buffer      
      moresamps = false;
    }
    
    if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i numsamps\t=\t\t\t\t%i\n", c->num, c->numsamps);
    if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] CH%i numreceived\t=\t\t\t%i\n", c->num, c->num_received);
    if( DEBUG ) printf("tcp_utils.c [get_chan_samples()] Parser->bufpos\t\t=\t\t\t%li\n", p->bufpos);
    if( DEBUG  )printf("tcp_utils.c [get_chan_samples()] CH%i--moresamps\t=\t\t\t\t%i\n", c->num, moresamps);
    
  } //end new samps  

  return moresamps;
}

int write_chan_samples(struct dewe_chan *c, int old, struct tcp_parser *p, bool tstamps_separate ){

  unsigned int *c_buffstart;
  int64_t *c_tstamp_buffstart;
  long int numsamps;
  size_t dsize;
  
  FILE *outfile = c->outfile;
  FILE * ts_file;
  if( c->is_asynchr && tstamps_separate ) {
    ts_file = c->ts_file;
  }
  else {
    ts_file = c->outfile;
  }

  if( DEBUG ) {
    printf("tcp_utils.c [write_chan_samples()] Channel:\t\t\t%i\n", c->num );
    printf("tcp_utils.c [write_chan_samples()] Old data:\t\t\t%i\n", old);
    printf("tcp_utils.c [write_chan_samples()] separate tstamp file:\t%i\n", tstamps_separate);
  }

  if( old == 0 ){
    
    if( c->numsamps == 0 ) {
      if( DEBUG ) printf("tcp_utils.c [write_chan_samples()] Doing new data with zero samps...poor logic\n");
      return EXIT_FAILURE;
    }
    numsamps = c->numsamps;
    
    c_buffstart = (unsigned int *)c->packaddr;
    if( c->is_asynchr ) c_tstamp_buffstart = (int64_t *)c->tstamps_addr;
  }
  else {

    if( c->oldnumsamps == 0 ){
      if( DEBUG ) printf("tcp_utils.c [write_chan_samples()] Writing old data with zero samps...poor logic\n");
      return EXIT_FAILURE;
    }
    numsamps = c->oldnumsamps;
    
    c_buffstart = (unsigned int *)c->oldpackaddr;
    if( c->is_asynchr ) c_tstamp_buffstart = (int64_t *)c->oldtstamps_addr;
  }
  
  /* if( c1->dsize != c2->dsize ){ */
  /*   printf("Unequal data sizes for each channel!\n"); */
  /*   printf("Channel %i datasize:\t%i\n", c1->num, c1->dsize); */
  /*   printf("Channel %i datasize:\t%i\n", c2->num, c2->dsize); */
  /*   return EXIT_FAILURE; */
  /* } */
  dsize = c->dsize;
  
  if( c_buffstart != NULL ){
    fwrite( c_buffstart, dsize, numsamps, outfile);
    if( c->is_asynchr ) fwrite( c_tstamp_buffstart, 8, numsamps, ts_file );
  }
  else {
    printf("tcp_utils.c [write_chan_data()] You just tried to slip me a null pointer! You code like a graduate student.\n");
    return EXIT_FAILURE;
  }

  if( DEF_VERBOSE ) printf("Wrote %li samps (%li bytes) to file\n", numsamps, numsamps * dsize );
  c->packs_tofile += 1;

  return EXIT_SUCCESS;

}

int combine_and_write_chandata( struct dewe_chan *c1 , struct dewe_chan *c2, int old, struct tcp_parser * p,  FILE *outfile){

  if( DEBUG ) {
    printf("tcp_utils.c [combine_and_write_chandata()] Combining CH%i and CH%i\n", c1->num, c2->num);
    printf("tcp_utils.c [combine_and_write_chandata()] Old data: %i\n", old);
  }
  unsigned short *c1_buffstart;
  unsigned short *c2_buffstart;
  long int numsamps;
  size_t dsize;
  
  if( old == 0 ){
    
    if( c1->numsamps == 0 || c2->numsamps == 0 ) {
      if( DEBUG ) printf("tcp_utils.c [combine_and_write_chandata()] Doing new data with zero samps...poor logic\n");
      return EXIT_FAILURE;
    }
    else if( c1->numsamps != c2->numsamps ){
      printf("Unequal number of samples for each channel!\n");
      printf("Channel %i:\t%i\n", c1->num, c1->numsamps);
      printf("Channel %i:\t%i\n", c2->num, c2->numsamps);
      return EXIT_FAILURE;
    }
    numsamps = c1->numsamps;
    
    c1_buffstart = (unsigned short *)c1->packaddr;
    c2_buffstart = (unsigned short *)c2->packaddr;
  }
  else {
    
    if( c1->oldnumsamps == 0 || c2->oldnumsamps == 0 ){
      if( DEBUG ) printf("tcp_utils.c [combine_and_write_chandata()] Doing old data with zero samps...poor logic\n");
      return EXIT_FAILURE;
    }
    else if( c1->oldnumsamps != c2->oldnumsamps ){
      printf("Unequal number of old samples for each channel!\n");
      printf("(old)Channel %i:\t%i\n", c1->num, c1->oldnumsamps);
      printf("(old)Channel %i:\t%i\n", c2->num, c2->oldnumsamps);
      return EXIT_FAILURE;
    }
    numsamps = c1->oldnumsamps;
    
    c1_buffstart = (unsigned short *)c1->oldpackaddr;
    c2_buffstart = (unsigned short *)c2->oldpackaddr;
  }
  
  if( c1->dsize != c2->dsize ){
    printf("Unequal data sizes for each channel!\n");
    printf("Channel %i datasize:\t%i\n", c1->num, c1->dsize);
    printf("Channel %i datasize:\t%i\n", c2->num, c2->dsize);
    return EXIT_FAILURE;
  }
  dsize = c1->dsize;
  
  if( ( c1_buffstart != NULL ) && ( c2_buffstart != NULL ) ){
    int16_t datum;
    for(int samp = 0; samp < numsamps; samp++){
      datum = join_upper10_lower6( *(c1_buffstart + samp ), *(c2_buffstart + samp ), 0);
      fwrite( &datum, dsize, 1, outfile);
    }
  }
  else {
    printf("tcp_utils.c [combine_and_write_chandata()] You just tried to slip me a null pointer! You code like a graduate student.\n");
    return EXIT_FAILURE;
  }

  if( DEF_VERBOSE ) printf("Combined and wrote %li samps (%li bytes) to file\n", numsamps, numsamps * dsize );
  p->npacks_combined += 1;

  return EXIT_SUCCESS;
}

//NOTE: THIS VARIATION ON THE ABOVE WRITES NEW SAMPLES TO A BUFFER INSTEAD OF A FILE
// This is done so that both rtd and an output file can use them
int combine_and_write_chandata_buff( struct dewe_chan *c1 , struct dewe_chan *c2, int old, struct tcp_parser *p, uint16_t *buff, int *pcount){

  if( DEBUG ) {
    printf("tcp_utils.c [combine_and_write_chandata()] Combining CH%i and CH%i\n", c1->num, c2->num);
    printf("tcp_utils.c [combine_and_write_chandata()] Old data: %i\n", old);
  }
  uint16_t *c1_buffstart;
  uint16_t *c2_buffstart;
  long int numsamps;
  size_t dsize;
  
  if( old == 0 ){
    
    if( c1->numsamps == 0 || c2->numsamps == 0 ) {
      if( DEBUG ) printf("tcp_utils.c [combine_and_write_chandata()] Doing new data with zero samps...poor logic\n");
      return EXIT_FAILURE;
    }
    else if( c1->numsamps != c2->numsamps ){
      printf("Unequal number of samples for each channel!\n");
      printf("Channel %i:\t%i\n", c1->num, c1->numsamps);
      printf("Channel %i:\t%i\n", c2->num, c2->numsamps);
      return EXIT_FAILURE;
    }
    numsamps = c1->numsamps;
    
    c1_buffstart = (uint16_t *)c1->packaddr;
    c2_buffstart = (uint16_t *)c2->packaddr;
  }
  else {
    
    if( c1->oldnumsamps == 0 || c2->oldnumsamps == 0 ){
      if( DEBUG ) printf("tcp_utils.c [combine_and_write_chandata()] Doing old data with zero samps...poor logic\n");
      return EXIT_FAILURE;
    }
    else if( c1->oldnumsamps != c2->oldnumsamps ){
      printf("Unequal number of old samples for each channel!\n");
      printf("(old)Channel %i:\t%i\n", c1->num, c1->oldnumsamps);
      printf("(old)Channel %i:\t%i\n", c2->num, c2->oldnumsamps);
      return EXIT_FAILURE;
    }
    numsamps = c1->oldnumsamps;
    
    c1_buffstart = (uint16_t *)c1->oldpackaddr;
    c2_buffstart = (uint16_t *)c2->oldpackaddr;
  }
  
  if( c1->dsize != c2->dsize ){
    printf("Unequal data sizes for each channel!\n");
    printf("Channel %i datasize:\t%i\n", c1->num, c1->dsize);
    printf("Channel %i datasize:\t%i\n", c2->num, c2->dsize);
    return EXIT_FAILURE;
  }
  dsize = c1->dsize;
  
  if( ( c1_buffstart != NULL ) && ( c2_buffstart != NULL ) ){
    /* int samp = numsamps; */
    /* //    buff += *pcount; //increment pointer by number of samples already recorded */
    /* while( samp-- > 0 ){  */
    /*   *buff++ = join_upper10_lower6_p( *c1_buffstart++, c2_buffstart++, 0); */
    /* } */
    int samp = 0;
    //    buff += *pcount; //increment pointer by number of samples already recorded
    while( samp++ < numsamps ){ 
      buff[samp] = join_upper10_lower6_p( c1_buffstart + samp, c2_buffstart + samp, 0);
    }

  }
  else {
    printf("tcp_utils.c [combine_and_write_chandata()] You just tried to slip me a null pointer! You code like a graduate student.\n");
    return EXIT_FAILURE;
  }

  if( DEF_VERBOSE ) printf("Combined and wrote %li samps (%li bytes) to file\n", numsamps, numsamps * dsize );
  p->npacks_combined += 1;

  *pcount += numsamps;

  return EXIT_SUCCESS;
}



//NOT FINISHED! Position reporting isn't clear yet
int print_chan_info(struct dewe_chan *c){
  
  if( DEBUG ) printf("tcp_utils.c [print_chan_info()]\n");
  
  printf("***Channel %i info***\n",c->num);
  printf("\tNew samples start address:\t\t%p\n", c->packaddr);
  printf("\tNum bytes received:\t\t\t%i\n", c->numbytes_received );
  printf("\tNum sample bytes:\t\t\t%i\n", c->numsampbytes );
  if( c->is_asynchr ){
  printf("\tNum timestamp bytes received:\t\t%i\n", c->tbytes_received );    
  printf("\tNum timestamp bytes:\t\t\t%i\n", c->numtbytes );    
  }
  printf("\tNum samps received:\t\t\t%i\n", c->num_received );
  printf("\tNum samples:\t\t\t\t%i\n", c->numsamps );
  printf("\n");
  printf("\tOld samples start address:\t\t%p\n", c->oldpackaddr);
  printf("\tOldnum bytes received:\t\t\t%i\n", c->oldnumbytes_received );
  printf("\tOldnum sample bytes:\t\t\t%i\n", c->oldnumsampbytes );
  if( c->is_asynchr ){
  printf("\tOldnum timestamp bytes received:\t%i\n", c->oldtbytes_received );    
  printf("\tOldnum timestamp bytes:\t\t\t%i\n", c->oldnumtbytes );    
  }
  printf("\tOldnum samps received:\t\t\t%i\n", c->oldnum_received );
  printf("\tOldnum samples:\t\t\t\t%i\n", c->oldnumsamps );

  return EXIT_SUCCESS;

}

//This function assumes that sizeof(short) >= 2 bytes
int16_t join_chan_bits(char a, char b)
{

  return ( a << 8 ) + b;

}


//This one is for combining the 10 upper bits we're getting from one DEWESoft channel
//and the 6 lower bits we're gettin from the other. Presumably they both come through as 
//16-bit data, and if they're in network order we need to take stock of that
//If so, use ntohs(var) ("network to host short"), assuming sizeof(short) >= 2. 

uint16_t join_upper10_lower6(uint16_t upper, uint16_t lower, bool from_network){
  if(from_network){
    //just in case lower has some garbage bits set above the lowest 6
    //lower &= ~(0b0000001111111111);
    return ( ntohs(upper) << 6) ^ ntohs(lower);
  } 
  else {
    //just in case lower has some garbage bits set above the lowest 6
    //    lower &= ~(0b1111111111 << 6);
    /* if( DEBUG ) { */
      /* printf("ushort upper = 0X%hX\n", upper); */
      /* printf("ushort lower = 0X%hX\n", lower); */
    /*   printf("ushort upper << 6 = 0X%hX\n",(upper << 6)); */
    /*   printf("lower &= ~(0b1111111111 << 6): 0X%hX\n", ( lower &= ~( 0b1111111111 << 6 ) ) );  */
    /*   printf("return of joinupper10_lower6(upper, lower, 0): 0X%hX\n", (upper << 10 ) ^ lower ); */
    /* } */
      return ( upper << 6 ) + ( lower >> 4 );
  }
}

uint16_t join_upper10_lower6_p(uint16_t *upper, uint16_t *lower, bool from_network){
  if(from_network){
    //just in case lower has some garbage bits set above the lowest 6
    //lower &= ~(0b0000001111111111);
    return ( ntohs(*upper) << 6) ^ ntohs(*lower);
  } 
  else {
    //just in case lower has some garbage bits set above the lowest 6
    //    lower &= ~(0b1111111111 << 6);
    /* if( DEBUG ) { */
      /* printf("ushort upper = 0X%hX\n", upper); */
      /* printf("ushort lower = 0X%hX\n", lower); */
    /*   printf("ushort upper << 6 = 0X%hX\n",(upper << 6)); */
    /*   printf("lower &= ~(0b1111111111 << 6): 0X%hX\n", ( lower &= ~( 0b1111111111 << 6 ) ) );  */
    /*   printf("return of joinupper10_lower6(upper, lower, 0): 0X%hX\n", (upper << 10 ) ^ lower ); */
    /* } */
      return ( *upper << 6 ) + ( *lower >> 4 );
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

int clean_chan_buffer(struct dewe_chan *c, char current){

  if( DEBUG ) printf("tcp_utils.c [clean_chan_buffer()] CH%i\n", c->num);

  //well, if the current packet is ready the old one must be too or must have already been handled, so it doesn't
  //hurt to reset oldpack stuff along the way
  //  if( c->oldpack_ready || c->pack_ready ){ 

  if( DEF_VERBOSE ) printf("Clearing old packet data for CH%i...", c->num);

  //now, if the new packet got handled as well
  //  if( c->pack_ready ){ //double bonus      
  if( current ){ //double bonus      
    
    if( DEF_VERBOSE ) printf("AND new packet data!\n");	  

    c->numsampbytes = 0;
    c->numbytes_received = 0;
    c->numsamps = 0;
    c->num_received = 0; 
    c->pack_ready = false;

    if(c->is_asynchr) {
      c->numtbytes = 0;
      c->tbytes_received = 0;
    }
  }
  else{ 
    //      memmove( c->d.type3, c->packaddr, c->num_received *  c->dsize ); //move new packet data to front of channel buff 
    memmove( c->d.type3, c->packaddr, c->numbytes_received ); //move new packet data to front of channel buff 
    //      if( c->is_asynchr ) memmove( c->timestamps, c->tstamps_addr, c->num_received * 8 ); //also timestamps, if applicable
    if( c->is_asynchr ) memmove( c->timestamps, c->tstamps_addr, c->tbytes_received ); //also timestamps, if applicable
    if( DEF_VERBOSE ) printf("but current packet isn't ready!\n");
    if( DEF_VERBOSE ) printf("\n");  
  }

  c->oldpackaddr = c->packaddr = c->d.type3; //beginning of buffer for everyone
    
  c->oldnumsampbytes = 0;
  c->oldnumbytes_received = 0;
  c->oldnumsamps = 0;
  c->oldnum_received = 0; 
  c->oldpack_ready = false;


  if( c->is_asynchr ){
    c->oldtstamps_addr = c->tstamps_addr = c->timestamps;

    c->oldnumtbytes = 0;
    c->oldtbytes_received = 0;
  }

  return EXIT_SUCCESS;
}

int update_end_of_loop(struct tcp_parser *p, char *buf_addr, struct tcp_header *th){

  if( DEBUG ) printf("tcp_utils.c [update_end_of_loop()]\n");
	
  if( p->parse_ok ){

    if(p->verbose){ printf("tcp_utils.c [update_end_of_loop()] p->parse_ok. Updating oldheader.\n"); }
    //new header parsed OK, so its address is qualified to serve as the oldheader address
    p->oldheader_addr = p->header_addr;
    p->oldhpos = p->hpos;
    
    
    //If the current packet extends beyond the current buffer of data, 
    //let user know and appropriately set packetpos and bufpos for next readthrough
    if( (p->bufrem - p->bufpos ) < ( th->pack_sz - p->packetpos + p->tailsz ) ){

      p->packetpos += p->bufrem - p->bufpos;
      p->bufpos = p->bufrem;

      if(p->verbose){ printf("tcp_utils.c [update_end_of_loop()] WARN: Buffer runs out before packet\n"); }
      if(p->verbose){ printf("tcp_utils.c [update_end_of_loop()] WARN: Current packet pos:\t%li\n", p->packetpos); }
      
    }
    else { //The buffer does contain the beginning of the next packet

      p->packetpos = th->pack_sz;
      p->bufpos = p->hpos + th->pack_sz;

      if( p->strip_packet ){
	p->t_in_this_buff = true;
      }
      if(p->verbose){ printf("tcp_utils.c [update_end_of_loop()] Found header, and this buffer contains its corresponding footer.\n"); }
      if(p->verbose){ printf("tcp_utils.c [update_end_of_loop()] Current packet pos:\t%li\n", p->packetpos); }
      if(p->verbose){ printf("tcp_utils.c [update_end_of_loop()] Current headcount:\t%i\n", p->hc); }
      if(p->verbose){ printf("tcp_utils.c [update_end_of_loop()] Current tailcount:\t%i\n", p->tc); }
      
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
    //    if(p->verbose){ printf("--->oldhpos is currently %li\n",p->oldhpos); }
    //Need to adjust oldhpos since its location was relative to an old buffer of data
    //Specifically, it needs to be a negative number to indicate it was found some number of bytes
    //BEFORE the current buffer of data
    //	p->
    //    p->oldhpos -= p->bufrem;
    //    if(p->verbose){ printf("--->oldhpos is NOW %li\n",p->oldhpos); }
    
    //if(p->verbose){ 	printf("bufrem is now %li\n",p->bufrem); }
    //if(p->verbose){ 	printf("bufpos is now %li\n",p->bufpos); }
    
    //skip these to end of buff, since we couldn't find a header and there's no way it's here
    p->bufpos = p->bufrem; //at end of buffer
    
    
  }

  return EXIT_SUCCESS;

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
    printf("Total bytes stripped: %li\n", p->deltotal);
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
  if (p->strip_packet == 2){
    if(p->stripfile != NULL)free(p->strip_fname);
    if(p->stripfile != NULL) free(p->stripfile);
  }

  free(p); //This doesn't work quite right because some pointers never get assigned anything but NULL
}
 
