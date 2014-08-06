/*
 * defaults.h
 *
 *  Created on: Mar 9, 2011
 *      Author: wibble
 *
 *  Jun 2, 2014, SMH: I've here completely ripped off MPD's qusb_acq
 */

#ifndef DEFAULTS_H_
#define DEFAULTS_H_

/* Size of data in bytes between frame sync pulses */
#define DEF_REVBUFSIZE 131200  // TCP data max receive size
#define DEF_PORT 5000 //Default TCP port

#define DEF_PREFIX "tcp_data"
#define DEF_OUTDIR "/home/spencerh/data/Wallops_TCPdata"
#define DEF_SLEEPTIME 0

#define DEF_CFSIZE 65536 // Colonel Frame size, words
// #define DEF_CFHEAD "Dartmouth College "
/* Accommodate the weirdness right now */
#define DEF_CFHEAD "Dartmouth College"

#define DEF_DIGITDATA false //Digitizer data
#define DEF_RTDSIZE 65536 // RTD Output size, words
#define DEF_RTDFILE "/tmp/rtd/rtd.data"
#define DEF_RTD_DT 0 // No RTD by default
#define DEF_RTDAVG 12

#define DEF_RUNMODE 0

//#define DEF_TIMEOUT 1000 //timeout of 1 second MAX!

#define MAXPORTS 4

//channel defaults
#define DEF_NUMCHANS 2
#define MAX_NUMCHANS 4

#endif /* DEFAULTS_H_ */
