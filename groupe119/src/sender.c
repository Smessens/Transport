#include <stdlib.h> /* EXIT_X */
#include <stdio.h> /* fprintf */
#include <unistd.h> /* getopt */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "real_address.h"
#include "create_socket.h"
#include "read_write_loop.h"
#include "wait_for_client.h"

int main(int argc, char *argv[])
{
	int opt;
  char* fileIn = NULL;

	while ((opt = getopt(argc, argv, "f:")) != -1) {
        if(opt == 'f') {
            fileIn = optarg;
		}
	}

	char *host = argv[optind];
	int port = atoi(argv[optind+1]);

	struct sockaddr_in6 addr;
	const char *err = real_address(host, &addr);

	if (err) {
		fprintf(stderr, "Could not resolve hostname %s: %s\n", host, err);
		return EXIT_FAILURE;
	}

  int sfd;
    sfd = create_socket(NULL, -1, &addr, port);
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, NULL, 0);

	if (sfd < 0) {
		fprintf(stderr, "Failed to create the socket!\n");
		return EXIT_FAILURE;
	}

	int fd;
  if(fileIn == NULL) {
  	fd = fileno(stdin);
  }
  else {
  	fd = open(fileIn, 0, O_WRONLY);
	}
  dup2(fd, fileno(stdin));

	read_write_loop_sender(sfd);

	close(sfd);
  
	return EXIT_SUCCESS;
}
