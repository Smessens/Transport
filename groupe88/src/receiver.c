#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "socket_interface.h"

int main(int argc, char const *argv[]) {
  // hostname port [-o format] [-m N]

  if(argc<3||argc>7)
  {
    fprintf(stderr,"Error : improper use of receiver\nHow to use receiver : ./receiver hostname port [-o format] [-m N]");
    return(EXIT_FAILURE);
  }
  
  char * format = "%d";
  int N = 100;

  if(argc>3)
  {
    int opt;
    while((opt = getopt(argc, (char * const*)argv, "o:m:"))!=-1)
     {
       switch(opt)
       {
         case 'o':
           format = optarg;
           break;
         case 'm':
           N = atoi(optarg);
           break;
         default:
           fprintf(stderr, "Error : improper use of receiver\nHow to use receiver : ./receiver hostname port [-o format] [-m N]");
           return(EXIT_FAILURE);
       }
     }
  }

  char * hostname = (char*)argv[optind];
  char * port = (char*)argv[optind+1];

  int sock = create_socket_real_adress(hostname, port);  /* Bound */

  if (sock < 0)
  {
    fprintf(stderr, "Failed to create the socket!\n");
    return(EXIT_FAILURE);
  }

  read_write_loop(sock,N,format);

  close(sock); //Renvoie -1 si fail ???

  return(EXIT_SUCCESS);
}
