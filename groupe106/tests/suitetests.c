#include "../src/packet_interface.h"
#include "../src/queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <netinet/in.h> /* * sockaddr_in6 */
#include <sys/types.h> /* sockaddr_in6 */
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdbool.h>

#include "../src/log.h"

void testQueue(){
  int seq = 0;
  int window = 5;
  int size = 512;
  pkt_t* pkt = pkt_new();
  if(pkt_set_type(pkt,1)!=PKT_OK) ERROR("set pkt type");
  if(pkt_set_tr(pkt,0)!=PKT_OK) ERROR("set pkt tr");
  if(pkt_set_window(pkt,window)!=PKT_OK) ERROR("set pkt window");
  if(pkt_set_length(pkt,size)!=PKT_OK) ERROR("set length");
  if(pkt_set_seqnum(pkt,seq)!=PKT_OK) ERROR("set seqnum");
  if(pkt_set_timestamp(pkt,42)!=PKT_OK) ERROR("set timestamp");
  char * buf = "Premier";
  if(pkt_set_payload(pkt,buf,strlen(buf))!=PKT_OK) ERROR("set payload");

  seq++;

  pkt_t *a = pkt_new();
  if(pkt_set_type(a,1)!=PKT_OK) ERROR("set pkt type");
  if(pkt_set_tr(a,0)!=PKT_OK) ERROR("set pkt tr");
  if(pkt_set_window(a,window)!=PKT_OK) ERROR("set pkt window");
  if(pkt_set_length(a,size)!=PKT_OK) ERROR("set length");
  if(pkt_set_seqnum(a,seq)!=PKT_OK) ERROR("set seqnum");
  if(pkt_set_timestamp(a,42)!=PKT_OK) ERROR("set timestamp");
  char * buf1 = "Deuxieme";
  if(pkt_set_payload(a,buf1,strlen(buf1))!=PKT_OK) ERROR("set payload");
  seq++;

  pkt_t *b = pkt_new();
  if(pkt_set_type(b,1)!=PKT_OK) ERROR("set pkt type");
  if(pkt_set_tr(b,0)!=PKT_OK) ERROR("set pkt tr");
  if(pkt_set_window(b,window)!=PKT_OK) ERROR("set pkt window");
  if(pkt_set_length(b,size)!=PKT_OK) ERROR("set length");
  if(pkt_set_seqnum(b,seq)!=PKT_OK) ERROR("set seqnum");
  if(pkt_set_timestamp(b,42)!=PKT_OK) ERROR("set timestamp");
  char * buf2 = "Troisieme";
  if(pkt_set_payload(b,buf2,strlen(buf2))!=PKT_OK) ERROR("set payload");

  seq++;
  pkt_t* c = pkt_new();
  if(pkt_set_type(c,1)!=PKT_OK) ERROR("set pkt type");
  if(pkt_set_tr(c,0)!=PKT_OK) ERROR("set pkt tr");
  if(pkt_set_window(c,window)!=PKT_OK) ERROR("set pkt window");
  if(pkt_set_length(c,size)!=PKT_OK) ERROR("set length");
  if(pkt_set_seqnum(c,seq)!=PKT_OK) ERROR("set seqnum");
  if(pkt_set_timestamp(c,42)!=PKT_OK) ERROR("set timestamp");
  char * buf3 = "Quatrieme";
  if(pkt_set_payload(c,buf3,strlen(buf3))!=PKT_OK) ERROR("set payload");

  seq++;


  struct Queue *queue = createQueue();
  printf("SIZE: %d\n",getSize(queue));
  addQueue(queue,pkt);
  printf("SIZE: %d\n",getSize(queue));
  addQueue(queue,a);
  printf("SIZE: %d\n",getSize(queue));
  addQueue(queue,b);
  printf("SIZE: %d\n",getSize(queue));
  addQueue(queue,c);
  printf("SIZE: %d\n",getSize(queue));

  pkt_t *searcha = pkt_new();
  searcha = searchPkt(queue,0);
  printf("Premier = %s\n", pkt_get_payload(searcha));
  searcha = pkt_new();
  searcha = searchPkt(queue,1);
  printf("Deuxieme = %s\n", pkt_get_payload(searcha));
  searcha = pkt_new();
  searcha = searchPkt(queue,2);
  printf("Troisieme = %s\n", pkt_get_payload(searcha));
  searcha = pkt_new();
  searcha = searchPkt(queue,3);
  printf("Quatrieme = %s\n", pkt_get_payload(searcha));

  pkt_t *searchb = pkt_new();
  searchb = deQueueReceiver(queue,0);
  printf("SIZE: %d\n",getSize(queue));
  printf("Premier = %s\n", pkt_get_payload(searchb));
  pkt_t *searchc = pkt_new();
  searchc = deQueueReceiver(queue,3);
  printf("SIZE: %d\n",getSize(queue));
  printf("Quatrieme = %s\n", pkt_get_payload(searchc));
   pkt_t *searchd = pkt_new();
    searchd = deQueueReceiver(queue,2);
    printf("SIZE: %d\n",getSize(queue));
    printf("Troisieme = %s\n", pkt_get_payload(searchd));
    pkt_t *searche = pkt_new();
     searche = deQueueReceiver(queue,1);
     printf("SIZE: %d\n",getSize(queue));
     printf("Deuxieme = %s\n", pkt_get_payload(searche));

     searche = deQueueReceiver(queue,5);
     printf("SIZE: %d\n",getSize(queue));
     if(searche==NULL) printf("la queue est vide\n");
     else{
       printf("Je sais plus = %s\n", pkt_get_payload(searche));
     }
     //printf("Amaury = %s\n", pkt_get_payload(searche));

}

void testPacket(){
  pkt_status_code status ;
  pkt_t *pkt = pkt_new();
  pkt_set_type(pkt,PTYPE_DATA);
  pkt_set_tr(pkt,0);
  pkt_set_window(pkt, 12);
  pkt_set_length(pkt,127);
  pkt_set_seqnum(pkt,12);
  pkt_set_timestamp(pkt,66);
  char* data = "Mathias on travaille?";
  pkt_set_payload(pkt, data, pkt_get_length(pkt));

  size_t len = 528;
  char *buf = malloc(len);
  status = pkt_encode(pkt, buf, &len);
    if(status<0)exit(-1);
    printf("-------------------------------------\n");
    printf("------------- Encodage --------------\n");
    printf("-------------------------------------\n");
    printf("TYPE : \t\t%u \n",((uint8_t)buf[0] >> 6));
    printf("TR : \t\t%u \n",((uint8_t)buf[0] >> 5) & 0b00000001);
      printf("WINDOW : \t%u \n",((uint8_t)buf[0]) & 0b00011111);
      printf("LENGTH : \t%u\n", (uint8_t)buf[1] & 0b01111111);
      printf("LENGTH2 : \t%u\n", (uint8_t)buf[2] & 0b01111111);
  char *text = malloc(27);
  memcpy(text, &buf[12], 27);
  printf("DATA : \t\t%s\n\n", text);
  free(text);
  printf("finfinfinf\n");


    pkt_t *pktd = pkt_new();
    status = pkt_decode(buf, 128, pktd);
    printf("status %d\n",status);
    if(status<0)exit(-1);
    printf("-------------------------------------\n");
    printf("------------- Decodage --------------\n");
    printf("-------------------------------------\n");
    printf("TYPE  : \t%u\n",pkt_get_type(pktd));
    printf("TR : \t\t%u\n",pkt_get_tr(pktd));
    printf("WINDOW : \t%u\n",pkt_get_window(pktd));
    printf("Length : \t%u\n",pkt_get_length(pktd));
    // printf("Seqnum decode \t: %u\n",pkt_get_seqnum(pktd));
    // printf("Timestamp decode \t: %u\n",pkt_get_timestamp(pktd));
    // printf("Crc1 decode \t: %u\n",pkt_get_crc1(pktd));
    printf("Payload : \t%s\n\n",pkt_get_payload(pktd));
    // printf("Crc2 decode \t: %u\n",pkt_get_crc2(pktd));
    pkt_del(pktd);


}

int main() {


  printf("-------------------------------------\n");
  printf("---------------- Queue --------------\n");
  printf("-------------------------------------\n");
  testQueue();
    printf("-------------------------------------\n");
    printf("--------------- Packet --------------\n");
    printf("-------------------------------------\n");

  testPacket();


  return 0;
}
