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
  p->header_addr = memmem(buf_addr + p->bufpos, bufrem - p->bufpos, hdstr, sizeof(hdstr) );

  if(p->header_addr != NULL) {

    p->hc += 1;
    p->hpos = (long int)p->header_addr - (long int)buf_addr;

    //get new header
    memcpy(header, p->header_addr, p->hdrsz);


    //buffer position in front of header, if that doesn't overrun current buffer
    if( ( p->hpos + p->hdrsz ) < bufrem ){
      
      p->bufpos = p->hpos + p->hdrsz;
      printf("Bufpos is %li\n",p->bufpos);
      p->packetpos = 0;     //packetpos is defined as zero where the packet header ends
      p->t_in_this_buff = true;
    }
    else { //We're overshooting the buffer, so deal with it. Should be rare.
      printf("New header will be outside current buffer. Dealing with it...\n");
      p->bufpos = bufrem;
      p->packetpos = bufrem - p->bufpos;
      if( p->strip_packet ){
	p->t_in_this_buff = false; //Also expecting the tail to be in next buff
      }
    }
    //If the current packet extends beyond the current buffer of data, 
    //let user know and appropriately set packetpos for next readthrough
    if( bufrem - p->bufpos < header->pack_sz ){
      p->packetpos = bufrem - p->bufpos;
      printf("WARN: Buffer runs out before packet\n");
      printf("WARN: Current packet pos:\t%li\n", p->packetpos);
      
      if( p->strip_packet ){
	p->t_in_this_buff = false; //Also expecting the tail to be in next buff
      }
    }
  } //if header_addr is NOT null
  else { //if header_addr IS null
    p->bufpos = bufrem;
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
		       size_t buflen, struct tcp_header *header) {

  //if there is a tail nearby from the last header search, account for it here
  int t_lastbytes = p->t_in_last_buff * p->tailsz;

  //We'll assume that for the main, packets headers and footers are where they say they are
  //If not, we'll just junk the packet or output it to a badpacket file
  
  //!!!!!!NOTE FOR MONDAY
  //!!!!!! Everything should be OK for killing the packet headers, but you haven't 
  //figured out how to deal with pesky footers yet. hprediction would be a nice solution,
  //but it isn't currently appropriate to calculate it before calling this function.
  //An alternative is to just search for the tail, kill it there, and optionally check
  //if the new header is right in front of it. (NOTE, SUCH A CHECK ISN'T POSSIBLE IN A LIVE STREAM)

  //Start the show by killing the obvious header
  memmove(p->header_addr - t_lastbytes,
	  p->header_addr + p->hdrsz, buflen - p->delbytes);
  
  //Not keeping bytes corresponding to the header anymore
  p->delbytes += p->hdrsz + t_lastbytes;
  p->bufrem -= (p->hdrsz + t_lastbytes);

  //Better take care of all addresses, or you'll live to regret it
  p->header_addr -= t_lastbytes; //stays in same spot if t_lastbytes is zero


  //Also all positions
  p->hpos -= t_lastbytes;
  p->bufpos -= t_lastbytes;

  if( p->t_in_this_buff ){ //Time to get the tail
   
    

    p->t_in_last_buff = false; //It WAS here, but not any more, the filthy animal
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
