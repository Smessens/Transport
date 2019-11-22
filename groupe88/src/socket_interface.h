#ifndef SOCKET_INTERFACE_H
#define SOCKET_INTERFACE_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "packet_interface.h"
#include "pkt_queue_interface.h"

/* Taille maximale permise pour un packet */
#define MAX_PACKET_SIZE 528

/* Numéro de séquence maximal permis pour les packets */
#define MAX_SEQNUM 255

/************************************** Structures **************************************/

/* Définition de struct source */
typedef struct source
{
  pkt_queue_t * stock;                /* liste chaînée de paquets */
  struct sockaddr_in6 source_addr;    /* adresse de la source */
  int fd;                             /* file descriptor du fichier de sortie lié à la source */
}source_t;

/* Définition de struct src_node de la src_queue */
typedef struct src_node
{
  source_t * item;                    /* item du noeud de la liste */
  struct src_node * next;             /* noeud suivant dans la liste */
}src_node_t;

/* Définition de struct src_queue */
typedef struct src_queue
{
  int nSrcMax;                        /* nombre de sources max */
  int nSrc;                           /* nombres de noeuds dans la liste */
  int i;                              /* indique le numéro du fichier à créer */
  src_node_t * firstNode;             /* premier noeud de la liste */
  src_node_t * lastNode;              /* dernier noeud de la liste */
}src_queue_t;

/************************** Fonctions relatives à la src_queue **************************/

/*
 * Ajoute une nouvelle source à la fin de la liste chainée de sources avec son adresse,
 * le file descriptor du fichier dans lequel le receiver écrit les packets reçus et la
 * liste chainée de packets reçus mais pas encore écrits dans le fichier de sortie.
 *
 * @from : struct de l'adresse de la source
 * @src : liste des sources en train d'échanger avec le receiver
 * @format : string du format avec lequel créé le fichier de sortie
 *
 * @post : si nSrc<nSrcMax alors nSrc = nSrc+ 1 et i = i + 1
 *
 * @return : pointeur vers la structure source ajoutée à la liste ou NULL s'il y a trop
 * de sources communiquant simultanément avec le receiver
 */
source_t * src_add(struct sockaddr_in6 from, src_queue_t * src, char * format);

/*
 * Supprime une source de la liste chainée de sources et libère la mémoire allouée
 * pour l'échange d'information avec cette source
 *
 * @src : liste des sources en train d'échanger avec le receiver
 * @source : pointeur vers la structure source à supprimer
 *
 * @ nSrc = nSrc - 1
 */
void src_delete(src_queue_t * src, source_t * source);

/*
 * Récupère la structure source associée à l'adresse from dans la liste de sources
 * enregistrées si elle en fait déjà partie
 *
 * @from : struct de l'adresse de la source
 * @src : liste des sources en train d'échanger avec le receiver
 *
 * @return : pointeur vers la structure source correespondante ou NULL si aucune
 * source ne possède cette adresse
 */
source_t * get_source(struct sockaddr_in6 from, src_queue_t * src);

/*************************** Fonctions relatives aux sources ***************************/

/*
 * Crée une nouvelle source en lui associant son adresse, le file descriptor du fichier
 * dans lequel le receiver écrit les packets reçus et en initialisant la liste chainée
 * pour stocker les packets reçus mais pas encore écrits dans le fichier de sortie.
 *
 * @source_addr : structure de l'adresse de la source
 * @fd : file descriptor du fichier créé pour recopier le fichier reçu
 *
 * @return : pointeur vers la structure source initialisée
 *
 * @post : get_nNodes(source->stock) = 0
 *         get_firstNode(firstNode) = NULL
 *         get_nextSeq(source->stock) = 0
 *         source->fd = fd
 *         get_addr(source) = source_addr
 */
source_t * source_new(struct sockaddr_in6 source_addr,int fd);

/*
 * Renvoie l'adresse associée à une source
 *
 * @source : pointeur vers la structure source dont il faut récupérer l'adresse
 */
struct sockaddr_in6 get_addr(source_t* source);

/*
 * Traite un packet reçu en l'ajoutant à la liste chainée des packets à stocker avant
 * d'être écrits dans le fichier de sortie s'il n'est pas tronqué et qu'il est dans
 * la fenêtre de réception du receiver et renvoie le packet ack ou nack à renvoyer
 * au sender selon le protocol de selective repeat
 *
 * @pre : pkt_get_type(pkt) = PTYPE_DATA
 *
 * @pkt : pointeur vers le packet reçu depuis l'adresse associé à la structure source
 * @source : pointeur vers la structure source depuis laquelle le packet a été reçu
 * et dans laquelle il faut ajouter le packet en mettant à jour le prochain numéro de
 * séquence attendu si nécessaire
 *
 * @return : pointeur vers une structure pkt_t de type PTYPE_ACK ou PTYPE_NACK à
 * envoyer au sender qui a envoyé le pkt reçu
 */
pkt_t * pkt_treat(pkt_t * pkt, source_t * source);

/*
 * Indique si le numéro de séquence d'un packet reçu est dans la fenêtre de réception
 * associée à la source dont il provient
 *
 * @pre : pkt_get_type(pkt) = PTYPE_DATA
 *        pkt_get_tr(pkt) = 0
 *
 * @pkt : pointeur vers le packet reçu depuis l'adresse associé à la structure source
 * @source : pointeur vers la structure source depuis laquelle le packet a été reçu
 *
 * @return : un entier positif si le packet peut être accepté et -1 s'il est en
 * dehors de la fenêtre de réception
 */
int seqnumOK(pkt_t * pkt, source_t * source);

/*
 * Indique si le data du premier packet de la liste chainée stock d'une source peut déjà
 * être écrit dans le fichier de sortie, c'est-à-dire si tous les packets avec celui-ci
 * ont déjà été reçus et écrits dans le fichier
 *
 * @source : pointeur vers une structure source
 *
 * @return : 0 si le premier packet du stock peut être retiré de la liste et écrit dans le
 * fichier de sortie et -1 sinon
 */
uint8_t canWrite(source_t * source);

/*
 * Libère en mémoire la liste chaînée de sources, le paquet reçu et le paquet envoyé
 *
 * @src : liste chaînée de sources
 * @pkt1 : paquet reçu ou paquet envoyé
 * @pkt2 : paquet envoyé ou paquet reçu
 */
void delete_all(src_queue_t * src, pkt_t * pkt1, pkt_t * pkt2);

/******************************** Fonctions du receiver ********************************/

/*
 * Crée un nouveau socket en associant directement le hostname sous forme d'un tableau
 * de charactères à un adresse IPv6 et lie cette adresse au socket précédemment créé.
 * Le port est également précisé.
 *
 * @hostname : adresse sous forme d'une liste de charactères
 * @port : port sous forme d'une liste de charactères
 *
 * @return : le file descriptor du socket ou -1 en cas d'erreur
 */
int create_socket_real_adress(char* hostname, char* port);

/*
 * Cette routine est une boucle infinie toujours prête à écouter / traiter des paquets
 * de la part des sources qui ont été définies en argument du programme (ou de la part
 * de toutes les sources si ça a été précisé en argument par :: ) et écrit ces paquets
 * dans un fichier par source. Le nombre de sources simultanées est limité à N.
 *
 * @sfd : le file descriptor du socket
 * @N : le nombre de sources maximales
 * @format : le format du fichier de sortie
 */
void read_write_loop(int sfd, int N, char* format);

#endif
