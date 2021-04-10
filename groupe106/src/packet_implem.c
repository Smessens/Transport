#include "packet_interface.h"
#include <string.h>
#include <zlib.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "log.h"

#define true 1
#define false 0

/* Extra #includes */
/* Your code will be inserted here */

struct __attribute__((__packed__)) pkt {
  uint8_t window;
  uint8_t tr;
  ptypes_t type;
  uint16_t length;
  uint8_t seqnum;
  uint32_t timestamp;
  uint32_t crc1;
  char * payload;
  uint32_t crc2;
};

/* Extra code */
/* Your code will be inserted here */

pkt_t* pkt_new()
{
  pkt_t* pkt_t_a = (pkt_t*)malloc(sizeof(pkt_t));
  if(pkt_t_a==NULL){
    return NULL;
  }
  pkt_t_a->payload=NULL;
  return pkt_t_a;
}

void pkt_del(pkt_t *pkt)
{
    free(pkt->payload);
    free(pkt);//freepaload si tu en a un puis le pkt
}

pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt)
{

  if((int)len<10){
    return E_NOHEADER;

  }
  uint8_t type = ((uint8_t) data[0]) >>6; //segment de code pour type et tr
  uint8_t tr = ((uint8_t)data[0])&0b00100000;
  tr = tr>>5;
  pkt_status_code erreur;
  bool flag = false;

  if(type==1){
    erreur = pkt_set_type(pkt,PTYPE_DATA);
    flag = true;
  }
  else if(type==2){
    if(tr==1){
      return E_TR;
    }
    erreur = pkt_set_type(pkt,PTYPE_ACK);
  }
  else if(type==3){
    if(tr==1){
      return E_TR;
    }
    erreur = pkt_set_type(pkt,PTYPE_NACK);
  }
  else{
    return E_TYPE;
  }
  if(erreur != PKT_OK) return erreur;

  erreur = pkt_set_tr(pkt,tr);
  if(erreur!=PKT_OK) return erreur;

  uint8_t window = ((uint8_t) data[0])&0b00011111; //segment de code pour window
  erreur = pkt_set_window(pkt,window);
  if(PKT_OK!=erreur) return erreur;

  int newindex=0;
  if(flag){ //DATA
    newindex = 2;
    uint16_t length;
    memcpy(&length,&data[1],2);
    length = ntohs(length);
    erreur = pkt_set_length(pkt,length);

    if(erreur!=PKT_OK) return erreur;
  } //Potentiellement else ici et peut memcpy


  uint8_t seqnum = (uint8_t) data[newindex+1]; //segment de code pour seqnum
  erreur = pkt_set_seqnum(pkt,seqnum);
  if(erreur!=PKT_OK) return erreur;

  uint32_t timestamp = (uint32_t) data[newindex+2];
  erreur = pkt_set_timestamp(pkt,timestamp);
  if(erreur!=PKT_OK) return erreur;

  uint32_t crc1;
  memcpy(&crc1,&data[newindex+6],4);
  crc1 = ntohl(crc1);
  uint32_t crc1_verif = crc32(0L,Z_NULL,0); //https://refspecs.linuxbase.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/zlib-crc32-1.html
  crc1_verif = crc32(crc1_verif, (Bytef *)&data[0],newindex+6);

  if(crc1_verif!=crc1 && pkt_get_tr(pkt)==0){
    return E_CRC;
  }
  erreur = pkt_set_crc1(pkt,crc1);
  if(erreur!=PKT_OK) return erreur;
  if(pkt_get_tr(pkt)==0 && pkt_get_type(pkt)==PTYPE_DATA){ //Il y a un payload
    uint16_t lengthpay = pkt_get_length(pkt);
    char* payload = (char *) malloc(lengthpay);
    memcpy(payload, &data[newindex+10],lengthpay);
    erreur = pkt_set_payload(pkt, payload, lengthpay);
    if(erreur!=PKT_OK) return erreur;
    free(payload);

    uint32_t crc2;
    memcpy(&crc2,&data[newindex+10+lengthpay],4);
    crc2=ntohl(crc2);
    uint32_t crc2_verif = crc32(0L,Z_NULL,0); //https://refspecs.linuxbase.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/zlib-crc32-1.html


    crc2_verif = crc32(crc2_verif, (Bytef *)&data[newindex+10],lengthpay);

    if(crc2_verif!=crc2){
      return E_CRC;
    }

    erreur = pkt_set_crc2(pkt, crc2);
    if(erreur!=PKT_OK) return erreur;

  }
  return PKT_OK;

}

pkt_status_code pkt_encode(const pkt_t* pkt, char *buf, size_t *len)
{
  u_int8_t type = ((uint8_t) pkt_get_type(pkt)<<6)&0b11000000; //type tr window
  u_int8_t tr = (pkt_get_tr(pkt)<<5)&0b00100000;
  u_int8_t window = (pkt_get_window(pkt))&0b00011111;
  u_int8_t final = type | tr | window;
  memcpy(&buf[0],&final,1);

  int newindex=0;
  int predict_length = predict_header_length(pkt);
  if(predict_length==8){
    newindex=2;
    uint16_t  lengtht = htons(pkt_get_length(pkt)); //qque chose 37856
    //memcpy(&lengtht, pkt_get_length,2);
    //lengtht = htons(lengtht);
    memcpy(&buf[1],&lengtht,2);
    uint16_t testlength;
    memcpy(&testlength,&buf[1],2);
  }
  if(predict_length==-1){
    return E_UNCONSISTENT;
  }
  uint8_t seq = pkt_get_seqnum(pkt);
  memcpy(&buf[newindex+1],&seq,1);

  uint32_t timestamp = pkt_get_timestamp(pkt);
  memcpy(&buf[newindex+2],&timestamp,4);

  uint32_t crc1 = crc32(0L,Z_NULL,0); //https://refspecs.linuxbase.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/zlib-crc32-1.html
  crc1 = crc32(crc1, (Bytef *)&buf[0],newindex+6);
  crc1 = htonl(crc1);
  memcpy(&buf[newindex+6], &crc1, 4);

  if(predict_length==8){
    uint16_t lon = pkt_get_length(pkt);
    memcpy(&buf[12],pkt_get_payload(pkt),lon);
    uint32_t crc2 = crc32(0L,Z_NULL,0); //https://refspecs.linuxbase.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/zlib-crc32-1.html
    crc2 = crc32(crc2, (Bytef *)pkt_get_payload(pkt),pkt_get_length(pkt));
    crc2 = htonl(crc2);
    memcpy(&buf[12+pkt_get_length(pkt)], &crc2, 4);
    *len = 16 + pkt_get_length(pkt);
    return PKT_OK;
  }
  *len =10;
  return PKT_OK;

}

ptypes_t pkt_get_type  (const pkt_t* pkt)
{
  return pkt->type;
}

uint8_t  pkt_get_tr(const pkt_t* pkt)
{
  return pkt->tr;
}

uint8_t  pkt_get_window(const pkt_t* pkt)
{
  return pkt->window;
}

uint8_t  pkt_get_seqnum(const pkt_t* pkt)
{
  return pkt->seqnum;
}

uint16_t pkt_get_length(const pkt_t* pkt)
{
  return pkt->length;
}

uint32_t pkt_get_timestamp   (const pkt_t* pkt)
{
  return pkt->timestamp;
}

uint32_t pkt_get_crc1   (const pkt_t* pkt)
{
  return pkt-> crc1;
}

uint32_t pkt_get_crc2   (const pkt_t* pkt)
{
  return pkt->crc2;
}

const char* pkt_get_payload(const pkt_t* pkt)
{
  return pkt->payload;
}


pkt_status_code pkt_set_type(pkt_t *pkt, const ptypes_t type)
{
  if((type==PTYPE_ACK) || (type == PTYPE_DATA) || (type==PTYPE_NACK)){
    pkt->type = type;
    return PKT_OK;
  }
  return E_TYPE;
}

pkt_status_code pkt_set_tr(pkt_t *pkt, const uint8_t tr)
{
  if((pkt->type!=PTYPE_DATA) && (tr==1)){
    return E_TR;
  }
  pkt->tr = tr;
  return PKT_OK;
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window)
{
  if(window<32){
    pkt->window = window;
    return PKT_OK;
  }
  return E_WINDOW;
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum)
{
  pkt->seqnum = seqnum;
  return PKT_OK;
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length)
{
  if(length<513){
    pkt->length = length;
    return PKT_OK;
  }
  return E_LENGTH;
}

pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp)
{
  pkt->timestamp = timestamp;
  return PKT_OK;
}

pkt_status_code pkt_set_crc1(pkt_t *pkt, const uint32_t crc1)
{
  pkt->crc1 = crc1;
  return PKT_OK;
}

pkt_status_code pkt_set_crc2(pkt_t *pkt, const uint32_t crc2)
{
  pkt->crc2 = crc2;
  return PKT_OK;
}

pkt_status_code pkt_set_payload(pkt_t *pkt,
                                const char *data,
                                const uint16_t length)
{
  pkt_status_code error = pkt_set_length(pkt, length);
  if(pkt->payload!=NULL){
    free(pkt->payload);
  }
  if(error == E_LENGTH){
    return E_LENGTH;
  }
  pkt->payload = (char*) malloc(length);
  if(pkt->payload == NULL){
    return E_NOMEM;
  }
  memcpy(pkt->payload, data, length);
  return PKT_OK;
}

ssize_t predict_header_length(const pkt_t *pkt)
{
  if(pkt->type==PTYPE_DATA && pkt->length>513){
    return -1;
  }
  if(pkt->type==PTYPE_DATA){
    return 8;
  }
  if(pkt->type==PTYPE_ACK || pkt->type==PTYPE_NACK){
    return 6;
  }
  return -1;

}


/*int main(int argc, char const *argv[]) {pkt_status_code status ;
  pkt_t *pkt = pkt_new();
  pkt->type = 0b01;
  pkt->tr = 0;
  pkt->window = 12;
  pkt->length = 127;
  pkt->seqnum = 12;
  pkt->timestamp = 66;
  char* data = "Mathias on travaille?";
  pkt_set_payload(pkt, data, pkt->length);

  size_t len = 128;
  char *buf = malloc(len);
  printf("coucocuje suis nffew\n");
  status = pkt_encode(pkt, buf, &len);
  printf("coucocuje suis new\n");
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


  return 0;
}*/
