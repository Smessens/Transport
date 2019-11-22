#include "packet_interface.h"

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

struct __attribute__((__packed__)) pkt {
  uint8_t window:5;
  uint8_t tr:1;
  uint8_t type:2;
  uint16_t length;
  uint8_t seqnum;
  uint32_t timestamp;
  uint32_t crc1;
  uint8_t* payload;
  uint32_t crc2;
};

void printPkt(pkt_t *pkt)
{
  printf("TYPE      : %2x\n",pkt->type);
  printf("TR        : %2x\n",pkt->tr);
  printf("WINDOW    : %2x\n",pkt->window);
  printf("L         : %2zx\n",varuint_predict_len(pkt_get_length(pkt)));
  if(varuint_predict_len(pkt_get_length(pkt))==1)
    printf("LENGTH    : %2x\n",pkt_get_length(pkt));
  if(varuint_predict_len(pkt_get_length(pkt))==2)
    printf("LENGTH    : %4x\n",pkt_get_length(pkt));
  printf("SEQNUM    : %d\n",pkt->seqnum);
  printf("TIMESTAMP : %8x\n",pkt->timestamp);
  printf("CRC1      : %8x\n",pkt->crc1);
  int i;
  if(pkt->type==PTYPE_DATA && pkt->tr==0)
  {
    printf("PAYLOAD   : ");
    for (i=0; i<pkt_get_length(pkt); i++)
    {
      printf("%c",pkt->payload[i]);
    }
    printf("\n");
    printf("CRC2      : %8x\n",pkt->crc2);
  }
}

pkt_t* pkt_new() {return calloc(1,sizeof(pkt_t));}

void pkt_del(pkt_t *pkt)
{
  if(pkt!=NULL)
  {
    if(pkt->payload!=NULL)
    {
      free((void *)pkt->payload);
    }
    free(pkt);
  }
}

pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt)
{
  // TYPE, TR, WINDOW
  memcpy(pkt,data,sizeof(uint8_t));

  if((pkt_get_type(pkt)!=PTYPE_DATA)&&(pkt_get_type(pkt)!=PTYPE_ACK)&&(pkt_get_type(pkt)!=PTYPE_NACK))
  {
    return(E_TYPE);
  }
  if((pkt_get_type(pkt)==PTYPE_ACK||pkt_get_type(pkt)==PTYPE_NACK)&&(pkt_get_tr(pkt)!=0))
  {
    return(E_TR);
  }

  // LENGTH
  uint16_t lengthPayload;
  ssize_t l = varuint_decode((uint8_t*)(data+1), varuint_len((uint8_t *) (data+1)), &lengthPayload);
  pkt_set_length(pkt,lengthPayload);

  if(l==-1)
  {
    return(E_LENGTH);
  }
  // TODO
  // if((pkt_get_length(pkt)+10+l)>(int)len)
  // {
  //   return(E_UNCONSISTENT);
  // }

  // SEQNUM, TIMESTAMP
  memcpy(&(pkt->seqnum),&data[1+l],5*sizeof(uint8_t));

  //CRC1
  uint32_t crc1 = ntohl(*(uint32_t *)(&data[6+l]));
  pkt_set_crc1(pkt,crc1);
  uint32_t calCRC1 = crc32(0L, Z_NULL, 0);
  char copyHeader[6+l];
  memcpy((void*)&copyHeader,(void*)data,6+l);
  copyHeader[0]=copyHeader[0] & 0b11011111;
  calCRC1 = crc32(calCRC1, (const Bytef *)copyHeader, 6+l);
  if(calCRC1!=pkt_get_crc1(pkt))
  {
    return(E_CRC);
  }

  if(pkt_get_tr(pkt) == 0 && pkt_get_type(pkt)==PTYPE_DATA)
  {
    // PAYLOAD
    pkt_status_code stat = pkt_set_payload(pkt, data+10+l, pkt_get_length(pkt));
    if(stat==E_NOMEM)
    {
      return(E_NOMEM);
    }

    // CRC2
    int j = pkt_get_length(pkt) + 10 + l;
    if(len>(size_t)j)
    {
      uint32_t crc2 = ntohl(*(uint32_t *)(&data[10+l+pkt_get_length(pkt)]));
      pkt_set_crc2(pkt,crc2);

      uint32_t calCRC2 = crc32(0L, Z_NULL, 0);
      calCRC2 = crc32(calCRC2, (const Bytef *)&data[10+l], pkt_get_length(pkt));
      if(calCRC2!=pkt_get_crc2(pkt))
      {
        return(E_CRC);
      }
    }
  }
  return(PKT_OK);
}

pkt_status_code pkt_encode(const pkt_t* pkt, char *buf, size_t *len)
{
  // TYPE, TR, WINDOW
  memcpy(buf,pkt,sizeof(uint8_t));

  // LENGTH
  ssize_t l = varuint_encode(pkt_get_length(pkt), (uint8_t*)buf+1, (*len)-1);
  if(l==-1)
  {
    return(E_NOMEM);
  }

  // SEQNUM, TIMESTAMP
  memcpy(&buf[1+l],&(pkt->seqnum),5);

  *len = 6+l;

  // CRC1
  uint32_t calCRC1 = crc32(0L, Z_NULL, 0);
  char copyHeader[6+l];
  memcpy((void*)&copyHeader,(void*)buf,6+l);
  copyHeader[0]=copyHeader[0] & 0b11011111;
  calCRC1 = crc32(calCRC1, (const Bytef *)copyHeader, 6+l);
  uint32_t CRC1buf = htonl(calCRC1);
  memcpy(&buf[6+l],&CRC1buf,4);
  *len += 4;

  if(pkt_get_type(pkt)==PTYPE_DATA)
  {
    // PAYLOAD
    memcpy(&buf[10+l],pkt_get_payload(pkt),pkt_get_length(pkt));
    *len += pkt_get_length(pkt);

    // CRC2
    if(pkt_get_length(pkt)>0)
    {
      uint32_t calCRC2 = crc32(0L, Z_NULL, 0);
      calCRC2 = crc32(calCRC2, (const Bytef *)&buf[10+l], pkt_get_length(pkt));
      uint32_t CRC2buf = htonl(calCRC2);
      memcpy(&buf[10+l+pkt_get_length(pkt)],((uint8_t*)&CRC2buf),4);
      *len += 4;
    }
  }

  return(PKT_OK);
}

ptypes_t pkt_get_type (const pkt_t* pkt){return(pkt->type);}

uint8_t  pkt_get_tr(const pkt_t* pkt){return(pkt->tr);}

uint8_t  pkt_get_window(const pkt_t* pkt){return(pkt->window);}

uint8_t  pkt_get_seqnum(const pkt_t* pkt){return(pkt->seqnum);}

uint16_t pkt_get_length(const pkt_t* pkt){return(pkt->length);}

uint32_t pkt_get_timestamp(const pkt_t* pkt){return(pkt->timestamp);}

uint32_t pkt_get_crc1(const pkt_t* pkt){return(pkt->crc1);}

uint32_t pkt_get_crc2(const pkt_t* pkt){return(pkt->crc2);}

const char* pkt_get_payload(const pkt_t* pkt){return((char*)pkt->payload);}

pkt_status_code pkt_set_type(pkt_t *pkt, const ptypes_t type)
{
  pkt->type = type;
  return(0);
}

pkt_status_code pkt_set_tr(pkt_t *pkt, const uint8_t tr)
{
  pkt->tr = tr;
  return(0);
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window)
{
  pkt->window = window;
  return(0);
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum)
{
  pkt->seqnum = seqnum;
  return(0);
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length)
{
  if(length>MAX_PAYLOAD_SIZE)
  {
    return(E_LENGTH);
  }
  pkt->length = length;
  return(PKT_OK);
}

pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp)
{
  memcpy(&(pkt->timestamp),&timestamp,4);
  return(0);
}

pkt_status_code pkt_set_crc1(pkt_t *pkt, const uint32_t crc1)
{
  pkt->crc1 = crc1;
  return(0);
}

pkt_status_code pkt_set_crc2(pkt_t *pkt, const uint32_t crc2)
{
  pkt->crc2 = crc2;
  return(0);
}

pkt_status_code pkt_set_payload(pkt_t *pkt, const char *data, const uint16_t length)
{
  pkt_status_code stat = pkt_set_length(pkt, length);
  if(stat==E_LENGTH)
  {
    return(E_LENGTH);
  }
  pkt->payload = (uint8_t*)realloc(pkt->payload,length);
  memcpy((void *)pkt->payload, data, length);
  return(PKT_OK);
}

ssize_t varuint_decode(const uint8_t *data, const size_t len, uint16_t *retval)
{
  ssize_t l = varuint_len(data);

  if(l>(ssize_t)len)
  {
    return(-1);
  }

  uint8_t varuint[l];
  memcpy(varuint,data,l);
  *varuint = *varuint&0b01111111;

  if(l==1)
  {
    *retval = *varuint;
    return(l);
  }
  if (l==2)
  {
    memcpy(retval,varuint,l);
    *retval = ntohs(*retval);
    return(l);
  }
  return(-1);
}

ssize_t varuint_encode(uint16_t val, uint8_t *data, const size_t len)
{
  ssize_t l = varuint_predict_len(val);


  if((int)len<(int)l)
  {
    return(-1);
  }

  if(l==1)
  {
    uint8_t val8 = (uint8_t)val;
    *data = val8;
    return(l);
  }
  if(l==2)
  {
    val = htons(val | 0b1000000000000000);
    memcpy(data,&val,l);
    return(l);
  }
  return(-1);
}

size_t varuint_len(const uint8_t *data)
{
  int l = ((uint8_t)data[0] & 0b10000000)>>7;
  return(l+1);

}

ssize_t varuint_predict_len(uint16_t val)
{
    if(val>MAX_PAYLOAD_SIZE)
    {
      return(-1);
    }
    if(val>0b01111111)
    {
      return(2);
    }
    return(1);
}

ssize_t predict_header_length(const pkt_t *pkt)
{
    return(6 + varuint_predict_len(pkt->length));
}

void printStatus(pkt_status_code dec)
{
  if(dec==E_TYPE){
    perror("ERROR TYPE");
  }
  else if(dec==E_TR){
    perror("ERROR TR");
  }
  else if(dec==E_LENGTH){
    perror("ERROR LENGTH");
  }
  else if(dec==E_CRC){
    perror("ERROR CRC");
  }
  else if(dec==E_WINDOW){
    perror("ERROR WINDOW");
  }
  else if(dec==E_SEQNUM){
    perror("ERROR SEQNUM");
  }
  else if(dec==E_NOMEM){
    perror("ERROR NOMEM");
  }
  else if(dec==E_NOHEADER){
    perror("ERROR NOHEADER");
  }
  else if(dec==E_UNCONSISTENT){
    perror("ERROR PACKET UNCONSISTENT");
  }
}
