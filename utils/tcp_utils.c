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

bool parse_tcp_header(struct tcp_parser *p, char *buf_addr, 
		       size_t bufrem, struct tcp_header *header) {

  char hdstr[STARTSTR_SZ] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };  
  char tlstr[STARTSTR_SZ] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };

  //  long int bufrem = buflen - p->delbytes;

  //npredicted position of the current header
  //  long int hprediction = p->oldhpos + p->hdrsz + header->pack_sz + p->tailsz;

  //Look for header beginning at the current buffer position
  p->header_addr = memmem(buf_addr + p->bufpos, p->bufrem - p->bufpos, hdstr, sizeof(hdstr) );

  if(p->header_addr != NULL) {

    p->hc += 1;
    p->hpos = (long int)p->header_addr - (long int)buf_addr;

    //get new header
    memcpy(header, p->header_addr, p->hdrsz);


    //buffer position in front of header, if that doesn't overrun current buffer
    if( ( p->hpos + p->hdrsz ) < p->bufrem ){
      
      p->bufpos = p->hpos + p->hdrsz;
      printf("tcp_utils::parse_tcp_header: Bufpos is %li\n",p->bufpos);
      p->packetpos = 0;     //packetpos is defined as zero where the packet header ends
      p->t_in_this_buff = true;
    }
    else { //We're overshooting the buffer, so deal with it. Should be rare.
      printf("New header will be outside current buffer. Dealing with it...\n");
      p->bufpos = p->bufrem;
      p->packetpos = p->bufrem - p->bufpos;
      if( p->strip_packet ){
	p->t_in_this_buff = false; //Also expecting the tail to be in next buff
      }
    }
    //If the current packet extends beyond the current buffer of data, 
    //let user know and appropriately set packetpos for next readthrough
    if( p->bufrem - p->bufpos < header->pack_sz ){
      p->packetpos = p->bufrem - p->bufpos;
      printf("tcp_utils::parse_tcp_header: WARN: Buffer runs out before packet\n");
      printf("tcp_utils::parse_tcp_hedaer: WARN: Current packet pos:\t%li\n", p->packetpos);
      
      if( p->strip_packet ){
	p->t_in_this_buff = false; //Also expecting the tail to be in next buff
      }
    }
  } //if header_addr is NOT null
  else { //if header_addr IS null
    p->bufpos = p->bufrem;
    return false;
  }

  return true;
}

int print_tcp_header(struct tcp_header *header){

  printf("TCP header start string =\t\t");
  for (int i = 0; i < STARTSTR_SZ; i ++){
    printf("%x",header->start_str[i]);
  }
  printf("\n");
  printf("Packet size:\t\t%"PRIi32"\n", header->pack_sz);
  printf("Packet type:\t\t%"PRIi32"\n", header->pack_type);
  printf("Packet number of samples:\t%"PRIi32"\n", header->pack_numsamps);
  //  printf("Total samples sent so far:\t%"PRIi64"\n", header->pack_totalsamps);
  printf("Total samples sent so far:\t%lli\n", header->pack_totalsamps);
  printf("Packet time:\t\t%4.4LE\n", header->pack_time);
  //  printf("Packet time in hex:\t%4.4LA\n", header->pack_time);
  printf("Sync channel num samples:\t%"PRIi32"\n", header->sync_numsamps);
  return EXIT_SUCCESS;
}


int print_raw_tcp_header(struct tcp_header *header){

  int i = 0;
  int j = 0;
  int rowmod = 2*STARTSTR_SZ;

  size_t hdrsz = sizeof(struct tcp_header);

  printf("\n*************\n");
  printf("Raw TCP header (%i bytes):\n",hdrsz);

  //print the whole header in a nice format
  for( i = 0; i < ( hdrsz / rowmod); i++){

    j = rowmod * i;
    //    printf("At address %p\n",header + j);
    
    do {
      printf("%i\t",j);
      j++;      
      if (j == hdrsz){break;}

    } while( ( j % rowmod ) != 0 );

    j = rowmod * i;

    printf("\n");

    do {
      printf("%#hX\t",header->start_str[j] );
      j++;
      if (j == hdrsz)
	break;
    } while( ( j % rowmod ) != 0 );
    printf("\n");

  }
  printf("*************\n\n");

  return EXIT_SUCCESS;
}


int print_header_memberszinfo(struct tcp_header *header){

  printf("\n");
  printf("sizeof start_str\t%i\n", sizeof(header->start_str));
  printf("sizeof pack_sz:\t\t%i\n", sizeof(header->pack_sz));
  printf("sizeof pack_type:\t%i\n", sizeof(header->pack_type));
  printf("sizeof pack_numsamps:\t%i\n", sizeof(header->pack_numsamps));
  //  printf("Total samples sent so far:\t%"PRIi64"\n", sizeof(header->pack_totalsamps));
  printf("sizeof totalsamps:\t%i\n", sizeof(header->pack_totalsamps));
  printf("sizeof pack_time:\t%i\n", sizeof(header->pack_time));
  printf("sizeof sync_numsamps:\t%i\n", sizeof(header->sync_numsamps));
  return EXIT_SUCCESS;
 
}


int strip_tcp_packet(struct tcp_parser *p, char *buf_addr, 
		       size_t bufrem, struct tcp_header *header) {

  char *tmp_tail_addr;
  long int tmp_hdr_addr = (long int)p->header_addr;
  int t_lastbytes = 0;

  //if there is a tail nearby from the last header search, account for it here
  if( p->t_in_last_buff ) {
    tmp_tail_addr=(char *)((long int)(p->header_addr) - p->tailsz);
    //    printf("tmp tail addr: %p\n",tmp_tail_addr);
    //    printf("header_addr: %p\n",p->header_addr);
    if( strncmp( (char *)(tmp_tail_addr), p->tlstr, p->tailsz) == 0 ){
      printf("tcp_utils::strip_tcp_packet: found tailstring\n");
      for(int i = 0; i < p->tailsz; i++){
	printf("0x%X ",tmp_tail_addr[i]);
      }
      printf("\n");
      
      t_lastbytes = p->tailsz;
      p->tkill += p->t_in_last_buff;
      
      p->t_in_last_buff = false;
    }
    else {
      printf("tcp_utils::strip_tcp_packet: Heard tell there was a tail here, but no sign...\n");
      p->t_in_last_buff = true;
    }
  }
  
  //We'll assume that for the main, packets headers and footers are where they say they are
  //If not, we'll just junk the packet or output it to a badpacket file
  
  //Start the show by killing the obvious header (as well as the footer from the last buffer, if applicable)
  printf("tcp_utils::strip_tcp_packet: bufrem is %li\n", p->bufrem);
  memmove((void *)((long int)p->header_addr - t_lastbytes),
	  //	  p->header_addr + p->hdrsz, buflen - p->delbytes);
  	  (void *)((long int)p->header_addr + p->hdrsz), p->bufrem);

  //Not keeping bytes corresponding to the header anymore
  p->delbytes += p->hdrsz + t_lastbytes;
  p->bufrem -= (p->hdrsz + t_lastbytes);
  p->hkill += 1;

    //Better take care of all addresses, or you'll live to regret it
  //  if(t_lastbytes){
  //  p->header_addr -= t_lastbytes; //stays in same spot if we didn't also kill a tail
  printf("BEFORE header_addr: %p\n",p->header_addr);
  p->header_addr -= t_lastbytes/sizeof(*p->header_addr); //header_addr is a long int *, so this skips back 8 bytes
  printf("AFTER header_addr: %p\n",p->header_addr);
  //Also all positions
  p->hpos -= t_lastbytes;
  p->bufpos -= (p->hdrsz + t_lastbytes); //Move buffer position back, since we killed the header
    //  }

  if( p->t_in_this_buff ){ //Time to get the tail
   
    //We'll just find it ourselves, since this tool needs to be independent of the program running it
    //    printf("p->header_addr
    p->tail_addr = memmem( (void *)((long int)p->header_addr + header->pack_sz), p->hdrsz, p->tlstr, p->startstr_sz);
    if( p->tail_addr != NULL ){
      //First, get tail pos relative to current buffer
      p->tpos = (long int)p->tail_addr - (long int)buf_addr;
      
      memmove( p->tail_addr, (void *)((long int)p->tail_addr + p->startstr_sz), p->bufrem - p->tpos - p->tailsz);

      p->bufrem -= p->tailsz;

      //In principle, what is now the tail address should be the location of the next header
      /* if( strncmp( (char *)(p->tail_addr), header->start_str, p->startstr_sz) == 0){ */
      /* 	printf("tcp_utils::strip_tcp_packet: Just killed tail in this buff, and the next header is RIGHT HERE\n"); */
      /* } */
      p->tkill += 1;
      p->t_in_last_buff = false; //It WAS here, but not any more, the filthy animal
    }
    else { //Where on earth is the blasted thing?
      fprintf(stderr,"tcp_utils::strip_tcp_packet: Couldn't find tail!! What is the meaning of life?\n");
      p->t_in_last_buff = true; //I guess it's in the next buffer--how did this escape us?
    }

  }
  else { //See you in the next merry-go-round, tail
    p->t_in_last_buff = true;

  }
  
  return EXIT_SUCCESS;
}

//This function assumes that sizeof(short) >= 2 bytes
short join_chan_bits(char a, char b)
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
    //    lower &= ~(0b0000001111111111);
    return ( ntohs(upper) << 6) ^ ntohs(lower);
  } 
  else {
    //just in case lower has some garbage bits set above the lowest 6
    //    lower &= ~(0b1111111111 << 6);
    return (upper << 6) ^ lower;
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

struct tcp_parser *parser_init(void){

  //  p = malloc( sizeof(struct tcp_parser) );
  struct tcp_parser *p;
  p = malloc( sizeof(struct tcp_parser));
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

  p->do_predict = false;
  p->isfile = false;
  p->filesize = 0;

  p->strip_packet = 0;
  p->strip_fname = NULL;
  //p->stripfile = NULL;
  p->hkill = 0;
  p->tkill = 0;
  p->t_in_this_buff = false;
  p->t_in_last_buff = false;

  p->oldheader_addr = NULL;
  p->header_addr = NULL;
  p->tail_addr = NULL;

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
