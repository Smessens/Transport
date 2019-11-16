#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>

#include "create_socket.h"

int create_socket(struct sockaddr_in6 *source_addr, int src_port, struct sockaddr_in6 *dest_addr, int dst_port) {

	int fd = socket(AF_INET6, SOCK_DGRAM,  IPPROTO_UDP);
	if(fd == -1) {
		fprintf(stderr, "ERROR CREATING SOCKET\n");
		return -1;
	}

	if(source_addr != NULL && src_port > 0) {
		source_addr->sin6_family = AF_INET6;
		source_addr->sin6_port = htons( src_port );

		if(bind(fd, (struct sockaddr*) source_addr, sizeof(struct sockaddr_in6)) == -1) {
		  fprintf(stderr, "%s\n", strerror(errno));
			fprintf(stderr, "ERROR CREATING BIND\n");
			return -1;
		}
	}

	if(dest_addr != NULL && dst_port > 0) {

		dest_addr->sin6_port = htons(dst_port);
		dest_addr->sin6_family = AF_INET6;

		if(connect(fd, (struct sockaddr *) dest_addr, sizeof(struct sockaddr_in6)) == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
			fprintf(stderr, "ERROR CREATING CONNECT\n");
			return -1;
		}
	}

    return fd;
}
