#ifndef TCP_UTILS_H_
#define TCP_UTILS_H_

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define STARTSTR_SZ 8
#define MAXNUMSAMPS 20000

//This assumes only one synchronous channel!
struct tcp_header {

  char start_str[STARTSTR_SZ];
  int32_t pack_sz; //in bytes
  int32_t pack_type;
  int32_t pack_numsamps; //number of synchronous samples per channel   //(there should only be one channel)
  int64_t pack_totalsamps; //number of samples acquired so far
  double pack_time; // as given above
  int32_t sync_numsamps;

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

  char do_chans; // if 1, just parse chan info; 2, write chandata OUT; 3, combine two chans with join_upper10_lower6
  int nchans;
  int npacks_combined; //keep track of number of packets successfully combined

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

/*According to the DEWESoft NET interface documentation, a TCP packet may have multiple channels and 
 *may be either synchronous or asynchronous. Those channels can each have their own data type, which are
 *0 - 8-bit unsigned int "uchar" or "uint8_t"
 *1 - 8-bit signed int "char" or "int8_t"
 *2 - 16-bit unsigned int "uint" or "uint16_t" 
 *3 - 16-bit signed int "int" or "int16_t"
 *4 - 32-bit signed int "long int" or "int32_t"
 *5 - Single floating point (32-bit) "float"
 *6 - 64-bit signed int "long long int" or "int64_t"
 *7 - Double floating point (64-bit) "double" or "double_t"
 */
static uint8_t chan_data_size[8] = { 8, 8, 16, 16, 32, 32, 64, 64 }; //indexes the sizes of data types above

struct dewe_chan {

  uint8_t num;

  bool is_singleval;
  
  int8_t dtype;
  int8_t dsize;

  long int bufsize;
  long int bufroom;

  //There must be a better way, but so be it...
  union dtype {
    uint8_t *type0;
    int8_t *type1;
    uint16_t *type2;
    int16_t *type3;
    uint32_t *type4;
    int32_t *type5;
    int64_t *type6;
    double_t *type7;
  } d;

  //these variables accommodate the unfortunate reality that most DEWESoft packets come through piecemeal
  // and that a given receive buffer probably contains data from an old packet as well as a new one

  void *oldpackaddr; //address of the beginning of the old packet samples
  int32_t oldnum_received; //number of samples currently held in buffer from an OLD packet
  int32_t oldnumsamps; //number of samples contained in OLD packet
  //  int32_t oldnum_not_received; //number of samples that we still haven't parsed through from the OLD packet
  bool oldpack_ready;

  void *packaddr; //address of the beginning of new packet samples
  int32_t num_received; //number of samples currently held in buffer from a newly parsed packet
  int32_t numsamps; //number of samples contained in newly parsed packet
  //  int32_t num_not_received; //number of samples that we still haven't parsed through from the NEW packet
  bool pack_ready;

  //for async channels
  bool is_asynchr;
  int64_t *timestamps; 

  char outfname[128];
  FILE *outfile;

};

//init routines
struct tcp_header *tcp_header_init(void);
struct dewe_chan *chan_init(int, int, bool, bool);
struct tcp_parser * parser_init(void);

//parse routines
bool parse_tcp_header(struct tcp_parser *, char *, struct tcp_header *);
int update_after_parse_header(struct tcp_parser *p, char * buf_addr, struct tcp_header *header);

//print routines
int print_tcp_header(struct tcp_header *);
int print_raw_tcp_header(struct tcp_header *);
int print_header_memberszinfo(struct tcp_header *);

//strip routines
int prep_for_strip(struct tcp_parser *, char *, struct tcp_header *);
int strip_tcp_packet(struct tcp_parser *, char *, struct tcp_header *);
int post_strip(struct tcp_parser *, char *, struct tcp_header *);

//chan routines
int get_chan_samples(struct dewe_chan *, struct tcp_parser *, struct tcp_header *);
int write_chan_samples(struct dewe_chan *, struct tcp_parser *, struct tcp_header *);
int update_chan_info(struct dewe_chan *, struct tcp_header *, struct tcp_parser *, char *);
int print_chan_info(struct dewe_chan *);
int combine_and_write_chandata(struct dewe_chan *, struct dewe_chan *, int, struct tcp_parser *, struct tcp_header *);
int16_t join_chan_bits(char, char);
uint16_t join_upper10_lower6(uint16_t, uint16_t, bool);

//end of buffer routines
int update_end_of_buff(struct tcp_parser *, char *, struct tcp_header *);

//finisher routines
void print_stats(struct tcp_parser *);
void free_parser(struct tcp_parser *);
void free_chan(struct dewe_chan *);

#endif /* TCP_UTILS_H_ */
