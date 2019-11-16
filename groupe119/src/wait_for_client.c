#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

#include "wait_for_client.h"

int wait_for_client(int sfd) {

	struct sockaddr_in6 src_addr;
	socklen_t addrlen = sizeof(struct sockaddr_in6);

	char buf[1024];

	if(recvfrom(sfd, (void *) buf, 1024, MSG_PEEK, (struct sockaddr*)&src_addr, &addrlen) == -1) {
		fprintf(stderr, "Erreur lors du recvfrom\n");
		return -1;
	}

	if(connect(sfd, (struct sockaddr*) &src_addr, addrlen) == -1) {
		fprintf(stderr, "%s\n", strerror(errno));
		fprintf(stderr, "ERROR CONNECTING\n");
		return -1;
	}

    fprintf(stderr, "Connected with client ! \n");

	return 0;
}
