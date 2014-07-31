#ifndef TCP_UTILS_H_
#define TCP_UTILS_H_

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define STARTSTR_SZ 8

//This assumes only one synchronous channel!
struct tcp_header {
  char start_str[STARTSTR_SZ];
  int pack_sz; //in bytes
  int pack_type;
  int pack_numsamps; //number of synchronous samples per channel   //(there should only be one channel)
  long int pack_totalsamps; //number of samples acquired so far
  double pack_time; // as given above
  int sync_numsamps;
};

struct chan_data {
  bool chan_is_synchr;
  bool chan_is_singleval;
  
  int32_t numsamps;
  int64_t timestamps;
};

struct tcp_parser {

  int numpackets;
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

  long int packetpos; //position within current packet EXCLUDING HEADER AND FOOTER, where "0" is t
  long int bufpos; //position of reading in current buffer of data, where "0" is the beginning of the buffer
  long int bufrem;
  long int delbytes;
  long int deltotal;
  long int total;

  bool parse_ok;

  bool do_predict;
  long int hprediction;
  unsigned int num_badp;

  bool isfile;
  long int filesize;
  long int wcount;

  char strip_packet;
  char *strip_fname;
  FILE *stripfile;
  //  long int wcount;
  bool hkill;
  bool oldtkill;
  bool tkill;
  unsigned int numhkill; //num headers killed
  unsigned int numtkill; //num footers killed
  bool t_in_this_buff;
  bool oldt_in_this_buff;

  //variables for looping
  long int *oldheader_addr; 
  long int *header_addr; //Use tail_addr and header_addr if header & footer are separate for some reason
  long int *tail_addr; 

  bool verbose;
};


struct tcp_header *tcp_header_init(void);
struct tcp_parser * parser_init(void);

bool parse_tcp_header(struct tcp_parser *, char *, struct tcp_header *);
int update_after_parse_header(struct tcp_parser *p, char * buf_addr, struct tcp_header *header);

int print_tcp_header(struct tcp_header *);
int print_raw_tcp_header(struct tcp_header *);
int print_header_memberszinfo(struct tcp_header *);

int prep_for_strip(struct tcp_parser *, char *, struct tcp_header *);
int strip_tcp_packet(struct tcp_parser *, char *, struct tcp_header *);
int post_strip(struct tcp_parser *, char *, struct tcp_header *);

int update_parser_addr_and_pos(struct tcp_parser *, char *, struct tcp_header *);

void print_stats(struct tcp_parser *);
void free_parser(struct tcp_parser *);

short join_chan_bits(char, char);
uint16_t join_upper10_lower6(uint16_t, uint16_t, bool);

#endif /* TCP_UTILS_H_ */
