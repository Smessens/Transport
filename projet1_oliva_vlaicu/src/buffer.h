#ifndef _BUFFER_H_
#define _BUFFER_H_

#include "packet_interface.h"

#include <stddef.h>

// Struct used for buffer

typedef struct bufferItem bi_t;

typedef struct bufferItemList bil_t;

typedef struct fdItem fdi_t;

typedef struct fdItemList fdil_t;

// Extern variables used for buffer and main

extern bil_t *bufferlist;
extern fdil_t *fdlist;
extern int maxsender;
extern int filecount;
extern int mainsocket;



/////////////////////////////////////////
// BUFFER_ITEM / LIST

/* Create new bufferItemList
Returns 0 on success, -1 otherwise*/
int init_bufferlist();

/* Add decoded data in the buffer
Returns 0 on success, -1 otherwise*/
int add_packet(bi_t *bi, int previouspkt);

/* Removes a bi from the buffer
Returns 0 on success, -1 otherwise*/
int remove_packet(int index);

/* Creates a BufferItem */
bi_t* create_bi(pkt_t *pkt, int fd);

/* Returns bufferItemList current size */
int get_buffer_size();

/* Returns the first element of the bufferItemList*/
bi_t* get_buffer_first();

/* Returns the last element of the bufferItemList*/
bi_t* get_buffer_last();

/* Returns the packet of a bufferItem*/
pkt_t* get_bi_packet(bi_t *bi);


/* Returns the next bufferItem of a bufferItem*/
bi_t* get_bi_next(bi_t *bi);

/* Returns the fd of a bufferItem*/
int get_bi_fd(bi_t *bi);

/* Returns 1 if buffer of bufferItem is empty
Returns 0 otherwise*/
int is_buffer_empty();

/* Free a bufferItemList */
void free_bufferlist();



/////////////////////////////////////////
// FD_ITEM / LIST

/* Create new fdItemList
Returns 0 on success, -1 otherwise*/
int init_fdlist();

/* Returns first fdi from linked list */
int get_first_fd();

/* Returns fdi containing fd */
fdi_t *get_fdi(int fd);

/* Add fdi struct to the linked list
Returns 0 on success, -1 otherwise*/
int add_fd(struct sockaddr_in6 *addr);

/* Removes a fdi from the linked list
Returns 0 on success, -1 otherwise*/
int remove_fd(int fd);

/* Returns 1 if linked list of fdi is empty
Returns 0 otherwise*/
int is_fd_empty();

/* Free a fdItemList */
void free_fdlist();



/////////////////////////////////////////
// RECEIVENG - SENDING - WRITING PACKETS

/* Returns the next seqnum to ack */
uint8_t ack_seq(int fd);

/* Sends a nack
Returns 0 on success, -1 otherwise*/
int send_nack(uint8_t seqnum, struct sockaddr_in6 *addr, int timestamp);

/* Sends an ack to the specified address
Returns 0 on success, -1 otherwise*/
int send_ack(int fd, uint8_t seqnum, struct sockaddr_in6 *addr, int timestamp, int resentlastack);

/* Writes packets from the buffer to a specific file
Returns 0 on success, -1 otherwise*/
int write_packet(int fd);

/* Receives a packet data and handles it
Returns 0 on success, -1 otherwise*/
int receive_packet(struct sockaddr_in6 *addr, char *data, size_t len);

/* Sets the sender limit */
void set_senderlimit(int n);

/* Sets the listening socket */
void set_mainsocket(int n);

/* Sets the format for outpul files
Returns 0 on success, -1 otherwise*/
int set_format(char *str);

/* Free format of output file */
void free_format();


/////////////////////////////////////////
// Printing in stdout
void print_fdlist();

void print_bufferlist();

#endif
