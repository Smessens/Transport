#include <stddef.h> /* size_t */
#include <stdint.h> /* uintx_t */
#include <stdio.h>  /* ssize_t */
#include <stdlib.h>
#include <zlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <math.h>

#include "packet_interface.h"
#include "pkt_queue_interface.h"

pkt_node_t * get_firstNode(pkt_queue_t * queue)
{
  return queue->firstNode;
}

uint16_t get_nextSeq(pkt_queue_t * queue)
{
  uint8_t seq = queue->nextSeq;
  return seq;
}

pkt_t * get_item(pkt_node_t* node)
{
  return node->item;
}

uint8_t get_nNodes(pkt_queue_t * queue)
{
  return queue->nNodes;
}

uint8_t get_window_size(pkt_queue_t * queue)
{
  return (MAX_WINDOW_SIZE-get_nNodes(queue));
}

uint8_t get_first_seqnum(pkt_queue_t * queue)
{
  if(queue->firstNode!=NULL)
  {
    return(pkt_get_seqnum(get_item(queue->firstNode)));
  }
  return -1;

}

pkt_t * pop(pkt_queue_t * queue)
{
  pkt_node_t * toDelete = get_firstNode(queue);
  pkt_t * rep = get_item(toDelete);
  queue->firstNode = queue->firstNode->next;
  free(toDelete);
  queue->nNodes--;
  return rep;
}

void add(pkt_queue_t * queue, pkt_t* pkt)
{
  uint16_t seq = pkt_get_seqnum(pkt);



  pkt_node_t* newNode = malloc(sizeof(pkt_node_t));
  newNode->item=pkt;

  if(queue->nNodes==0)
  {
    newNode->next = NULL;
    queue->firstNode = newNode;
    queue->nNodes++;
    return;
  }
  pkt_node_t* current = queue->firstNode;
  while(current->next!=NULL)
  {
    if(seq%256==pkt_get_seqnum(current->item))
    {
      printf("SEQ DEJA PRESENT : %d\n", pkt_get_seqnum(pkt));
      pkt_del(pkt);
      free(newNode);
      return;
    }
  current=current->next; 
  }
  current = queue->firstNode;
  if(seq<=MAX_WINDOW_SIZE && pkt_get_seqnum(current->item)>256-32){
    seq = seq +256;
  }
  if(seq<pkt_get_seqnum(current->item)||(seq-pkt_get_seqnum(current->item)>256-32))
  {
      newNode->next = current;
      queue->firstNode = newNode;
      queue->nNodes++;
      return;
  }

  while(current->next!=NULL)
  {
    if(seq<pkt_get_seqnum(current->next->item))
    {
      if(seq>pkt_get_seqnum(current->item))
      {
        newNode->next = current->next;
        current->next = newNode;
        queue->nNodes++;
        return; 
      }
    }
    if(seq>=pkt_get_seqnum(current->next->item)&&pkt_get_seqnum(current->next->item)<MAX_WINDOW_SIZE && seq>256-32){
      if(seq/256==1)
      {
        if((seq%256)<pkt_get_seqnum(current->next->item))
        {
          newNode->next = current->next;
          current->next = newNode;
          queue->nNodes++;
          return; 
        }

      }
      else if(seq>pkt_get_seqnum(current->item))
      {
        newNode->next = current->next;
        current->next = newNode;
        queue->nNodes++;
        return; 
      }
    }
    current=current->next;
  }
  current->next = newNode;
  newNode->next = NULL;
  queue->nNodes++;
}

void updateSeq (pkt_queue_t * queue)
{
  pkt_node_t* current = queue->firstNode;

  while(current->next!=NULL && pkt_get_seqnum(current->item)<queue->nextSeq)
  {
    current=current->next;
  }

  if(pkt_get_seqnum(current->item)==queue->nextSeq)
  {
    while(current->next!=NULL && pkt_get_seqnum(current->item)==queue->nextSeq)
    {
      current=current->next;
      queue->nextSeq ++;
      queue->nextSeq = (queue->nextSeq)%256;
    }
    if(current->next!=NULL){
      printf("NOUVEAU FIRST SEQ : %d \n",pkt_get_seqnum(current->item));
    }
    if(current->next==NULL && pkt_get_seqnum(current->item)==queue->nextSeq)
    {
      queue->nextSeq ++;
      queue->nextSeq = (queue->nextSeq)%256;
    }
  }
}

void printQueue(pkt_queue_t *queue)
{
  printf(" -------------------- Debut queue ---------------------------\n");
  printf("Nombre de pkt stockés : %d\n", queue->nNodes);
  printf("nextSeq : %d\n",queue->nextSeq);
  pkt_node_t *current = queue->firstNode;

  while(current!=NULL)
  {
    printPkt(current->item);
    current=current->next;
  }

  
}
