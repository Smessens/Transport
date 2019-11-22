#include "buffer.h"
#include "packet_interface.h"

#include <fcntl.h>
#include <errno.h>

bil_t *bufferlist;
fdil_t *fdlist;
int senderlimit;
int filecount = 0;
int mainsocket;
char *format;
int last_timestamp=0;
int last_recup_seq=-1;
/* Structure used to represent an item of the buffer */
struct bufferItem {
  pkt_t *packet; // Decoded packet
  int fd; // Fd on which the data has to be written
  bi_t *next; // Pointer to next item
};

struct bufferItemList {
  bi_t *first; // Head of the list
  bi_t *last; // End of the list
  int size; // Size of the list
};

/* Structure used to store opened fd and adresses */
struct fdItem {
  struct sockaddr_in6 *sender; // Ipv6 address from sender
  int fd; // Filedescriptor of the file where data is written
  uint8_t lastseqnum; // Seqnum of the last packet received
  uint8_t lastack; // Last seqnum that we acked
  uint8_t writtenseqnum; // Seqnum of the last packet written on the file
  fdi_t *next; // Pointer to next item
};

struct fdItemList {
  fdi_t *first; // Head of the list
  fdi_t *last;
  int size; // Size of the list
};


/////////////////////////////////////////
// BUFFER_ITEM / LIST

/* Create new bufferItemList
Returns 0 on success, -1 otherwise*/
int init_bufferlist(){
  bufferlist = (bil_t *) malloc(sizeof(bil_t));
  if(bufferlist==NULL){
    perror("Malloc init_bufferlist");
    return -1;
  }
  bufferlist->first = NULL;
  bufferlist->last = NULL;
  bufferlist->size = 0;
  return 0;
}

/* Add decoded data in the buffer
Returns 0 on success, -1 otherwise*/
int add_packet(bi_t *bi, int previouspkt){
  if(is_buffer_empty()==1){ // List is empty
    bufferlist->first = bi;
    bufferlist->last = bufferlist->first;
    bufferlist->size++;
    return 0;
  }
  else if(previouspkt==0){ // Packet is inserted at the end
    if(pkt_get_seqnum((bufferlist->last)->packet)<pkt_get_seqnum(bi->packet)){
      bufferlist->last->next = bi;
      bufferlist->last = bi;
      bufferlist->size++;
    }
    return 0;
  }
  else{
    // If first->next in null
    if((bufferlist->first)->next==NULL){
      // if it has the same fd as pkt->fd
      if((bufferlist->first)->fd==bi->fd){
        // if pkt->seqnum > bi->seqnum  place it as the new first
        if(pkt_get_seqnum((bufferlist->first)->packet)>pkt_get_seqnum(bi->packet)){ // if pkt->seqnum >
          bi->next=bufferlist->first;
          bufferlist->first=bi;
          bufferlist->size++;
          return 0;
        }
      }
      // Fd is different or bi->seqnum > pkt->seqnum.
      // place it as first->next
      if((bufferlist->first)->fd!=bi->fd || pkt_get_seqnum((bufferlist->first)->packet)<pkt_get_seqnum(bi->packet)){
        (bufferlist->first)->next=bi;
        bufferlist->last=bi;
        bi->next=NULL;
        bufferlist->size++;
      }
      return 0;

    }
    else{
      // travel to find same fd
      bi_t *current = bufferlist->first;

      if(current->fd==bi->fd){

        
        if(pkt_get_seqnum(current->packet)>pkt_get_seqnum(bi->packet)){
          bi->next = current;
          bufferlist->size++;
          bufferlist->first=bi;
          return 0;
        }
      }
      while(current->next!=NULL){
        // If the same fd as bi
        if((current->next)->fd == bi->fd){
          // if bi is the same as current->next or the first element.
          if((pkt_get_seqnum(bi->packet)==pkt_get_seqnum((current->next)->packet))||(pkt_get_seqnum(bi->packet)==pkt_get_seqnum((bufferlist->first)->packet))){
            return 0;
          }
          // if bi has to be before current->next
          if(pkt_get_seqnum((current->next)->packet)>pkt_get_seqnum(bi->packet)){
            bi->next = current->next;
            current->next = bi;
            bufferlist->size++;
            return 0;
          }
        }
        // if current is the last element from its fd
        // check if current->fd==bi->fd
        // if true, bi is the biggest element of this fd.
        else if(current->fd==bi->fd && (pkt_get_seqnum(current->packet)!=pkt_get_seqnum(bi->packet))){
          bi->next=current->next;
          current->next=bi;
          bufferlist->size++;
          return 0;
        }
        current=current->next;
      }
      // current->next == NULL
      // this means that either current->fd == bi->fd && current->seqnum < bi->seqnum
      // or that it is a bi from a new fd.
      // in both cases we put it a the end of the list.
      if(current->fd!=bi->fd || pkt_get_seqnum(current->packet)<pkt_get_seqnum(bi->packet)){
        current->next=bi;
        bi->next=NULL;
        bufferlist->size++;
        bufferlist->last=bi;
      }
      return 0;
    }
  }
  return -1;
}

/* Removes a bi from the buffer
Returns 0 on success, -1 otherwise*/
int remove_packet(int index){
  if(bufferlist->size==1){ // Last packet from buffer
    pkt_del((bufferlist->first)->packet);
    free(bufferlist->first);
    bufferlist->size--;
    return 0;
  }
  if(index==0){ // Remove first packet
    bi_t *temp = bufferlist->first->next;
    pkt_del((bufferlist->first)->packet);
    free(bufferlist->first);
    bufferlist->first = temp;
    bufferlist->size--;
    return 0;
  }
  else { // Any other packet
    bi_t *current = bufferlist->first->next;
    bi_t *previous = bufferlist->first;
    int i=0;
    for(;i<index-1; i++){
      if(current!=NULL){
        previous = current;
        current = current->next;
      } else {
        return -1;
      }
    }
    previous->next = current->next;
    if(index==bufferlist->size-1){
      bufferlist->last = previous;
    }
    pkt_del(current->packet);
    free(current);
    bufferlist->size--;
    return 0;
  }
  return -1;
}

/* Creates a BufferItem */
bi_t* create_bi(pkt_t *pkt, int fd){
  bi_t *bi = (bi_t *) malloc(sizeof(bi_t));
  bi->packet=pkt;
  bi->fd=fd;
  return bi;
}

/* Returns bufferItemList current size */
int get_buffer_size(){
  return bufferlist->size;
}

/* Returns the first element of the bufferItemList*/
bi_t* get_buffer_first(){
  return bufferlist->first;
}

/* Returns the last element of the bufferItemList*/
bi_t* get_buffer_last(){
  return bufferlist->last;
}

/* Returns the packet of a bufferItem*/
pkt_t* get_bi_packet(bi_t *bi){
  return bi->packet;
}

/* Returns the next bufferItem of a bufferItem*/
bi_t* get_bi_next(bi_t *bi){
  return bi->next;
}

/* Returns the fd of a bufferItem*/
int get_bi_fd(bi_t *bi){
  return bi->fd;
}

/* Returns 1 if buffer of bufferItem is empty
Returns 0 otherwise*/
int is_buffer_empty(){
  if(bufferlist->size<=0){
    return 1;
  }
  return 0;
}

/* Free a bufferItemList */
void free_bufferlist(){
  bi_t *current = bufferlist->first;
  int i = bufferlist->size;
  for(;i>0;i--){
    bi_t *temp = current;
    current = current->next;
    pkt_del(temp->packet);
    free(temp);
  }
  free(bufferlist);
}



/////////////////////////////////////////
// FD_ITEM / LIST

/* Create new fdItemList
Returns 0 on success, -1 otherwise*/
int init_fdlist(){
  fdlist = (fdil_t *) malloc(sizeof(bil_t));
  if(fdlist==NULL){
    perror("Malloc init_fdlist");
    return -1;
  }
  fdlist->first = NULL;
  fdlist->last = NULL;
  fdlist->size = 0;
  return 0;
}

/* Returns first fdi from linked list */
int get_first_fd(){
  return bufferlist->first->fd;
}

/* Returns fdi containing fd */
fdi_t *get_fdi(int fd){
  fdi_t *current = fdlist->first;
  int i = fdlist->size;
  for(;i>0;i--){
    if(current->fd==fd){
      return current;
    }
    current = current->next;
  }
  return NULL;
}

/* Add fdi struct to the linked list
Returns 0 on success, -1 otherwise*/
int add_fd(struct sockaddr_in6 *addr){
  fdi_t *fdi = (fdi_t *) malloc(sizeof(fdi_t));
  if(fdi==NULL){
    perror("Malloc add_fd");
    return -1;
  }
  fdi->sender = (struct sockaddr_in6*) malloc(sizeof(struct sockaddr_in6));
  if(fdi->sender==NULL){
    perror("Malloc add_fd");
    return -1;
  }
  memcpy((void *) fdi->sender,(void *) addr, sizeof(struct sockaddr_in6));
  char *str;
  if(format!=NULL){
    str = (char *) malloc(strlen(format)+1);
    if(str==NULL){
      perror("Malloc add_fd");
      return -1;
    }
  }
  sprintf(str, format, filecount);
  filecount++;
  fdi->fd = open(str, O_CREAT|O_APPEND|O_WRONLY, S_IRWXU); // Create the file with unique name
  free(str);
  fdi->lastseqnum = (uint8_t) 255;
  fdi->lastack = (uint8_t) 255;
  fdi->writtenseqnum =  (uint8_t) 255;
  if(is_fd_empty()==1){ // If first item of the list
    fdlist->first = fdi;
    fdlist->last = fdi;
    fdlist->size++;
    return 0;
  }
  fdlist->last->next = fdi;
  fdlist->last = fdi;
  fdlist->size++;
  return 0;
}

/* Removes a fdi from the linked list
Returns 0 on success, -1 otherwise*/
int remove_fd(int fd){
  fdi_t *current = fdlist->first;
  fdi_t *previous = NULL;
  int i = fdlist->size;
  for(;i>0;i--){
    if(current->fd==fd){
      if(previous==NULL){ // If first item of the list
        fdlist->first = current->next;
        close(current->fd);
        free(current->sender);
        free(current);
        fdlist->size--;
        return 0;
      }
      previous->next = current->next;
      close(current->fd);
      free(current->sender);
      free(current);
      fdlist->size--;
      return 0;
    }
    previous = current;
    current = current->next;
  }
  return -1;
}

/* Returns 1 if linked list of fdi is empty
Returns 0 otherwise*/
int is_fd_empty(){
  if(fdlist->size<=0){
    return 1;
  }
  return 0;
}

/* Free a fdItemList */
void free_fdlist(){
  fdi_t *current = fdlist->first;
  int i = fdlist->size;
  for(;i>0;i--){
    fdi_t *temp = current;
    current = current->next;
    free(temp->sender);
    temp->sender=NULL;
    free(temp);
  }
  free(fdlist);
}




/////////////////////////////////////////
// RECEIVENG - SENDING - WRITING PACKETS

/* Returns the next seqnum to ack */
uint8_t ack_seq(int fd){
  bi_t *current = bufferlist->first;
  fdi_t *fdi = get_fdi(fd);
  int i = bufferlist->size;
  for(;i>0;i--){
    if(current->fd==fd){
      if(pkt_get_seqnum(current->packet)==fdi->lastack+1){
        fdi->lastack++;
      }
    }
    current = current->next;
  }
  return fdi->lastack; // Returns the highest ack possible
}

/* Sends a nack
Returns 0 on success, -1 otherwise*/
int send_nack(uint8_t seqnum, struct sockaddr_in6 *addr, int timestamp){
  pkt_t *pkt = pkt_new();
  char buf[11];
  size_t len = 11;
  memset(buf, 0, len);
  pkt_set_type(pkt, PTYPE_NACK);
  pkt_set_window(pkt, 31-(uint8_t)((bufferlist->size-1)/senderlimit));
  pkt_set_length(pkt, 0);
  pkt_set_timestamp(pkt, timestamp);
  pkt_set_seqnum(pkt, seqnum);
  int s;
  int err;
  if((s = pkt_encode(pkt, buf, &len))!=PKT_OK){
    perror("Error send_nack encode");
    pkt_del(pkt);
    return -1;
  }
  err = sendto(mainsocket,(void *) buf, len, 0, (struct sockaddr *) addr, (socklen_t) sizeof(struct sockaddr_in6));
  if(err==-1){
    perror("Error send_nack sendto");
    pkt_del(pkt);
    return -1;
  }
  pkt_del(pkt);
  return 0;
}

/* Sends an ack to the specified address
Returns 0 on success, -1 otherwise*/
int send_ack(int fd, uint8_t seqnum, struct sockaddr_in6 *addr, int timestamp, int resentlastack){
  fdi_t *fdi = get_fdi(fd);
  pkt_t *pkt = pkt_new();
  char buf[11];
  size_t len = 11;
  memset(buf, 0, len);
  pkt_set_type(pkt, PTYPE_ACK);
  pkt_set_window(pkt, 31-(uint8_t)((bufferlist->size-1)/senderlimit));
  pkt_set_length(pkt, 0);
  pkt_set_timestamp(pkt, timestamp);
  int s;
  int err;
  if(resentlastack==1){
    pkt_set_seqnum(pkt, fdi->lastack+1);
    pkt_set_timestamp(pkt, last_timestamp);
    if((s = pkt_encode(pkt, buf, &len))!=PKT_OK){
      perror("Error send_ack encode");
      pkt_del(pkt);
      return -1;
    }
    err = sendto(mainsocket,(void *) buf, len, 0, (struct sockaddr *) addr, (socklen_t) sizeof(struct sockaddr_in6)); // RESEND LAST ACK
    if(err==-1){
      perror("Error send_ack sendto");
      pkt_del(pkt);
      return -1;
    }
    pkt_del(pkt);
    return 0;
  }
  if(seqnum == (uint8_t) (fdi->lastack+1)){ // If its the next packet to ack
    pkt_set_seqnum(pkt, seqnum+1);
    if((s = pkt_encode(pkt, buf, &len))!=PKT_OK){
      perror("Error send_ack encode");
      pkt_del(pkt);
      return -1;
    }
    err = sendto(mainsocket,(void *) buf, len, 0, (struct sockaddr *) addr, (socklen_t) sizeof(struct sockaddr_in6)); // SEND ACK WITH SEQNUM + 1
    if(err==-1){
      perror("Error send_ack sendto");
      pkt_del(pkt);
      return -1;
    }
    pkt_del(pkt);
    fdi->lastack++;
    return 0;
  }
  else { // Else we have to check the buffer to ack all the packets we can
    uint8_t ack = ack_seq(fd);
    pkt_set_seqnum(pkt, ack+1);
    if((s = pkt_encode(pkt, buf, &len))!=PKT_OK){
      perror("Error send_ack encode");
      pkt_del(pkt);
      return -1;
    }
    err = sendto(mainsocket,(void *) buf, len, 0, (struct sockaddr *) addr, (socklen_t) sizeof(struct sockaddr_in6));// SEND ACK WITH ack_seq+1 RET
    if(err==-1){
      perror("Error send_ack sendto");
      pkt_del(pkt);
      return -1;
    }
    pkt_del(pkt);
    return 0;
  }
  pkt_del(pkt);
  return -1;
}

/* Writes packets from the buffer to a specific file
Returns 0 on success, -1 otherwise*/
int write_packet(int fd){
  int index = 0;
  bi_t *current = bufferlist->first;
  fdi_t *fdi = get_fdi(fd);
  int removed = 0;
  int err = 0;
  int i = bufferlist->size;
  for(;i>0;i--){
    if(current->fd==fd){ // We have to write content and remove from buffer
      if(fdi!=NULL){
        if((uint8_t)(pkt_get_seqnum(current->packet))==(uint8_t) (fdi->writtenseqnum+1)){
          size_t len = pkt_get_length(current->packet);
          err = write(current->fd, (void *) pkt_get_payload(current->packet), len);
          if(err==-1){
            printf("%s\n", "Error on write");
            return -1;
          }
          fdi->writtenseqnum++;
          removed = 1;
          if(pkt_get_length(current->packet)==0){
            remove_fd(current->fd);
            remove_packet(index);
            return 0;
          }
          current = current->next;
          remove_packet(index);
          index--;
        }
      }
    }
    if(removed==0){
      current = current->next;
    }
    removed = 0;
    index++;
  }
  return -1;
}

/* Receives a packet data and handles it
Returns 0 on success, -1 otherwise*/
int receive_packet(struct sockaddr_in6 *addr, char *data, size_t len){
  fdi_t *current;
  int i, s;
  pkt_t *pkt = pkt_new();
  int corrupted = 0;
  if((s = pkt_decode(data, len, pkt))!=PKT_OK){ // Packet is discarded
    if(s==E_CRC){
      corrupted = 1;

    }
  }
  if(pkt_get_tr(pkt)==1){ // Packet is truncated
    send_nack(pkt_get_seqnum(pkt), addr, pkt_get_timestamp(pkt));
    pkt_del(pkt);
    return 0;
  }
  bi_t *bi = (bi_t *) malloc(sizeof(bi_t));
  if(bi==NULL){
    pkt_del(pkt);
    free(bi);
    return -1;
  }
  bi->packet = pkt;
  bi->next = NULL;
  if(is_fd_empty()==0){
    current = fdlist->first;
  }
  for(i=0; i<fdlist->size; i++){
    if(memcmp((void *)addr->sin6_addr.s6_addr, (void *)current->sender->sin6_addr.s6_addr, 16)==0){
      if(corrupted==1){
        last_recup_seq=current->lastseqnum+1;
        send_ack(current->fd, current->lastseqnum+1, addr, pkt_get_timestamp(pkt), 1);
        pkt_del(pkt);
        free(bi);
        return 2;
      }
      bi->fd = current->fd;
      uint8_t seqnum = pkt_get_seqnum(pkt);
      last_timestamp=pkt_get_timestamp(pkt);
      if(seqnum == (uint8_t) 255){ // Reach max seqnum field we need to reset
        if((uint8_t)(current->lastack+1)==(uint8_t)255){ // We have everything we can reset
          current->lastseqnum = 255;
          add_packet(bi, 0);
          send_ack(current->fd, seqnum, addr, pkt_get_timestamp(pkt), 0);
          write_packet(current->fd);
          return 0;
        } else { // Need to write all packets from this sender as soon as possible to reset
          current->lastseqnum = 255;
          add_packet(bi, 0);
          send_ack(current->fd, current->lastack, addr, pkt_get_timestamp(pkt), 0);
          write_packet(current->fd);
          return 0;
        }
      }
      if(seqnum<current->lastseqnum){ // Packet was resent
        if(current->writtenseqnum==255 && current->lastseqnum==255 && seqnum==0){ // Special case when we reset seqnum
          current->lastseqnum = 0;
          add_packet(bi, 0);
          send_ack(current->fd, seqnum, addr, pkt_get_timestamp(pkt), 0);
          return 0;
        }
        else if(seqnum>current->writtenseqnum){ // Packet is not written we can add it
          add_packet(bi, 1);
          send_ack(current->fd, current->lastack, addr, pkt_get_timestamp(pkt), 0);
          return 0;
        }
        // Packet is already written so we discard it
        pkt_del(pkt);
        free(bi);
        return 2;
      } else if(seqnum==current->lastseqnum){ // Packet is already written or in the buffer so we discard it
        pkt_del(pkt);
        free(bi);
        return 2;
      } else { // Packet is received in sequence we can add it

        current->lastseqnum = seqnum;
        add_packet(bi, 0);
        send_ack(current->fd, seqnum, addr, pkt_get_timestamp(pkt), 0);
        return 0;
      }
    }
    current = current->next;
  }
  if(fdlist->size<=senderlimit && pkt_get_seqnum(pkt)==0){ // New sender we add him and his packet
    add_fd(addr);
    current = fdlist->first;
    bi->fd = current->fd;
    add_packet(bi,0);
    current->lastseqnum = 0;
    send_ack(fdlist->last->fd, pkt_get_seqnum(pkt), addr, pkt_get_timestamp(pkt), 0);
    return 1;
  }
  pkt_del(pkt);
  free(bi);
  return -1;
}

/* Sets the sender limit */
void set_senderlimit(int n){
  senderlimit = n;
}

/* Sets the listening socket */
void set_mainsocket(int n){
  mainsocket = n;
}

/* Sets the format for outpul files
Returns 0 on success, -1 otherwise*/
int set_format(char *str){
  format = (char *) malloc(strlen(str)+1);
  if(format==NULL){
    perror("Malloc format");
    return -1;
  }
  memcpy(format, str, strlen(str)+1);
  return 0;
}

/* Free format of output file */
void free_format(){
  free(format);
}



/////////////////////////////////////////
// Printing in stdout

void print_fdlist(){
  if(is_fd_empty()==1){
    printf("%s\n", "NULLFD");
    return;
  }
  fdi_t *current = fdlist->first;
  while(current!=NULL){
    printf("%d\n", current->fd);
    current = current->next;
  }
}

void print_bufferlist(){
  if(is_buffer_empty()==1){
    printf("%s\n", "NULLBUFFER");
    return;
  }
  bi_t *current = bufferlist->first;
  int index=0;
  while(current!=NULL){
    printf("%d:(%d;%d)\n", index,  pkt_get_seqnum(current->packet), (current->fd)); // print (index:(seqnum;fd))
    index++;
    current = current->next;
  }
  printf("\n");
}
