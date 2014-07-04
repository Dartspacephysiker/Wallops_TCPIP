/* Ripped from 'http://stackoverflow.com/questions/10686368/file-transfer-using-tcp-on-linux'
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/socket.h>
#define PORT 5000 // The port which is communicate with server
#define BACKLOG 10
#define LENGTH 65536 // Buffer length
int main (int argc, char * argv[])
{
  char sdbuf[LENGTH]; // Send buffer

  char *f_name;
  int sockfd; // Socket file descriptor
  int nsockfd; // New Socket file descriptor
  int optval = 1;
  int num;
  int sin_size; // to store struct size
  struct sockaddr_in addr_local;
  struct sockaddr_in addr_remote;

  if(argc != 2){
    printf("Usage:\t./server <file to send>\n");
    return(EXIT_FAILURE);
  }
  else {
    f_name = argv[argc-1];
  }

  /* Get the Socket file descriptor */
  if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
    {
      printf ("ERROR: Failed to obtain Socket Descriptor.\n");
      return (0);
    }
  else printf ("[server] obtain socket descriptor successfully.\n");
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  /* Fill the local socket address struct */
  addr_local.sin_family = AF_INET; // Protocol Family
  addr_local.sin_port = htons(PORT); // Port number
  addr_local.sin_addr.s_addr = INADDR_ANY; // AutoFill local address
  bzero(&(addr_local.sin_zero), 8); // Flush the rest of struct

  /* Bind a special Port */
  if( bind(sockfd, (struct sockaddr*)&addr_local, sizeof(struct sockaddr)) == -1 )
    {
      printf ("ERROR: Failed to bind Port %d.\n",PORT);
      return (0);
    }
  else printf("[server] bind tcp port %d in addr 0.0.0.0 sucessfully.\n",PORT);

  /* Listen remote connect/calling */
  if(listen(sockfd,BACKLOG) == -1)
    {
      printf ("ERROR: Failed to listen Port %d.\n", PORT);
      return (0);
    }
  else printf ("[server] listening the port %d sucessfully.\n", PORT);
  int success = 0;
  while(success == 0)
    {
      sin_size = sizeof(struct sockaddr_in);

      /* Wait a connection, and obtain a new socket file despriptor for single connection */
      if ((nsockfd = accept(sockfd, (struct sockaddr *)&addr_remote, &sin_size)) == -1) 
	printf ("ERROR: Obtain new Socket Descriptor error.\n");
      //      else printf ("[server] server has got connect from %s.\n", inet_ntoa(addr_remote.sin_addr));

      printf("[server] send %s to the client...", f_name);
      FILE *fp = fopen(f_name, "r");
      if(fp == NULL)
	{
	  printf("ERROR: File %s not found.\n", f_name);
	  exit(1);
	}
      bzero(sdbuf, LENGTH);
      int f_block_sz;
      while((f_block_sz = fread(sdbuf, sizeof(char), LENGTH, fp))>0)
	{
	  if(send(nsockfd, sdbuf, f_block_sz, 0) < 0)
	    {
	      printf("ERROR: Failed to send file %s.\n", f_name);
	      break;
	    }
	  bzero(sdbuf, LENGTH);
	}
      printf("ok!\n");
      success = 1;
      close(nsockfd);
      printf("[server] connection closed.\n");
      while(waitpid(-1, NULL, WNOHANG) > 0);
    }
}
