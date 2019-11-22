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

#include "socket_interface.h"
#include "pkt_queue_interface.h"

struct sockaddr_in6 get_addr(source_t* source)
{
  return source->source_addr;
}

void printSource(source_t * source)
{
  printf("---------- STOCK ----------- \n");
  printQueue(source->stock);
  printf("----- FILE DESCRIPTOR ------ \n");
  printf("%d\n", source->fd);
  printf("--------- ADDRESS ---------- \n");
  printf("%s \n", source->source_addr.sin6_addr.s6_addr);

}

source_t * source_new(struct sockaddr_in6 source_addr, int fd)
{
  source_t* new = (source_t *)malloc(sizeof(source_t));
  if(new==NULL)
  {
    perror("Failed to malloc new source_t");
    return NULL;
  }
  new->stock = (pkt_queue_t *)malloc(sizeof(pkt_queue_t));
  if(new->stock == NULL)
  {
    perror("Failed to malloc new queue_t");
    free(new);
    return NULL;
  }
  new->stock->nNodes = 0;
  new->stock->firstNode = NULL;
  new->stock->nextSeq = 0;
  new->source_addr = source_addr;
  new->fd = fd;
  return new;
}

pkt_t * pkt_ack(source_t * source)
{
  pkt_t * rep = pkt_new();
  if(rep==NULL)
  {
    perror("Failed to create a new packet");
    return(NULL);
  }
  pkt_set_type(rep, PTYPE_ACK);
  pkt_set_timestamp(rep, source->stock->lastTS);
  pkt_set_window(rep, get_window_size(source->stock));
  pkt_set_seqnum(rep, source->stock->nextSeq);

  return(rep);
}

pkt_t * pkt_treat(pkt_t * pkt, source_t * source)
{
  int dif = seqnumOK(pkt, source);
  pkt_queue_t * stock = source->stock;
  stock->lastTS = pkt_get_timestamp(pkt);
  pkt_t * rep;

  if(pkt_get_tr(pkt)==1)
  {
    if(dif !=-1)
    {
      rep = pkt_new();
      if(rep==NULL)
      {
        perror("Failed to create a new packet");
        return(NULL);
      }

      pkt_set_type(rep, PTYPE_NACK);
      pkt_set_seqnum(rep,pkt_get_seqnum(pkt));
      pkt_set_window(rep, get_window_size(stock));
      pkt_set_timestamp(rep, pkt_get_timestamp(pkt));
    }
    else
    {
      rep = pkt_ack(source);
    }
    pkt_del(pkt);
    return rep;

  }
  else
  {
    if(dif !=-1)
    {
      printf("ADD SEQ : %d\n", pkt_get_seqnum(pkt));
      add(stock,pkt);
      if(dif==0)
      {
        updateSeq(stock);
      }
    }
    else
    {
      //printf("Pkt ignoré : seqnum hors window \n");
      pkt_del(pkt);
    }
    rep = pkt_ack(source);
    return rep;
  }
}

int seqnumOK(pkt_t * pkt, source_t * source)
{
  pkt_queue_t * stock = source->stock;
  int dif = (int)pkt_get_seqnum(pkt) - (int)get_nextSeq(stock);
  if(dif < get_window_size(stock) && dif >= 0 )
  {
    return dif;
  }
  else if(dif+MAX_SEQNUM< get_window_size(stock))
  {
    return dif+MAX_SEQNUM;
  }
  return(-1);
}

int create_socket_real_adress(char* hostname, char* port)
{
  int sock = 0;
  struct addrinfo hints;
  struct addrinfo* res;

  memset(&hints,0,sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype  = SOCK_DGRAM;
  hints.ai_flags     = AI_PASSIVE;

  if(strcmp(hostname,"::")==0)
  {
    hostname=NULL;
  }

  if(getaddrinfo(hostname, port, &hints, &res)!=0)
  {
    perror("getaddrinfo failed");
    return(-1);
  }

  while(1)
  {
    if(res==NULL)
    {
      perror("didn't find a valid socket");
      return(-1);
    }
    if((sock = socket(res->ai_family,res->ai_socktype,res->ai_protocol))!=-1)
    {
      break;
    }
    perror("socket failed");
    res=res->ai_next;
  }

  if(bind(sock, res->ai_addr, res->ai_addrlen)==-1)
  {
    perror("bind failed");
    return(-1);
  }

  freeaddrinfo(res);
  return(sock);
}

void delete_all(src_queue_t * src, pkt_t * pkt1, pkt_t * pkt2)
{
  if(pkt1!=NULL)
  {
    pkt_del(pkt1);
  }

  if(pkt2!=NULL)
  {
    pkt_del(pkt2);
  }

  if(src==NULL)
  {
    return;
  }
  src_node_t * current = src->firstNode;
  while(src->firstNode!=NULL){
    src_delete(src, current->item);
  }
  free(src);

}

void read_write_loop(int sfd, int N, char* format)
{
  struct sockaddr_in6 from;
  socklen_t fromsize = sizeof(struct sockaddr_in6);

  char bufRead[MAX_PACKET_SIZE];
  char bufWrite[12];

  struct timeval timeout;
  timeout.tv_sec = 4;
  timeout.tv_usec = 0;

  fd_set set;
  fd_set file;

  int sel = 0;
  ssize_t numBytes = 0;
  ssize_t send;
  size_t bufWrite_len = 12;

  pkt_t * pkt_recv = NULL;
  pkt_t * pkt_sendBack = NULL;

  pkt_status_code dec;
  pkt_status_code enc;

  source_t * source;

  src_queue_t srcStruct = {.nSrcMax=N, .nSrc=0, .i=0, .firstNode=NULL};
  src_queue_t * src = &srcStruct;

  while(1)
  {
    FD_ZERO(&set);
    FD_SET(sfd,&set);

    FD_ZERO(&file);
    int j;
    src_node_t* current = src->firstNode;
    while(current!=NULL)
    {
      FD_SET(current->item->fd,&file);
      current=current->next;
    }

    if((sel = select(FD_SETSIZE, &set, &file, NULL, &timeout))!=-1)
    {
      if(FD_ISSET(sfd,&set))
      {
        numBytes = recvfrom(sfd, bufRead, MAX_PACKET_SIZE, 0,
          (struct sockaddr *)&from, &fromsize);
        if(numBytes<0)
        {
          perror("Failed to receive data");
          delete_all(src, pkt_recv, pkt_sendBack);
          return;
        }

        pkt_recv = pkt_new();
        if(pkt_recv==NULL)
        {
          perror("Failed to create new pkt");
          return;
        }
        dec = pkt_decode(bufRead, numBytes, pkt_recv);

        memset(bufRead,0,MAX_PACKET_SIZE);

        source = get_source(from, src);

        if(source == NULL && dec != 0)                                    /* Nouvelle source avec un paquet corrompu */
        {
          perror("Nouvelle src et 1er pkt corrompu");                     /* Source et paquet ignorés */
        }
        else                                                              /* Source trouvee ou paquet valide */
        {
          if(source == NULL && pkt_get_seqnum(pkt_recv)==0)
          {
            source = src_add(from, src, format);
            if(source == NULL)
            {
              perror("Too many sources");
            }
          }
          if(!(source == NULL && pkt_get_length(pkt_recv)!=0))
          {
            if(source==NULL)
            {
              // renvoyer le ack précédemment perdu
              pkt_sendBack = pkt_new();
              pkt_set_type(pkt_sendBack, PTYPE_ACK);
              pkt_set_seqnum(pkt_sendBack, (pkt_get_seqnum(pkt_recv)+1)%256);
              pkt_set_timestamp(pkt_sendBack, pkt_get_timestamp(pkt_recv));
              pkt_set_window(pkt_sendBack, pkt_get_window(pkt_recv));
              pkt_del(pkt_recv);
            }
            else if(dec!=0)
            {
              perror("Failed to decode the received packet");
              printStatus(dec);
              pkt_del(pkt_recv);
              pkt_sendBack = pkt_ack(source);
            }
            else                                                          /* Data décodé */
            {
              if(pkt_get_type(pkt_recv)==PTYPE_DATA)
              {
                if((pkt_sendBack = pkt_treat(pkt_recv, source))==NULL)
                {
                  return;
                }
              }
              else /* Packet autre type que data reçu */
              {
                pkt_sendBack = pkt_ack(source);
                perror("Pkt ignoré : autre que PTYPE_DATA\n");
              }
            }
            enc = pkt_encode(pkt_sendBack, bufWrite, &bufWrite_len);
            if((enc)!=0)
            {
              perror("Failed to encode the packet to send back");
              printStatus(enc);
              delete_all(src, pkt_recv, pkt_sendBack);
              return;
            }
            if((send = sendto(sfd, bufWrite, bufWrite_len, 0, (const struct sockaddr *)&from, fromsize))==-1||send<bufWrite_len)
            {
              perror("Failed to write on the socket");
              delete_all(src, pkt_recv, pkt_sendBack);
              return;
            }
            pkt_del(pkt_sendBack);
            bufWrite_len = 12;

            memset(bufWrite,0,bufWrite_len);
          }
        }
      }
      current = src->firstNode;
      while(current!=NULL)
      {
        int fd = current->item->fd;
        if(FD_ISSET(fd,&file))
        {
          pkt_queue_t * stock = current->item->stock;
          int fd = current->item->fd;
          
          while( canWrite(source)==0 && stock->firstNode!=NULL)
          {
            printf("NEXT SEQ : %d\n",get_nextSeq(source->stock));
            pkt_t * toWrite = pop(stock);
             printf("SEQ : %d\n",pkt_get_seqnum(toWrite));
            if(write(fd, pkt_get_payload(toWrite), pkt_get_length(toWrite))==-1)
            {
              perror("Failed to write in the file");
              delete_all(src, pkt_recv, pkt_sendBack);
              pkt_del(toWrite);
              return;
            }
            if(pkt_get_length(toWrite)==0)
            {
              perror("Fin du fichier");
              FD_CLR(fd,&file);
              src_delete(src, current->item);
            }
            pkt_del(toWrite);
            break;
          }
        }
        current=current->next;
      }
    }
    else
    {
      perror("Select -1\n");
      delete_all(src, pkt_recv, pkt_sendBack);
    }
  }
}

uint8_t canWrite(source_t * source)
{
  pkt_queue_t * stock = source->stock;

  if(stock->firstNode==NULL)
  {
    return -1;
  }
  if(get_first_seqnum(stock)==-1)
  {
    return -1;
  }

  int dif =  (int)get_nextSeq(stock) - (int)get_first_seqnum(stock);


  if(dif < 32 && dif >= 0 )
  {
    return 0;
  }
  else if(dif+MAX_SEQNUM< 32)
  {
    return 0;
  }
  // if(get_first_seqnum(stock)<get_nextSeq(stock) && get_nextSeq(stock)-get_first_seqnum(stock)>=0)
  // {
  //   return 0;
  // }
  // else if(get_first_seqnum(stock)-get_nextSeq(stock)+256<32)
  // {
  //   return 0;
  // }
  return -1;
}

source_t * src_add(struct sockaddr_in6 from, src_queue_t * src, char * format)
{
  source_t * source;
  if(src->nSrc==src->nSrcMax)
  {
    return NULL;
  }

  char filename[sizeof(format)];
  sprintf(filename, format, src->i);
  int fd = open(filename,O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
  if(fd==-1)
  {
    perror("Failed to open file");
    return NULL;
  }

  if((source = source_new(from, fd))==NULL)
  {
    perror("Failed to create new src");
    return NULL;
  }
  src_node_t* newNode = (src_node_t*)malloc(sizeof(src_node_t));
  newNode->item = source;
  newNode->next = NULL;

  if(src->nSrc==0)
  {
    src->firstNode=newNode;
  }
  else
  {
    src->lastNode->next = newNode;
  }

  src->lastNode = newNode;
  src->nSrc++;
  src->i++;

  return source;
}

source_t * get_source(struct sockaddr_in6 from, src_queue_t * src)   //-> receiver
{
  src_node_t* current = src->firstNode;
  struct sockaddr_in6 ad = get_addr(current->item);
  while(current!=NULL)
  {
    if(strcmp(ad.sin6_addr.s6_addr,from.sin6_addr.s6_addr)==0)
    {
      return(current->item);
    }
    current=current->next;
  }
  return(NULL);
}


void src_delete(src_queue_t * src, source_t * source)
{
    src_node_t* current = src->firstNode;

    if(current->item==source)
    {
      src->firstNode = current->next;
      free(current->item->stock);
      free(current->item);
      free(current);
      src->nSrc--;
      return;
    }

    while(current->next->item!=source)
    {
      current=current->next;
    }

    if(current->next==src->lastNode)
    {
      src->lastNode = current;
      free(current->item->stock);
      free(current->next->item);
      free(current->next);
      src->nSrc--;
    }
    else
    {
      src_node_t* save = current->next;
      current->next = current->next->next;
      free(save->item->stock);
      free(save->item);
      free(save);
      src->nSrc--;
    }
}
