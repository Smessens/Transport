#ifndef LINFO1341_ADDITIONALFUNCTION_H
#define LINFO1341_ADDITIONALFUNCTION_H

#include <netinet/in.h> /* * sockaddr_in6 */
#include <sys/types.h> /* sockaddr_in6 */
#include "packet_interface.h"

/*
//structure for receiver
typedef struct node_rec {
    pkt_t *pkt;
    struct node_rec *next;
} rec_node_t;
typedef struct queue_rec{
    struct node_rec *head;
    int size;
} rec_queue_t;
*/



/* Creates a socket and initialize it
 * @source_addr: if !NULL, the source address that should be bound to this socket
 * @src_port: if >0, the port on which the socket is listening
 * @dest_addr: if !NULL, the destination address to which the socket should send data
 * @dst_port: if >0, the destination port to which the socket should be connected
 * @return: a file descriptor number representing the socket,
 *         or -1 in case of error (explanation will be printed on stderr)
 */
int create_socket(struct sockaddr_in6 *source_addr,
                  int src_port,
                  struct sockaddr_in6 *dest_addr,
                  int dst_port);

/* Resolve the resource name to an usable IPv6 address
 * @address: The name to resolve
 * @rval: Where the resulting IPv6 address descriptor should be stored
 * @return: NULL if it succeeded, or a pointer towards
 *          a string describing the error if any.
 *          (const char* means the caller cannot modify or free the return value,
 *           so do not use malloc!)
 */
const char * real_address(const char *address, struct sockaddr_in6 *rval);

/* Block the caller until a message is received on sfd,
 * and connect the socket to the source addresse of the received message.
 * @sfd: a file descriptor to a bound socket but not yet connected
 * @return: 0 in case of success, -1 otherwise
 * @POST: This call is idempotent, it does not 'consume' the data of the message,
 * and could be repeated several times blocking only at the first call.
 */
int wait_for_client(int sfd);

/* Loop reading a socket and printing to stdout,
 * while reading stdin and writing to the socket
 * @sfd: The socket file descriptor. It is both bound and connected.
 * @return: as soon as stdin signals EOF
 */

//*******************************************************************
//ADD FONCTION FOR RECEIVE
//*******************************************************************
typedef struct node {
    int seqnum;
    uint32_t timestamp;
    char* pay;
    int lenpay;
    struct node *next;
}node_receive;

typedef struct queue{
    struct node *head;
    int size;
}queue_receive;

//init a empty queue
queue_receive* new_queue();

//init a empty node
node_receive* new_node(int seq, uint32_t stamptime, char* payload, int len);

//free a node
void node_free(node_receive *nodeReceive);

//free the queue
void queue_free(queue_receive *queueReceive);

//Push a new node in Queue in order of seqnum
//return 0 if the push sucess, 0 if already in the buffer
//The Buffer will always be in this order : [segnum waited, ..., 255, 0, ..., segnum waited -1]
int push_order(queue_receive *queu, node_receive *newnode, int seqwait);

//pop de first element of the structure and return it ONLY IF the seqnum correspond to the seqnum waited
node_receive* pop_queue(queue_receive* queueReceive, int seqwait);



#endif //LINFO1341_ADDITIONALFUNCTION_H
