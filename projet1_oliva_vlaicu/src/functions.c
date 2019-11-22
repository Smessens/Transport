#include "functions.h"

void error(const char *msg){
  perror(msg);
  exit(1);
}

char *getIpv6(const char *host, const char *port, struct sockaddr_in6 *ipv6) {
    struct addrinfo *hostinfo;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    // We get the addrinfo from the host
    if(getaddrinfo(host, port, &hints, &hostinfo) != 0)
     	return "getIpv6: Unable to getaddrinfo";
    //The address we're looking for
    memset(ipv6, 0, sizeof(struct sockaddr_in6));
    memcpy((void*)ipv6, (void*)(hostinfo->ai_addr), sizeof(struct sockaddr_in6));
    freeaddrinfo(hostinfo);
    return NULL;
}

void printIpv6(struct sockaddr_in6 *ipv6){
  char ipstr[INET6_ADDRSTRLEN];
  void *addr = &(ipv6->sin6_addr);
  inet_ntop(AF_INET6, addr, ipstr, sizeof ipstr);
  write(2, ipstr, sizeof(ipstr));
}

int createSocket(const char *host, const char *port){

	// Set the receiver
	struct sockaddr_in6 receiver;
	memset(&receiver, 0, sizeof(receiver));
	char* err = getIpv6(host, port, &receiver);
	if(err!=NULL){
		error(err);
	}

	//receiver.sin6_addr = in6addr_any;
	//receiver.sin6_port = htons(atoi(port));

	// Create the listener socket
	int listener = socket(AF_INET6, SOCK_DGRAM, 0);
	if(listener<0){
		error("listeningLoop : Unable to create the socket");
	}

	// Bind it to the host address and port
	if(bind(listener, (const struct sockaddr *)&receiver, sizeof(receiver))<0){
		error("listeningLoop : Unable to bind the socket");
	}

	return listener;
}
