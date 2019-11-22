#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

#include <netdb.h>
#include <stdio.h>
#include "packet_interface.h"

/* Print error message on stdout and exit */
void error(const char *msg);

/* Get the ipv6 from the host */
char *getIpv6(const char *host, const char *port, struct sockaddr_in6 *ipv6);

/* Print the ipv6 of an addrinfo struct on stdout */
void printIpv6(struct sockaddr_in6 *ipv6);

/* Create a socket on host and port and return it */
int createSocket(const char *host, const char *port);

#endif
