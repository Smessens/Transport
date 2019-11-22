#ifndef PKT_QUEUE_INTERFACE_H
#define PKT_QUEUE_INTERFACE_H

/* Taille maximale de Window */
#define MAX_WINDOW_SIZE 31

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

/* Définition de struct pkt_node de struct pkt_queue */
typedef struct pkt_node
{
  pkt_t * item;                     /* item du noeud de la liste */    
  struct pkt_node * next;           /* noeud suivant dans la liste */
}pkt_node_t;

/* Définition de struct pkt_queue */
typedef struct pkt_queue
{
  struct pkt_node *firstNode;       /* premier noeud de la liste */
  uint8_t nNodes;                   /* nombre de noeuds dans la liste */
  uint8_t nextSeq;                  /* prochain seqnum attendu */
  uint32_t lastTS;                  /* dernier timestamp reçu */
}pkt_queue_t;

/********************************* Accesseurs *********************************/

/*
 * Accesseur pour l'item d'un noeud
 */
pkt_t * get_item(pkt_node_t * node);

/*
 * Accesseurs pour les éléments d'une liste chaînée 
 */
uint8_t get_window_size(pkt_queue_t * queue);
uint8_t get_first_seqnum(pkt_queue_t * queue);
pkt_node_t * get_firstNode(pkt_queue_t * queue);
uint16_t get_nextSeq(pkt_queue_t * queue);
uint8_t get_nNodes(pkt_queue_t * queue);

/*
 * Retire le premier noeud de la liste chaînée
 *
 * @queue : la liste chaînée
 *
 * @pre  : la liste n'est pas vide
 * @post : le noeud est retiré de la liste chaînée
 *
 * @return : l'item du premier noeud retiré de la liste, à savoir un paquet (un pointeur vers un paquet)
 */
pkt_t * pop(pkt_queue_t * queue);

/*
 * Ajoute un noeud contenant un paquet dans la liste
 *
 * @queue : la liste chaînée
 * @pkt : un pointeur vers le paquet à ajouter
 *
 * @post : le prochain seqnum attendu n'est pas encore mis à jour
 */
void add(pkt_queue_t * queue, pkt_t * pkt);

/*
 * Met à jour le prochain seqnum attendu
 *
 * @queue : la liste chaînée
 *
 * @post : le prochain seqnum attendu (nextSeq) est mis à jour dans la liste chaînée
 *
 */
void updateSeq(pkt_queue_t * queue);

/*
 * Imprimeurs pour debugger
 */
void printQueue(pkt_queue_t * queue);

#endif /* PKT_QUEUE_INTERFACE_H */
