/*
 * defaults.h
 *
 *  Created on: Mar 9, 2011
 *      Author: wibble
 *
 *  Jun 2, 2014, SMH: I've here completely ripped off MPD's qusb_acq
 *  Aug 3, 2015, SMH: Creating new, high-speed version of tcp_player
 */

#ifndef DEFAULTS_H_
#define DEFAULTS_H_

/* Size of data in bytes between frame sync pulses */
#define DEF_REVBUFSIZE 131200  // TCP data max receive size
#define DEF_PORT 5000 //Default TCP port

#define DEF_PREFIX "tcp_data"
#define DEF_OUTDIR "/home/spencerh/data/CAPER/Wallops_TCPdata"
#define DEF_SLEEPTIME 0
#define DEF_SLEEPTIME_HS 500

#define DEF_HS_REVBUFSIZE 1312000 //TCP data max receive size for highspeedversion
#define DEF_HS_PREFIX "tcp_data_highspeed"
#define DEF_HS_IS_SYNCHR_CHAN false

#define DEF_DIGITDATA false //Digitizer data
#define DEF_RTDSIZE 65536 // RTD Output size, words
#define DEF_RTDFILE "/tmp/rtd/rtd_tcp.data"
#define DEF_RTD_DT 0 // No RTD by default
#define DEF_RTDAVG 12

#define DEF_RUNMODE 0

#define MAXPORTS 4

//channel defaults
#define DEF_NUMCHANS 2
#define MAX_NUMCHANS 2

//#define MAXNUMSAMPS

#endif /* DEFAULTS_H_ */
