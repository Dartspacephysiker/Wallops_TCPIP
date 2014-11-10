/*
 * tcp_tstamp_to_ascii.c 
 *
 * Aug 8 2014
 * Author: Hammertime @ Wallops
 *
 */

#define _GNU_SOURCE

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcp_tstamp_to_ascii.h"

int main( int argc, char **argv ){

  char *ts_fname;
  FILE *ts_infile;
  
  char *ofname;
  FILE *ofile;

  size_t ts_dsize = sizeof(double);
  double tstamp, oldtstamp;
  long int tcount = 0;


  //optional ts_infile stuff
  bool do_data;
  size_t dsize = sizeof(uint16_t);
  uint16_t datum, oldatum;
  char *d_fname;
  FILE *d_infile;

  //Handle command line  
  if(argc == 2) {
    ts_fname = strdup(argv[1]);
  }
  else if(argc == 3){
    ts_fname = strdup(argv[1]);
    do_data = true;
    if( ( strncmp( argv[2], "1", 1)  ) == 0 ){
      char *tmp =strstr( ts_fname, "tstamps.data");
      size_t len = (size_t)((long int)tmp - (long int)ts_fname - 1);
      printf("len: %zi\n", len);
      d_fname = malloc( strlen(ts_fname) );
      strncpy( d_fname, ts_fname, len );
      strncat( d_fname, ".data", 5);
    }
    else {
      d_fname = strdup(argv[2]);
    }
  }
  else {
    printf("%s <tcp timestamp file> <optional data file>\n",argv[0]);
    printf("\tUses a timestamp file outputted by either tcp_fileparse or tcp_player and creates a single-column ASCII .txt file\n");
    printf("\tNOTE: if <optional data file> is set to \"1\", the program assumes the same filename prefix for the data file\n");
    return(EXIT_SUCCESS);
  }

  //txt file name
  //  strstr( ts_fname, "tstamp.data
  char *tmp =strstr( ts_fname, "tstamps.data");
  if( tmp != NULL ) { 
    size_t len = (size_t)((long int)tmp - (long int)ts_fname);
    printf("len: %zi\n", len);
    ofname = malloc( 256 );
    strncpy( ofname, ts_fname, len );
    if( do_data ) {
      strncat( ofname, "samples_tstamps.txt", 19); 
    }
    else {
      strncat( ofname, "tstamps.txt", 11);
    }
  }
  else{ 
    printf("Is this a bona fide tstamp file? Couldn't find \"tstamp.data\" at end of string\n"); 
    return(EXIT_FAILURE);
  }

  //Open tstamp file
  printf("Opening timestamp file %s\n",ts_fname);
  if( ( ts_infile = fopen(ts_fname,"rb") ) == NULL ){
    fprintf(stderr,"Gerrorg. Couldn't open file %s.\n",ts_fname);
    return(EXIT_FAILURE);
  }

  if( do_data ){
    printf("Opening timestamp file %s\n",d_fname);
    if( ( d_infile = fopen(d_fname,"rb") ) == NULL ){
      fprintf(stderr,"Gerrorg. Couldn't open file %s.\n",d_fname);
      return(EXIT_FAILURE);
    }
  }

  //Open txt file
  printf("Opening output file %s\n",ofname);
  if( ( ofile = fopen(ofname, "w") ) == NULL ){
    fprintf(stderr,"Gerrorg. Couldn't open file %s.\n",ofname);
    return(EXIT_FAILURE);
  }

  //init for loop
  //  oldtstamp = tstamp = 0;
  fread( &oldtstamp, ts_dsize, 1, ts_infile );
  if( do_data )   fread( &oldatum, dsize, 1, d_infile );

  //loop through file
  if( !do_data ) {
    fprintf( ofile, "#\ttstamp\tdiff\n");
    fprintf( ofile, "0\t%.7f\t----\n", oldtstamp );    
    while( fread( &tstamp, ts_dsize, 1, ts_infile ) > 0 ){
      fprintf( ofile, "%li\t%.7f\t%.7f\n", tcount, tstamp, tstamp-oldtstamp );
      oldtstamp = tstamp;
      tcount ++;
    }
  }
  else {
    fprintf( ofile, "#\tsample\tsampdiff\ttstamp\tsdiff\n");
    fprintf( ofile, "%li\t%"PRIi16"\t----\t%.7f\t----\n", tcount, oldatum, oldtstamp );
    while( fread( &tstamp, ts_dsize, 1, ts_infile ) > 0 ){ 
      fread( &datum, dsize, 1, d_infile );
      fprintf( ofile, "%li\t%"PRIu16"\t%"PRIi16"\t\t%.7f\t%.7f\n", tcount, datum, ((int16_t)datum - (int16_t)oldatum), tstamp, tstamp-oldtstamp );
      oldtstamp = tstamp;
      oldatum = datum;
      tcount ++;
    }
  }
  fprintf( ofile, "\n");
  
  printf("Finished!!\n");
  printf("Wrote %li timestamps to %s\n", tcount, ofname );
  
  //clean up shop
  free(ts_fname);
  free(ofname);

  fclose(ts_infile);
  fclose(ofile);

  return EXIT_SUCCESS;
}
