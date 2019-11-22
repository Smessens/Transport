#include "functions.h"
#include "packet_interface.h"
#include "buffer.h"

#include <poll.h>
#include <signal.h>

static volatile int keepRunning = 1;
int n = 1;

void intHandler(int dummy) {
  keepRunning = 0;
}

/* Infinite loop that will listen the socket and receive the data */
int listeningLoop(const char *host, const char *port);

/* ------------------------ MAIN ------------------------ */

/* Set up the options from the arguments and starts other functions */
int main(int argc, char *argv[]){

  char *hostname;
  char *port;
  char *format=NULL;

  // Get args
  int opt=getopt(argc, argv, "o:m:");
  while(opt!=-1){
    if(opt=='m'){
      n=atoi(optarg);
      if(n<0){
        error("ERROR, please enter a valid number of connections -m. (N>=0)");
      }
    }
    else if(opt=='o'){
      format=optarg;
    }
    else{
      error("wrong arguments.");
    }
    opt=getopt(argc, argv, "o:m:");
  }
  if (argv[optind] == NULL || argv[optind + 1] == NULL) {
    error("ERROR: Mandatory argument(s) missing: hostname port (in this order)\n");
    exit(1);
  }
  else{
    hostname=argv[optind];
    port=argv[optind+1];
  }

  printf("host %s; port %s; format %s; number of connections %d\n", hostname, port, format, n);

  // Set senderlimit
  set_senderlimit(n);

  // Set format
  if(format!=NULL){
    set_format(format);
  } else {
    set_format("%d");
  }

  // Start infinite loop
  listeningLoop(hostname, port);
  return 0;
}

int listeningLoop(const char *host, const char *port){

  // Create the listening socket
  int listener = createSocket(host, port);

  // Prepare structure for sender
  struct sockaddr_in6 sender;
  memset(&sender, 0, sizeof(sender));
  socklen_t senderlen = (sizeof(struct sockaddr_in6));
  char buf[528];

  // Initialize buffer
  init_bufferlist();
  init_fdlist();

  // Set listening socklen
  set_mainsocket(listener);

  // Catch ctrl c
  signal(SIGINT, intHandler);

  // Prepare for poll()
  int ret, pret;
  struct pollfd fds[1];
  int timeout = 1;

  // Infinite loop to listen
  while (keepRunning){

    fds[0].fd = listener;
    fds[0].events = POLLIN;

    pret = poll(fds, 1, timeout);
    if(pret==-1){ // If ctrl + c
      free_bufferlist();
      free_fdlist();
      free_format();
      close(listener);
      printf("Loop finished\n"); // Loops finishes here when we ctrl c
    } else if(pret==0){ // If timeout we can write
      if(is_buffer_empty()==0){
        int firstfd = get_first_fd();
        write_packet(firstfd);
      }
    } else { // Receive packet
      if(get_buffer_size()>=(30*n) && is_buffer_empty()==0){ // If the buffer is full we  have to write
        int firstfd = get_first_fd();
        write_packet(firstfd);
      }
      memset(buf, 0, 528);
      memset(&sender, 0, sizeof(sender));
      if((ret = recvfrom(listener, buf, 528, 0, (struct sockaddr *)&sender, &senderlen))<0){
        error("listeningLoop : Unable to recvfrom");
      }
      receive_packet(&sender, buf, 528);
    }
  }
  close(listener);
  return 0;
}
