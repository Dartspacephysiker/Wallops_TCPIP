/*
 * tcp_utils.c
 *
 *  July 2014
 *  Author: Hammertime
nnn */

#define _GNU_SOURCE

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "tcp_utils.h"

bool parse_tcp_header(struct tcp_parser *p, char *buf_addr, 
		       size_t buflen, struct tcp_header *header) {

  char hdstr[STARTSTR_SZ] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };  
  char tlstr[STARTSTR_SZ] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };

  //npredicted position of the current header
  //  long int hprediction = p->oldhpos + p->hdrsz + header->pack_sz + p->tailsz;

  //Look for header beginning at the current buffer position
  p->header_addr = memmem(buf_addr + p->bufpos, buflen - p->bufpos, hdstr, sizeof(hdstr) );

  if(p->header_addr != NULL) {

    p->hc += 1;
    p->hpos = (long int)p->header_addr - (long int)buf_addr;

    //get new header
    memcpy(header, p->header_addr, p->hdrsz);


    //buffer position in front of header, if that doesn't overrun current buffer
    if( ( p->hpos + p->hdrsz ) < buflen ){
      
      p->bufpos = p->hpos + p->hdrsz;
      printf("Bufpos is %li\n",p->bufpos);
      p->packetpos = 0;     //packetpos is defined as zero where the packet header ends
    }
    else { //We're overshooting the buffer, so deal with it. Should be rare.
      printf("New header will be outside current buffer. Dealing with it...\n");
      p->bufpos = buflen;
      p->packetpos = buflen - p->bufpos;
    }



    //If the current packet extends beyond the current buffer of data, 
    //let user know and appropriately set packetpos for next readthrough
    if( buflen - p->bufpos < header->pack_sz ){
      p->packetpos = buflen - p->bufpos;
      printf("WARN: Buffer runs out before packet\n");
      printf("WARN: Current packet pos:\t%li\n", p->packetpos);
    }
  }
  else {
    p->bufpos = buflen;
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


int print_header_sizeinfo(struct tcp_header *header){

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

  //Here we are. The main issue is how to scoot around the memory

  //What if, say, we get 
  

  return EXIT_SUCCESS;
}
