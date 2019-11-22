#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <zlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <unistd.h>
#include "../src/packet_interface.h"
#include "../src/buffer.h"


int senderlimit;
char *format;
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

int init_suite() {
  return 0;
}

int clean_suite() {
  return 0;
}
/* Writes packets from the buffer to a specific file
Returns 0 on success, -1 otherwise*/
int modified_write_packet(int fd){
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
         // size_t len = pkt_get_length(current->packet);
         // err = write(1, (void *) pkt_get_payload(current->packet), len);
         // printf("Writting...");
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


// MODIFIED VERSION OF RECEIVE_PACKET ONLY FOR TESTING PURPOSES
// Modified to be tested with sepcial return values (one for each case):
/* Receives a packet data and handles it
Returns 0 on success, -1 otherwise

Invalid packet: return 2
Truncated Pkt: return 3
Maximum seqnum field:  
          --We have all the precedent seqnums in order: return 4
            Otherwise return -4
Packet was resent: 
          --Special case we reset seqnum: return 5
          --Packet wasn't written yet: return 6
Packet was already in buffer, pkt discarded: return -6 
Normal packet in good sequence: return 7

New sender added: return 8
*/
int modified_receive_packet(struct sockaddr_in6 *addr, char *data, size_t len){
  fdi_t *current;
  int i, s;
  pkt_t *pkt = pkt_new();

  if((s = pkt_decode(data, len, pkt))!=PKT_OK){ // Packet is discarded
    pkt_del(pkt);
    return 2;
  }
  if(pkt_get_tr(pkt)==1){ // Packet is truncated
    //send_nack(pkt_get_seqnum(pkt), addr, pkt_get_timestamp(pkt));
    pkt_del(pkt);
    return 3;
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
      bi->fd = current->fd;
      uint8_t seqnum = pkt_get_seqnum(pkt);
      if(seqnum == (uint8_t) 255){ // Reach max seqnum field we need to reset
        if((uint8_t)(current->lastack+1)==(uint8_t)255){ // We have everything we can reset
          current->lastseqnum = seqnum;
          add_packet(bi, 0);
          //send_ack(current->fd, seqnum, addr, pkt_get_timestamp(pkt), 0);
          current->lastack++;
          modified_write_packet(current->fd);
          return 4;
        } else { // Need to write all packets from this sender as soon as possible to reset
          add_packet(bi, 0);
          //send_ack(current->fd, current->lastack, addr, pkt_get_timestamp(pkt), 0);
          current->lastack++;
          modified_write_packet(current->fd);
          free_bufferlist();
          init_bufferlist();
          return -4;
        }
      }
      if(seqnum<current->lastseqnum){
        // Packet was resent
        if(current->lastseqnum==255 && seqnum==0){ // Special case when we reset seqnum
          current->lastseqnum = seqnum;
          add_packet(bi, 0);
          //send_ack(current->fd, seqnum, addr, pkt_get_timestamp(pkt), 0);
          current->lastack++;
          return 5;
        }
        else if(seqnum>current->writtenseqnum){ // Packet is not written we can add it
          add_packet(bi, 1);
          //send_ack(current->fd, current->lastack, addr, pkt_get_timestamp(pkt), 0);
          current->lastack++;
          return 6;
        }
        // Packet is already written so we discard it
        pkt_del(pkt);
        free(bi);
        return -6;
      } else if(seqnum==current->lastseqnum){ 
       pkt_del(pkt);
       free(bi);
       return -6;
      } else { // Packet is received in sequence we can add it
        current->lastseqnum = seqnum;
        add_packet(bi, 0);
        //send_ack(current->fd, seqnum, addr, pkt_get_timestamp(pkt), 0);
        current->lastack++;
        return 7;
      }
    }
    current = current->next;
  }
  if(fdlist->size<=senderlimit && pkt_get_seqnum(pkt)==0){ // New sender we add him and his packet
    add_fd(addr);
    
    current = fdlist->first;
    
    current->fd=0;
    bi->fd = current->fd;
    add_packet(bi,0);
    current->lastseqnum = 0;
    //send_ack(fdlist->last->fd, pkt_get_seqnum(pkt), addr, pkt_get_timestamp(pkt), 0);
    current->lastack++;
    current->writtenseqnum=0;
    return 8;
  }
  pkt_del(pkt);
  free(bi);
  return -1;
}

// creates a normal pkt with pkt->seqnum = seqnum.
pkt_t* create_normal_pkt(int seqnum){
  pkt_t *pkt = pkt_new();
  pkt_set_type(pkt, 1);
  pkt_set_tr(pkt, 0);
  pkt_set_window(pkt, 12);
  pkt_set_length(pkt, 384);
  pkt_set_seqnum(pkt, seqnum);
  pkt_set_timestamp(pkt, 3528);
  pkt_set_payload(pkt, "abcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefgh", 384);
  return pkt;
}

// creates a normal & VALID packet with pkt->seqnum = seqnum.
pkt_t* create_receive_pkt(int seqnum){
  char* buf2 = "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
  size_t len2=360;
  pkt_t *pkt_test=pkt_new();
  pkt_set_type(pkt_test, 1);
  pkt_set_tr(pkt_test, 0);
  pkt_set_window(pkt_test, 0);
  pkt_set_length(pkt_test, (uint16_t)len2);
  pkt_set_seqnum(pkt_test,(uint8_t) seqnum);
  pkt_set_payload(pkt_test, buf2,(uint16_t) len2);
  return pkt_test;
}

// test bon fonctionnement de add_packet
void test_add_packet() {
  init_bufferlist();
  // Test if elements are added in right order for 1 same fd.
  add_packet(create_bi(create_normal_pkt(0), 0),1);
  CU_ASSERT_EQUAL(0, pkt_get_seqnum(get_bi_packet(get_buffer_first())));

  add_packet(create_bi(create_normal_pkt(1),0),1);
  CU_ASSERT_EQUAL(1, pkt_get_seqnum(get_bi_packet(get_bi_next(get_buffer_first()))));

  add_packet(create_bi(create_normal_pkt(3), 0),1);

  CU_ASSERT_EQUAL(3, pkt_get_seqnum(get_bi_packet(get_bi_next(get_bi_next(get_buffer_first())))));

  add_packet(create_bi(create_normal_pkt(2), 0),1);
  CU_ASSERT_EQUAL(2, pkt_get_seqnum(get_bi_packet(get_bi_next(get_bi_next(get_buffer_first())))));
  
  CU_ASSERT_EQUAL(3, pkt_get_seqnum(get_bi_packet(get_bi_next(get_bi_next(get_bi_next(get_buffer_first()))))));

  // Test if elements are added in right order for different fds.
  free_bufferlist();
  init_bufferlist();
  add_packet(create_bi(create_normal_pkt(0), 0),1);
  CU_ASSERT_EQUAL(0, pkt_get_seqnum(get_bi_packet(get_buffer_first())));

  add_packet(create_bi(create_normal_pkt(1), 0),1);
  CU_ASSERT_EQUAL(1, pkt_get_seqnum(get_bi_packet(get_bi_next(get_buffer_first()))));

  add_packet(create_bi(create_normal_pkt(0), 1),1);
  CU_ASSERT_EQUAL(0, pkt_get_seqnum(get_bi_packet(get_bi_next(get_bi_next(get_buffer_first())))));

  add_packet(create_bi(create_normal_pkt(2), 1),1);
  CU_ASSERT_EQUAL(2, pkt_get_seqnum(get_bi_packet(get_bi_next(get_bi_next(get_bi_next(get_buffer_first()))))));

  add_packet(create_bi(create_normal_pkt(3), 0),1);
  CU_ASSERT_EQUAL(3, pkt_get_seqnum(get_bi_packet(get_bi_next(get_bi_next(get_buffer_first())))));

  add_packet(create_bi(create_normal_pkt(1), 1),1);
  CU_ASSERT_EQUAL(1, pkt_get_seqnum(get_bi_packet(get_bi_next(get_bi_next(get_bi_next(get_bi_next(get_buffer_first())))))));

  add_packet(create_bi(create_normal_pkt(3), 1),1);
  CU_ASSERT_EQUAL(3, pkt_get_seqnum(get_bi_packet(get_bi_next(get_bi_next(get_bi_next(get_bi_next(get_bi_next(get_bi_next(get_buffer_first())))))))));
  CU_ASSERT_EQUAL(7, get_buffer_size());
}

void test_remove_packet() {

  init_bufferlist();
  add_packet(create_bi(create_normal_pkt(0), 0),1);
  add_packet(create_bi(create_normal_pkt(1), 0),1);
  add_packet(create_bi(create_normal_pkt(0), 1),1);
  add_packet(create_bi(create_normal_pkt(2), 1),1);
  add_packet(create_bi(create_normal_pkt(3), 0),1);
  add_packet(create_bi(create_normal_pkt(1), 1),1);
  add_packet(create_bi(create_normal_pkt(3), 1),1); 
  // HOW THE BUFFER SHOULD APPEAR (index:(seqnum;fd))
  // 0:(0;0)
  // 1:(1;0)
  // 2:(3;0)
  // 3:(0;1)
  // 4:(1;1)
  // 5:(2;1)
  // 6:(3;1)
  remove_packet(1);
  CU_ASSERT_EQUAL(3, pkt_get_seqnum(get_bi_packet(get_bi_next(get_buffer_first()))));
  remove_packet(0);
  CU_ASSERT_EQUAL(3, pkt_get_seqnum(get_bi_packet(get_buffer_first())));
  remove_packet(4);
  CU_ASSERT_EQUAL(2, pkt_get_seqnum(get_bi_packet(get_bi_next(get_bi_next(get_bi_next(get_buffer_first()))))));
}



/* THIS FUNCTION TESTS MOST OF OUR PROJECT AS IT
   USES THE MOST IMPORTANT FUNCTIONS USED IN THE PROJECT. 
   TESTING: 
   Valid sequence of packets.
   Reception of an invalid packet.
   Max seqnum normal case.
   Max seqnum & first new packet is an invalid packet.
   Packet redundancy.
   Reception of a skipped packet
   */
void test_modified_receive_packet() {

  init_fdlist();
  init_bufferlist();
  set_senderlimit(5);

  // Create fake senders
  struct sockaddr_in6 sender1;
  memset(&sender1, 0, sizeof(sender1));
  memcpy(sender1.sin6_addr.s6_addr, "a", 1);


  // Create fake packets
  size_t len2=360;
  size_t len3=(size_t)(unsigned)372;
  

  int ret_val_receive_packet;
  int i;

  /////////////////////////////////////
  // TESTING VALID SEQUENCE OF PACKETS
  for(i=0;i<5;i++){
    pkt_t* pkt=create_receive_pkt(i);
    char* buf3 = (char *)malloc(len2 +1);
    memset(buf3, 0, len2 +1);
    pkt_encode(pkt, buf3,&len3);
    ret_val_receive_packet=modified_receive_packet(&sender1, buf3, len3+4);
    pkt=NULL;
  }
  CU_ASSERT_EQUAL(7, ret_val_receive_packet); // Normal packet sequence
  

  ///////////////////////////////////////////////////////
  // TESTING AN INVALID PACKET AND TESTING SIZE OF BUFFER
  size_t len4=52;
  pkt_t* pkt=create_receive_pkt(i);
  char* buf3 = (char *)malloc(len2 +1);
  memset(buf3, 0, len2 +1);
  pkt_encode(pkt, buf3,&len4);
  int m5=modified_receive_packet(&sender1, buf3, len4+4);
  CU_ASSERT_EQUAL(2, m5); // invalid packet sequence
  CU_ASSERT_EQUAL(5, get_buffer_size()); // buffer size should be unchanged


  free_bufferlist();
  init_bufferlist();
  free_fdlist();
  init_fdlist();
  

  //////////////////////////////////////////////////////////////////////////
  // TESTING MAX SEQNUM (Checks if buffer and seqnum is reseted accordingly)
  for(i=0;i<256;i++){
    pkt_t* pkt=create_receive_pkt(i);
    char* buf3 = (char *)malloc(len2 +1);
    memset(buf3, 0, len2 +1);
    pkt_encode(pkt, buf3,&len3);
    ret_val_receive_packet=modified_receive_packet(&sender1, buf3, len3+4);
    pkt=NULL;
  }
  CU_ASSERT_EQUAL(4, ret_val_receive_packet); //(Checks if buffer and seqnum is reseted accordingly)

  // reinitializing buffers
  free_bufferlist();
  init_bufferlist();
  free_fdlist();
  init_fdlist();
  

  /////////////////////////////////////////////////////////////////////////////////
  // TESTING MAX SEQNUM with redundant first new value (packet should be discarded)
  for(i=0;i<257;i++){
    pkt_t* pkt;
    if(i==256){
      pkt=create_receive_pkt(i-1);
    }
    else{
      pkt=create_receive_pkt(i);
    }
    char* buf3 = (char *)malloc(len2 +1);
    memset(buf3, 0, len2 +1);
    pkt_encode(pkt, buf3,&len3);
    ret_val_receive_packet=modified_receive_packet(&sender1, buf3, len3+4);
    pkt=NULL;
  }
  CU_ASSERT_EQUAL(-4, ret_val_receive_packet); //(MAX SEQNUM ACHIEVED, new packet with seq=0 was already sent before ==> PACKET DISCARDED.)

  // reinitializing buffers
  free_bufferlist();
  init_bufferlist();
  free_fdlist();
  init_fdlist();
  

  ////////////////////////////////////////////////
  // TESTING REDUNDANCY (PACKET ALREADY IN BUFFER)
  for(i=0;i<5;i++){
    pkt_t* pkt;
    if(i==4){
      pkt=create_receive_pkt(i-1);
    }
    else{
      pkt=create_receive_pkt(i);
    }
    char* buf3 = (char *)malloc(len2 +1);
    memset(buf3, 0, len2 +1);
    pkt_encode(pkt, buf3,&len3);
    ret_val_receive_packet=modified_receive_packet(&sender1, buf3, len3+4);
    pkt=NULL;
  }
  CU_ASSERT_EQUAL(-6, ret_val_receive_packet); //(PACKET ALREADY IN BUFFER --> DISCARDED)

  // reinitializing buffers
  free_bufferlist();
  init_bufferlist();
  free_fdlist();
  init_fdlist();
  

  /////////////////////////////////////
  // TESTING RECEIVED A SKIPPED PACKET
  for(i=0;i<6;i++){
    pkt_t* pkt;
    if(i==4){
      pkt=create_receive_pkt(5);
    }
    else if(i==5){
      pkt=create_receive_pkt(4);
    }
    else{
      pkt=create_receive_pkt(i);
    }
    char* buf3 = (char *)malloc(len2 +1);
    memset(buf3, 0, len2 +1);
    pkt_encode(pkt, buf3,&len3);
    ret_val_receive_packet=modified_receive_packet(&sender1, buf3, len3+4);
    pkt=NULL;
  }
  CU_ASSERT_EQUAL(6, ret_val_receive_packet); //(A MISSED PACKET IS RESENT)

 free_bufferlist();
 free_fdlist();
}

int main()
{
  //initalisation de la stack
  //head=(struct node**)malloc(sizeof(struct node**));

  //initialisation registre de tests
  if (CUE_SUCCESS != CU_initialize_registry())
    return CU_get_error();

  //ajout de la suite au registre
  CU_pSuite pSuite1 = NULL;
  pSuite1 = CU_add_suite("Tests_Stack", init_suite, clean_suite);
  if (NULL == pSuite1) {
    CU_cleanup_registry();
    return CU_get_error();
  }

  if((NULL == CU_add_test(pSuite1, "fonction add_packet()", test_add_packet))||(NULL == CU_add_test(pSuite1, "fonction remove_packet()", test_remove_packet))
    ||(NULL == CU_add_test(pSuite1, "fonction modified_receive_packet()", test_modified_receive_packet))){
    CU_cleanup_registry();
  return CU_get_error();
}
  //on lance les tests et on affiche les resultats
CU_basic_run_tests();
  //on libere les ressources
CU_cleanup_registry();
return CU_get_error();
}