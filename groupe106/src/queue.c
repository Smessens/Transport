#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <time.h>
#include "packet_interface.h"
#include "queue.h"
#include "log.h"

struct Elem{
  pkt_t * packet;
  struct Elem *next;
  struct timespec timee;
  int tpsmilli;
};

struct Queue{
  struct Elem *front,  *back;
  int maxsize;
  int size;
};

struct Queue* createQueue(){
  struct Queue * queue = (struct Queue *)malloc(sizeof(struct Queue));
  if(queue==NULL) return NULL;
  queue->front=NULL;
  queue->back=NULL;
  queue->size=0;
  return queue;
}

void delQueue(struct Queue * queue){
  free(queue->front);
  free(queue);
}

int getSize(struct Queue *queue){
  return queue->size;
}

struct Elem *getFront(struct Queue *queue){
  return queue->front;
}

struct timespec getTimeElem(struct Elem* elem){
  return elem->timee;
}

int isEmpty(struct Queue* queue){
  return queue->size==0;
}

struct Elem* getNext(struct Elem* elem){
  return elem->next;
}

void setTime(struct Elem * elem){
  int err  = clock_gettime(CLOCK_MONOTONIC,&elem->timee);
  if(err==-1) ERROR("Time");
}

void printSeq(struct Elem * elem){
  while(elem!=NULL){
    ERROR("seqnum queue: %d\n", pkt_get_seqnum(elem->packet));
    elem=elem->next;
  }
}

void addQueue(struct Queue *queue, pkt_t* key){
  struct Elem* newelem = (struct Elem*)malloc(sizeof(struct Elem));
  newelem->packet = key;
  setTime(newelem);
  newelem->tpsmilli = 0;
  if (isEmpty(queue)){
    queue->front = newelem;
    queue->back = newelem;
    newelem->next = NULL;
    queue->size++;
  }
  else{
    queue->back->next = newelem;
    newelem->next = NULL;
    queue->back = newelem;
    queue->size++;
  }
}

pkt_t * deQueueReceiver(struct Queue * queue, int seq){
  if(isEmpty(queue)){
    return NULL;
  }
  struct Elem *current = queue->front;
  struct Elem *previous = NULL;
  pkt_t* pkt;
  if(pkt_get_seqnum(current->packet)==seq){
    pkt=queue->front->packet;
    if(queue->size==1){
      queue->front=NULL;
      queue->back=NULL;
      queue->size=0;
    }
    else{
      queue->front = queue->front->next;
      queue->size--;
    }
    free(current);
    return pkt;
  }
  while(current->next!=NULL){
    previous=current;
    current=current->next;
    if(pkt_get_seqnum(current->packet)==seq){
      pkt = current->packet;
      previous->next = current->next;
      if(current->next==NULL){
        queue->back = previous;
      }
      queue->size--;
      free(current);
      return pkt;
    }
  }
  return NULL;
}

pkt_t * searchPkt(struct Queue* queue, int seq){
  struct Elem *current = queue->front;
  while(current!=NULL){
    if(pkt_get_seqnum(current->packet)==seq){
      return current->packet;
    }
    current = current->next;
  }
  return NULL;
}

pkt_t* getPackett(struct Elem *element){
  return element->packet;
}

void addTime(struct Elem * element, int temps){
  element->tpsmilli = element->tpsmilli +temps;
}
int getTimee(struct Queue* queue, int seq){
  struct Elem *current = queue->front;
  while(current!=NULL){
    if(pkt_get_seqnum(current->packet)==seq){
      return current->tpsmilli;
    }
    current = current->next;
  }
  return -1;
}
