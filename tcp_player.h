/*
 * tcp_player.h
 *
 *  Ripped off: Jun 3, 2014
 *      Author: wibble
 *      Thief: SMH
 */

#ifndef EPP_ACQ_H_
#define EPP_ACQ_H_

#include "tcp_player_struct.h"

void tcp_play(struct player_opt);
static void do_depart(int);
void *tcp_player_data_pt(void *);

#endif /* EPP_ACQ_H_ */
