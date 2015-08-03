/*
 * tcp_player_helpers.h
 *
 *  Ripped off: Jun 5, 2014
 *      Author: wibble
 *      Thief: SMH
 *
 *  2015/08/03 : High-speed version being written 
 */    

#ifndef EPP_HELPERS_H_
#define EPP_HELPERS_H_

#include <stdbool.h>
#include <stdint.h>

#include "tcp_player_struct.h"
#include "tcp_utils.h"
#include "defaults.h"

void init_opt(struct player_opt *);
int parse_opt(struct player_opt *, int, char **);

void printe(char *, ...);

/* For high-speed version! */
void init_opt_hs(struct player_opt *);
int parse_opt_hs(struct player_opt *, int, char **);

#endif /* EPP_HELPERS_H_ */
