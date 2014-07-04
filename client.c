/* Ripped from 'http://stackoverflow.com/questions/10686368/file-transfer-using-tcp-on-linux'
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define PORT 5000
#define LENGTH 512 // Buffer length
int main(int argc, char *argv[])
{
  char* f_name = "receive.txt";
  FILE *fp = fopen(f_name, "a");

  int sockfd; // Socket file descriptor
  char revbuf[LENGTH]; // Receiver buffer
  struct sockaddr_in remote_addr; 

  /* Get the Socket file descriptor */
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
      printf("ERROR: Failed to obtain Socket Descriptor!\n");
      return (0);
    }
  /* Fill the socket address struct */
  remote_addr.sin_family = AF_INET; 
  remote_addr.sin_port = htons(PORT); 
  inet_pton(AF_INET, "127.0.0.1", &remote_addr.sin_addr); 
  bzero(&(remote_addr.sin_zero), 8);
  /* Try to connect the remote */
  if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1)
    {
      printf ("ERROR: Failed to connect to the host!\n");
      return (0);
    }
  else printf("[client] connected to server at port %d...ok!\n", PORT);
  //printf ("OK: Have connected to %s\n",argv[1]);
  printf("[client] receive file sent by server to %s...",f_name);
  if(fp == NULL) {
    printf("File %s cannot be opened.\n", f_name);
    exit(errno);
  }
  else
    {
      bzero(revbuf, LENGTH);
      int f_block_sz = 0;
      int success = 0;
      while(success == 0)
	{
	  while(f_block_sz = recv(sockfd, revbuf, LENGTH, 0))
	    {
	      if(f_block_sz < 0)
		{
		  printf("Receive file error.\n");
		  break;
		}
	      else if(f_block_sz == 0)
		{
		  printf("[client] connection lost.\n");
		  break;
		}
	      int write_sz = fwrite(revbuf, sizeof(char), f_block_sz, fp);
	      if(write_sz < f_block_sz)
		{
		  printf("File write failed.\n");
		  break;
		}
	      bzero(revbuf, LENGTH);
	    }
	  printf("ok!\n");
	  success = 1;
	  fclose(fp);
	}
    }
  close (sockfd);
  return (0);
}
