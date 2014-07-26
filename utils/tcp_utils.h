#ifndef TCP_UTILS_H_
#define TCP_UTILS_H_

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#define STARTSTR_SZ 8

struct tcp_parser {

  int hc; //tcp header count
  int tc; //tcp footer count
  int startstr_sz;
  int hdrsz; //size of header
  int tailsz; //size of footer  

  //variables for looping
  void *oldheader_addr; 
  void *header_addr; //Use tail_addr and header_addr if header & footer are separate for some reason
  void *tail_addr; 

  //variables for bookkeeping
  long int oldhpos;
  long int hpos; //position of header relative to current buffer startpoint
  long int tpos; //position of tail relative to current buffer startpoint
  //  long int thdiff;

  long int packetpos; //position within current packet EXCLUDING HEADER AND FOOTER, where "0" is t
  long int bufpos; //position of reading in current buffer of data, where "0" is the beginning of the buffer
  long int keep;
  long int total;

  bool isfile;
  long int filesize;

  //First, check the most obvious case: tail immediately followed by a header
  //If that isn't the case, see if it's a classic header-followed-by-footerheader
  //If it isn't THAT, then something is funky and it's a fringe case.
  //

  //Fringe cases
  //--->If the tail isn't immediately followed by a header, see what is in between
  //------>Are we finding a tail followed by a tail? Junk the whole thing, if so--Bad packet.
  //------>
  //
  //--->Is it a header followed by a header? More garbage--Toss that packet. Nothing to salvage
  //
  //--->As an aside, look into the tcp protocol. How guaranteed are we to get the header and footer?
  //--->What we really ought to be doing is just looking for headers, reading the samples that follow,
  //--->then tossing everything until we find another sweet, sweet header. That's the real thing.
  //=============================================================================================
  //                                                                       ======================
  //Then you can find a program that just pulls out all of the good packets,
  //and sends the bad packets, bones and all, to anathema maranatha in some error file, if desired.
  //DO THAT, MY BOY

};


//This assumes only one synchronous channel!
struct tcp_header {
  unsigned char start_str[STARTSTR_SZ];
  int32_t pack_sz; //in bytes
  int32_t pack_type;
  int32_t pack_numsamps; //number of synchronous samples per channel 
                          //(there should only be one channel)
  int64_t pack_totalsamps; //number of samples acquired so far
  double pack_time; // as given above

  int32_t sync_numsamps;
};

struct chan_data {
  bool chan_is_synchr;
  bool chan_is_singleval;
  
  int32_t numsamps;
  int64_t timestamps;
};

bool parse_tcp_header(struct tcp_parser *, char *, size_t, struct tcp_header *);
int print_tcp_header(struct tcp_header*);
int print_raw_tcp_header(struct tcp_header*);
int strip_tcp_packet(struct tcp_parser *, char *, size_t, struct tcp_header *);
int print_header_sizeinfo(struct tcp_header *);


#endif /* TCP_UTILS_H_ */
