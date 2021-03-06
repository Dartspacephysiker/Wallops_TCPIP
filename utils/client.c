/* client.c 
Ripped from 'http://stackoverflow.com/questions/10686368/file-transfer-using-tcp-on-linux'
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define DEF_PORT 5000
#define LENGTH 512 // Buffer length
int main(int argc, char *argv[])
{
  char* f_name;
  //  int err;

  int  imod = 10;

  char *servaddr;
  int servport = DEF_PORT;
  
  int sockfd; // Socket file descriptor
  char sdbuf[LENGTH]; // Send buffer
  struct sockaddr_in remote_addr; 

  servaddr = malloc(sizeof(char) * 20);

  if( argc == 3 ) {
    f_name = argv[1];
    strcpy(servaddr,argv[2]);
    printf("File to send: %s\nAttempting to connect to server %s on port %i\n",f_name, servaddr,servport);
  } 
  else if(argc == 4){
    f_name = argv[1];
    strcpy(servaddr,argv[2]);
    servport = atoi(argv[3]);
    printf("File to send: %s\nAttempting to connect to server %s on port %i\n",f_name, servaddr,servport);

  }
  else {
    printf("Usage: ./client <file to send> <server_ip_addr> <server port number (Default: %i)>\n",DEF_PORT);
    exit(EXIT_FAILURE);
  }
  
  FILE *fp = fopen(f_name, "r");
  if(fp == NULL) {
    printf("File %s cannot be opened.\n", f_name);
    exit(errno);
  }

  /* Get the Socket file descriptor */
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
      printf("ERROR: Failed to obtain Socket Descriptor!\n");
      return (0);
    }
  /* Fill the socket address struct */
  remote_addr.sin_family = AF_INET; 
  remote_addr.sin_port = htons(servport); 
  //  inet_pton(AF_INET, "10.170.26.78", &remote_addr.sin_addr); 
  if( inet_pton(AF_INET, servaddr, &remote_addr.sin_addr ) != 1 )
    {
      fprintf(stderr,"Bad address provided!\nCould not convert '%s' to valid binary IPv4 address...\n",servaddr);
      return(EXIT_FAILURE);
    } 
  bzero(&(remote_addr.sin_zero), 8);
  /* Try to connect the remote */
  if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1)
    {
      printf ("ERROR: Failed to connect to the host!\n");
      return (0);
    }
  else printf("[client] connected to server at port %d...OK!\n", servport);

  printf("[client] sending to server: %s...\n",f_name);

  bzero(sdbuf, LENGTH);
  int success = 0;

  while(success == 0)
    {
      /*********************************************/
      printf("[client] Now sending %s to the server...", f_name);
      bzero(sdbuf, LENGTH);
      int f_block_sz;
      int i = 0;
      int bytes_sent;
      long long int tot_bytes_sent = 0;
      while( ( f_block_sz = fread(sdbuf, sizeof(char), LENGTH, fp) ) >0 )
	{
	  if( ( bytes_sent = send(sockfd, sdbuf, f_block_sz, 0) ) < 0)
	    {
	      printf("ERROR: Failed to send file %s.\n", f_name);
	      break;
	    }
	  if( ( i++ % imod ) == 0)
	    {
	      printf("Sent %i bytes\n",bytes_sent);
	    }
	  tot_bytes_sent += bytes_sent;
	  bzero(sdbuf, LENGTH);
	}
      printf("OK!\n");
      success = 1;
      fclose(fp);
      printf("Sent %lli total bytes\n",tot_bytes_sent);
      /*********************************************/
    }
      /*original*/
    /* 	  while(f_block_sz = recv(sockfd, revbuf, LENGTH, 0)) */
    /* 	    { */
    /* 	      if(f_block_sz < 0) */
    /* 		{ */
    /* 		  printf("Receive file error.\n"); */
    /* 		  break; */
    /* 		} */
    /* 	      else if(f_block_sz == 0) */
    /* 		{ */
    /* 		  printf("[client] connection lost.\n"); */
    /* 		  break; */
    /* 		} */
    /* 	      int write_sz = fwrite(revbuf, sizeof(char), f_block_sz, fp); */
    /* 	      if(write_sz < f_block_sz) */
    /* 		{ */
    /* 		  printf("File write failed.\n"); */
    /* 		  break; */
    /* 		} */
    /* 	      bzero(revbuf, LENGTH); */
    /* 	    } */
    /* 	  printf("ok!\n"); */
    /* 	  success = 1; */
    /* 	  fclose(fp); */
    /* 	} */
    /* } */
  close (sockfd);
  return (0);
}
