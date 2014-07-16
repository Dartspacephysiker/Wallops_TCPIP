/*
 * simple_fifo.c
 *
 *  Created on: Mar 1, 2011
 *      Author: wibble
 *
 *  fifo_skip and fifo_search during Jul 2014 by Spencer Hatch, MPD
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_fifo.h"

int fifo_init(struct simple_fifo *fifo, long int size) {
	int ret = 0;

	fifo->size = size;
	fifo->base = malloc(size);
	if (fifo->base == NULL) {
		ret = EFIFO_MEM;
	}

	fifo->tail = fifo->base+size;
	fifo->head = fifo->tail;

	return(ret);
}

int fifo_write(struct simple_fifo *fifo, char *new_data, long int new_size) {
  long int span = fifo->size - new_size; // Amount of data to save
  long int shift = fifo->head - fifo->base; // Current read point

  //	printf("fifo_write(): have %li, adding %li.\n", fifo->tail-fifo->head, new_size);fflush(stdout);

  if (new_size >= shift) {
    memmove(fifo->base, fifo->base+new_size, span);
    memmove(fifo->base+span, new_data, new_size);
    fifo->head = fifo->base;
  } else {
    memmove(fifo->head-new_size, fifo->head, fifo->tail-fifo->head);
    memmove(fifo->tail-new_size, new_data, new_size);
    fifo->head = fifo->head - new_size;
  }

  return(0);
}

long int fifo_read(char *out, struct simple_fifo *fifo, long int bytes) {
  long int ret = 0;
  void *pret;

  //	printf("fifo_read(): want %li, have %li.\n", bytes, fifo_avail(fifo));fflush(stdout);

  if (bytes <= fifo_avail(fifo)) {
    if (out == NULL) out = malloc(bytes); // Malloc if out is new

    pret = memmove(out, fifo->head, bytes); // Move data
    if (pret != NULL) {
      fifo->head += bytes;
      ret = bytes;
    }
  }

  return(ret);
}

int fifo_kill(struct simple_fifo *fifo, long int bytes) {
  int ret = 0;

  if (bytes <= fifo_avail(fifo)) {
    fifo->head += bytes;
    ret = bytes;
  }

  return(ret);
}

void fifo_destroy(struct simple_fifo *fifo) {
  free(fifo->base);
}

long int fifo_avail(struct simple_fifo *fifo) {
  return(fifo->tail - fifo->head);
}

/*Returns the distance of the beginning of the string relative to the head of the FIFO*/
long int fifo_search(struct simple_fifo *fifo, char *search_str, size_t search_len) {
  
  void *ret = NULL;
  
  ret = memmem(fifo->head, search_len, search_str, strlen(search_str) );

  if(ret == NULL) {
    return EXIT_FAILURE;
  } 
  return ret - (void *)fifo->head;
}
 

/* Returns the distance from fifo->head to the location where the beginning of the skipped byte string is found
 * AFTER resizing the FIFO.
 * This function moves all data between fifo->head and the first instance of skip_str forward by "skipbytes" bytes
 * So it effectively resizes the data available in the FIFO by overwriting all bytes to be skipped
 */
long int fifo_skip(char *skip_str, long int start_loc, long int skipbytes, size_t search_len, struct simple_fifo *fifo) {
  void *str_loc;
  long int span; // Amount of data between fifo->head and str_loc
  //  long int shift = fifo->head - fifo->base; // Current read point

  /* Calculate distance from head to skip_str */
  str_loc = memmem(fifo->head+start_loc, search_len, skip_str, strlen(skip_str) ); 
  if( str_loc != NULL ){

    span = (long int)str_loc - (long int)fifo->head; 

    /* Move all data between fifo->head and str_loc forward by skipbytes, overwriting all bytes to be skipped */
    memmove(fifo->head + skipbytes, fifo->head, span);
    
    fifo->head += skipbytes;
    
    return span; 
    }
  else {
    return EXIT_FAILURE;
  }
}
