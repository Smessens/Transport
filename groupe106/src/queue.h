#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "packet_interface.h"
#include "log.h"

// Structure to store the packet
typedef struct Elem_t Elem;

// Structure that points to structure ELem
typedef struct Queue_t Queue;

/*
* Initialize the structure queue
* RETURN the structure queu
*/
struct Queue* createQueue();

/*
* delete the structure queue
* @queue -> the structure queue to delete
* RETURN void
*/
void delQueue(struct Queue * queue);

/*
* to get the size of the queue
* @queue -> the structure queue
* RETURN the size of the queue
*/
int getSize(struct Queue *queue);

/*
* to get the first elem in the queue
* @queue -> the structure queue
* RETURN the first elem in the queue
*/
struct Elem *getFront(struct Queue *queue);

/*
* to get the time in an elem structure
* @elem -> the elem structure
* RETURN the time in the structure elem
*/
struct timespec getTimeElem(struct Elem* elem);

/*
* say if the queue is empty or not
* @queue -> the structure queue
* RETURN 0 if the queue is not empty and 1 if the queue is empty
*/
int isEmpty(struct Queue* queue);

/*
* to get the next elem
* @elem -> the structure elem
* RETURN the next elem that is in his variable elem->next
*/
struct Elem* getNext(struct Elem* elem);

/*
* to set the timer of an elem
* @elem -> the structure elem
* RETURN void
*/
void setTime(struct Elem * elem);

/*
* to print the seqnum of the packet that are in the queue
* @elem -> the structure elem
* RETURN void
*/
void printSeq(struct Elem * elem);

/*
* function that add a packet in the queue
* @queue -> the structure queue
* @key -> the packet that we want to add in the queue
* RETURN void
*/
void addQueue(struct Queue *queue, pkt_t* key);

/*
* function that delete the packet that has the corresponding sequence number to seq
* @queue -> the structure queue
* @seq -> the sequence number that we want to delete
* RETURN the packet that we had deleted from the queue or NULL if the packet is not here
*/
pkt_t * deQueueReceiver(struct Queue * queue, int seq);

/*
* function that search the packet that has the corresponding sequence number to seq
* @queue -> the structure queue
* @seq -> the sequence number that we want to search in the queue
* RETURN the packet that we had been find in the queue or NULL if the packet is not here
*/
pkt_t * searchPkt(struct Queue* queue, int seq);

/*
* to have the packet in the structure elem
* @elem -> the structure elem
* RETURN the packet that are store in this structure elem
*/
pkt_t* getPackett(struct Elem* element);

/*
* this structure is used to find the packet with the longest RTT and the shortest RTT
* @elem -> the structure elem
* @temps -> the time since the first time the packet has been sent
* RETURN void
*/
void addTime(struct Elem * element, int temps);

/*
* get the time since the first time the sender send the packet that has the corresponding sequence number to seq
* @queue -> the structure queue
* @seq -> the sequence number that we want to search in the queue
* RETURN the timer of the packet
*/
int getTimee(struct Queue* queue, int seq);
