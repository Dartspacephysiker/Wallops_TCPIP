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

  //init struct
  struct dewe_chan *c;
  c = malloc( sizeof(struct dewe_chan));
  
  c->chan_num = chan_num;

  //init data
  if(is_singleval){
    c->is_singleval = true;
    c->dsize = chan_data_size[7]; //double float, 64-bit, as required by DEWESoft
    c->d.type7 = malloc( sizeof(double_t) );
  }
  else {
    c->dtype = dtype;
    c->dsize = chan_data_size[dtype];

    //handle various channel types
    switch ( dtype ) {
      case 0:
	c->d.type0 = malloc( chan_data_size[dtype] * MAXNUMSAMPS );
	break;
      case 1:
	c->d.type1 = malloc( chan_data_size[dtype] * MAXNUMSAMPS );
	break;
      case 2:
	c->d.type2 = malloc( chan_data_size[dtype] * MAXNUMSAMPS );
	break;
      case 3:
	c->d.type3 = malloc( chan_data_size[dtype] * MAXNUMSAMPS );
	break;
      case 4:
	c->d.type4 = malloc( chan_data_size[dtype] * MAXNUMSAMPS );
	break;
      case 5:
	c->d.type5 = malloc( chan_data_size[dtype] * MAXNUMSAMPS );
	break;
      case 6:
	c->d.type6 = malloc( chan_data_size[dtype] * MAXNUMSAMPS );
	break;
      case 7:
	c->d.type7 = malloc( chan_data_size[dtype] * MAXNUMSAMPS );
	break;	
    }
    printf("Channel data type %u: %u bytes per sample\n", dtype, chan_data_size[dtype]);
    printf("Malloc'ed %i bytes for channel %u...\n", chan_data_size[dtype] * MAXNUMSAMPS, chan_num );

    c->samppos = 0;
    c->numsamps = 0;
    c->num_waiting_to_receive = 0;
    //    chan->samps = malloc( MAXNUMSAMPS * chan->dsize );

    if(is_asynchr){
      c->is_asynchr = true;
      c->timestamps = malloc( MAXNUMSAMPS * 8); //Accommodate 64-bit timestamps
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

bool parse_tcp_header(struct tcp_parser *p, char *buf_addr, struct tcp_header *th) {

  //Look for header beginning at the current buffer position
  p->header_addr = memmem(buf_addr + p->bufpos, p->bufrem - p->bufpos, p->startstr, p->startstr_sz );

  if(p->header_addr != NULL) {

    //get new header
    //    memcpy(header, p->header_addr, p->hdrsz); //this doesn't work because of mem alignment
    
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



int print_tcp_header(struct tcp_header *th){

  printf("TCP header start string =\t\t");
  for (int i = 0; i < STARTSTR_SZ; i ++){
    printf("%x",th->start_str[i]);
  }
  printf("\n");
  printf("Packet size:\t\t%"PRIi32"\n", th->pack_sz);
  printf("Packet type:\t\t%"PRIi32"\n", th->pack_type);
  printf("Packet number of samples:\t%"PRIi32"\n", th->pack_numsamps);
  //  printf("Total samples sent so far:\t%"PRIi64"\n", th->pack_totalsamps);
  printf("Total samples sent so far:\t%.06lli\n", th->pack_totalsamps);
  printf("Packet time:\t\t%f\n", th->pack_time);
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

  if(p->parse_ok) { //the bugle horn of a new packet!

    //update position
    p->hpos = (long int)p->header_addr - (long int)buf_addr;
    p->bufpos = p->hpos;
    p->packetpos = - p->startstr_sz;

  }

  return EXIT_SUCCESS;

}

int prep_for_strip(struct tcp_parser *p, char * buf_addr, struct tcp_header *th){

 if(p->parse_ok) {

   //if space between end of buffer and new header is less than the entire new TCP packet
    if( (p->bufrem - p->hpos ) < ( th->pack_sz + p->tailsz + p->startstr_sz ) ){

      p->t_in_this_buff = false; //expecting the tail to be in next buff

      if(p->verbose){ printf("tcp_utils::prep_for_strip: WARN: Buffer runs out before packet\n"); }
      if(p->verbose){ printf("tcp_utils::prep_for_strip: WARN: Current packet pos:\t%li\n", p->packetpos); }
      
    }
    else { //The buffer does contain a tail

      p->t_in_this_buff = true;

      if(p->verbose){ printf("tcp_utils:prep_for_strip: Found header, and this buffer contains its corresponding footer.\n"); }
      if(p->verbose){ printf("tcp_utils:prep_for_strip: Current packet pos:\t%li\n", p->packetpos); }
      if(p->verbose){ printf("tcp_utils:prep_for_strip: Current headcount:\t%li\n", p->hc); }
      if(p->verbose){ printf("tcp_utils:prep_for_strip: Current tailcount:\t%li\n", p->tc); }
      
    }
  }  //if header_addr is NOT null
  else { //if header_addr IS null

    //if the total left to read in the buffer is less than the total left to read in the packet (+ tail)
    if( ( p->bufrem - p->bufpos ) < ( th->pack_sz - p->packetpos + p->tailsz ) ) {
      
      p->t_in_this_buff = false;

      printf("tcp_utils:prep_for_strip: Couldn't find header, and this buffer "
	     "does NOT contain the footer from the last header.\n");

    } 

    else { //what is left in the buffer DOES have a tail in it, if pack_sz isn't lying.
           //This is an extremely unlikely scenario, you know.

      p->t_in_this_buff = true; //The tail is in this buffer, children

      if(p->verbose) {
	printf("tcp_utils:prep_for_strip: Couldn't find header, but this buffer "
	       "contains the footer from the last header.\n"); 
	printf("tcp_utils:prep_for_strip: Current packet pos:\t%li\n", p->packetpos); 
	printf("tcp_utils:prep_for_strip: Current headcount:\t%li\n", p->hc); 
	printf("tcp_utils:prep_for_strip: Current tailcount:\t%li\n", p->tc); 
      }     

    }
  }

 return EXIT_SUCCESS;

}

int strip_tcp_packet(struct tcp_parser *p, char *buf_addr, struct tcp_header *th) {

  char *tmp_tail_addr;
  long int tmp_tail_pos;

  bool oldt_in_next_buff = false;

  
  if( p->t_in_this_buff ){ //Time to get the tail
   
    //We'll just find it ourselves, since this tool needs to be independent of the program running it
    //    tmp_tail_addr = (char *)((long int)buf_addr + p->bufpos + p-> startstr_sz + th->pack_sz + 
    //			     p->tailsz - p->packetpos );
    tmp_tail_addr = (char *)((long int)buf_addr + p->bufpos + th->pack_sz - p->packetpos );

    tmp_tail_pos = (long int)tmp_tail_addr - (long int)buf_addr;
    
    if(p->verbose){ printf("tcp_utils::strip_tcp_packet: p->t_in_this_buff = true\n"); }
    if(p->verbose){ printf("tcp_utils::strip_tcp_packet: p->delbytes = %i\n", p->delbytes); }
    

    p->tail_addr = memmem( tmp_tail_addr, p->hdrsz + p->startstr_sz + p->tailsz, p->tlstr, p->tailsz);
    if( p->tail_addr != NULL ){

      //First, get tail pos relative to current buffer      

      p->tpos = (long int)p->tail_addr - (long int)buf_addr;
      if(p->verbose){ printf("tcp_utils::strip_tcp_packet: tmp_tail_pos= %li\n", tmp_tail_pos); }
      if(p->verbose){ printf("tcp_utils::strip_tcp_packet: tail_pos= %li\n", p->tpos); }
      

      //kill it
      memmove( p->tail_addr, (void *)((long int)p->tail_addr + p->tailsz), p->bufrem - p->tpos - p->tailsz);
      p->tkill = true;

      //      p->oldt_in_this_buff = false; //It WAS here, but not any more, the filthy animal

      //In principle, what is now the tail address should be the location of the next header
      /* if( strncmp( (char *)(p->tail_addr), th->start_str, p->startstr_sz) == 0){ */
      /* 	printf("tcp_utils::strip_tcp_packet: Just killed tail in this buff, and the next header is RIGHT HERE\n"); */
      /* } */
    }
    else { //Where on earth is the blasted thing?
      if(p->verbose){ printf("tcp_utils::strip_tcp_packet: Couldn't find tail!! What is the meaning of life?\n"); }
      oldt_in_next_buff = true; //I guess it's in the next buffer--how did this escape us?
    }

  }
  else { //See you in the next merry-go-round, tail
    oldt_in_next_buff = true;

  }

  //Now blast the obvious header (as well as the footer from the last buffer, if applicable)
  if( p->parse_ok ){ //Note, if !parse_ok then header_addr is NULL

    if(p->verbose){ printf("tcp_utils::strip_tcp_packet: Blasting header...\n"); }
    //if(p->verbose){     printf("tcp_utils::strip_tcp_packet: bufrem is %li\n", p->bufrem); }

    //if there is a tail nearby from the last header search, account for it here
    if( p->oldt_in_this_buff ) {

      //double check, just to be safe
      if( strncmp( (char *)((long int)p->header_addr - p->tailsz), p->tlstr, p->tailsz) == 0 ){

	p->oldtkill = true;
      	p->oldt_in_this_buff = false;

	if(p->verbose){ printf("tcp_utils::strip_tcp_packet: found oldtailstring\n"); }
	for(int i = 0; i < p->tailsz; i++){
	  if(p->verbose){ printf("0x%X ",((char *)((long int)p->header_addr - p->tailsz))[i]); }
	}
	if(p->verbose){ printf("\n"); }
      
      }
      else {

	p->oldt_in_this_buff = true;

	if(p->verbose){ printf("tcp_utils::strip_tcp_packet: Heard tell there was a tail here, but no sign...\n"); }

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

  //  p->oldhpos -= p->tailsz * (int)p->oldtkill;
  p->hpos -= ( p->tailsz * ((int)p->oldtkill + (int)p->tkill) + p->hdrsz * p->hkill ); //p->tailsz * (int)p->oldtkill;
  p->packetpos += (int)p->hkill * (p->hdrsz) + p->tailsz * (int)p->oldtkill;
  p->bufpos -= ( p->tailsz * ((int)p->oldtkill + (int)p->tkill) + p->hdrsz * p->hkill ); 
  p->bufrem -= ( (int)p->hkill * (p->hdrsz) + p->tailsz * ((int)p->oldtkill + (int)p->tkill) );
  p->delbytes += (int)p->hkill * (p->hdrsz) + p->tailsz * ((int)p->oldtkill + (int)p->tkill);


  if( p->oldtkill ){

    if(p->verbose){ printf("tcp_utils::post_strip: Killed an old tail\n"); }
    if(p->verbose){ printf("BEFORE header_addr: %p\n",p->header_addr); }
    p->header_addr -= p->oldtkill; //header_addr is a long int *, so this skips back 8 bytes
    if(p->verbose){ printf("AFTER header_addr: %p\n",p->header_addr); }

  }

  p->numhkill += (int)p->hkill;
  p->numtkill += (int)p->oldtkill + (int)p->tkill;
    
  p->hkill = false;
  p-> oldtkill = p->tkill = false;

}

int get_chan_info( struct dewe_chan *c, int numchans ){
  /* You are right now working on getting info for the next readthrough, and deciding
whether to make a bunch of variables like "oldnum_waiting_to_receive" and "oldnumsamps" so that
when the time comes, you can grab all of the relevant samples out of the buffer, run them through
join_upper10_lower6, and write the new awesome data to file for viewing pleasure.

You might have one function do this combining and writing to file for both old and new, and make "oldsamps" or "newsamps" an argument to the function. 

Another option might be having variables oldsamp_addr, oldnumsamps, oldwaiting_to_receive, and the like--
whatever is necessary. 

-->Where to put pointer to file? 

*/
}

int get_chan_samples( struct dewe_chan *c , struct tcp_parser * p , struct tcp_header *th){

  long int chanbufspace; //number of bytes remaining to pick up

  if( p->parse_ok ){ //A fresh batch!

    //    handle_previous_chansamps

    c->samppos = 0;
    c->numsamps = th->pack_numsamps;
    chanbufspace = c->numsamps * c->dsize;

    /* if( ( p->bufrem - p->bufpos ) > ) { //if buffer contains all samples for this channel */

    /*   c->num_waiting_to_receive = zero; */
    /* } */
  } 
  else {
    chanbufspace = ( c->numsamps - c-> samppos ) * c->dsize;
  }

  //make sure buffer contains all samples for this channel

  //make sure channel buffer is bad enough to hold all samples
  //c->dsize is number of bytes for given data size
  if(  chanbufspace > (p->bufrem - p->bufpos) ){

  }
  else { //there isn't enough space to hold everything!

  }

}

int handle_chan_samples( struct dewe_chan *c , struct tcp_parser * p , struct tcp_header *th){

  

}

int update_parser_addr_and_pos(struct tcp_parser *p, char *buf_addr, struct tcp_header *th){

	
  if( p->parse_ok ){

    if(p->verbose){ printf("tcp_utils:;update_parser_addr_and_pos: p->parse_ok. Updating oldheader.\n"); }
    //new header parsed OK, so its address is qualified to serve as the oldheader address
    p->oldheader_addr = p->header_addr;
    p->oldhpos = p->hpos;
    
    
    //If the current packet extends beyond the current buffer of data, 
    //let user know and appropriately set packetpos and bufpos for next readthrough
    if( (p->bufrem - p->bufpos ) < ( th->pack_sz - p->packetpos + p->tailsz ) ){

      p->packetpos += p->bufrem - p->bufpos;
      p->bufpos = p->bufrem;

      if(p->verbose){ printf("tcp_utils::update_parser_addr_and_pos: WARN: Buffer runs out before packet\n"); }
      if(p->verbose){ printf("tcp_utils::update_parser_addr_and_pos: WARN: Current packet pos:\t%li\n", p->packetpos); }
      
    }
    else { //The buffer does contain the beginning of the next packet

      p->packetpos = th->pack_sz;
      p->bufpos = p->hpos + th->pack_sz;

      if( p->strip_packet ){
	p->t_in_this_buff = true;
      }
      if(p->verbose){ printf("tcp_utils:update_parser_addr_and_pos: Found header, and this buffer contains its corresponding footer.\n"); }
      if(p->verbose){ printf("tcp_utils:update_parser_addr_and_pos: Current packet pos:\t%li\n", p->packetpos); }
      if(p->verbose){ printf("tcp_utils:update_parser_addr_and_pos: Current headcount:\t%li\n", p->hc); }
      if(p->verbose){ printf("tcp_utils:update_parser_addr_and_pos: Current tailcount:\t%li\n", p->tc); }
      
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
  printf("\n*************************\n\n");


}

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
