#ifndef TCP_UTILS_H_
#define TCP_UTILS_H_

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#define STARTSTR_SZ 8

struct tcp_parser {

  int hc; //tcp header count
  int tc; //tcp footer count
  int hdrsz; //size of header

  char startstr[8];
  int startstr_sz;
  char tlstr[8];
  int tailsz; //size of footer  

  //variables for bookkeeping
  long int oldhpos;
  long int hpos; //position of header relative to current buffer startpoint
  long int tpos; //position of tail relative to current buffer startpoint
  //  long int thdiff;

  long int packetpos; //position within current packet EXCLUDING HEADER AND FOOTER, where "0" is t
  long int bufpos; //position of reading in current buffer of data, where "0" is the beginning of the buffer
  long int bufrem;
  long int delbytes;
  long int deltotal;
  long int total;

  bool parse_ok;
  bool do_predict;

  bool isfile;
  long int filesize;

  char strip_packet;
  char *strip_fname;
  FILE *stripfile;
  long int wcount;
  int hkill; //num headers killed
  int tkill; //num footers killed
  bool t_in_this_buff;
  bool t_in_last_buff;

  //variables for looping
  long int *oldheader_addr; 
  long int *header_addr; //Use tail_addr and header_addr if header & footer are separate for some reason
  long int *tail_addr; 

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
int print_tcp_header(struct tcp_header *);
int print_raw_tcp_header(struct tcp_header *);
int print_header_memberszinfo(struct tcp_header *);
int strip_tcp_packet(struct tcp_parser *, char *, size_t, struct tcp_header *);
short join_chan_bits(char, char);
uint16_t join_upper10_lower6(uint16_t, uint16_t, bool);

struct tcp_header *tcp_header_init(void);
struct tcp_parser * parser_init(void);
void free_parser(struct tcp_parser *);

#endif /* TCP_UTILS_H_ */
