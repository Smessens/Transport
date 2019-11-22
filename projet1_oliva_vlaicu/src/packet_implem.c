#include "packet_interface.h"

/* Extra #includes */
/* Your code will be inserted here */

struct __attribute__((__packed__)) pkt {
  ptypes_t type:2;
  uint8_t tr:1;
  uint8_t window:5;
  uint8_t l:1;
  uint16_t length;
  uint8_t seqnum;
  uint32_t timestamp;
  uint32_t crc;
  uint32_t crc2;
  char *payload;
};

/* Extra code */
/* Your code will be inserted here */

pkt_t* pkt_new()
{
  pkt_t *pkt = calloc(1, sizeof(pkt_t));
  if(pkt==NULL){
    perror("Calloc error");
    return NULL;
  }
  return pkt;
}

void pkt_del(pkt_t *pkt)
{
  if(pkt) {
    if(pkt->payload)
    free(pkt->payload);
    free(pkt);
  }
}

/*
* Decode des donnees recues et cree une nouvelle structure pkt.
* Le paquet recu est en network byte-order.
* La fonction verifie que:
* - Le CRC32 du header recu est le mÃƒÂªme que celui decode a la fin
*   du header (en considerant le champ TR a 0)
* - S'il est present, le CRC32 du payload recu est le meme que celui
*   decode a la fin du payload
* - Le type du paquet est valide
* - La longueur du paquet et le champ TR sont valides et coherents
*   avec le nombre d'octets recus.
*
* @data: L'ensemble d'octets constituant le paquet recu
* @len: Le nombre de bytes recus
* @pkt: Une struct pkt valide
* @post: pkt est la representation du paquet recu
*
* @return: Un code indiquant si l'operation a reussi ou representant
*         l'erreur rencontree.
*/
pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt)
{
  // The first byte contains type, tr and window
  uint8_t typeTrWindow;
  memcpy(&typeTrWindow, data, 1);
  // Window is the last 5 bits
  uint8_t window = typeTrWindow & 31;
  // TR is the third bit
  uint8_t tr = (typeTrWindow & 32)>>5;
  // Type is the first 2 bits
  ptypes_t type = typeTrWindow>>6;

  // Check if length is 1 or 2 bytes and get the length
  uint16_t length;
  size_t byte_length = varuint_len((uint8_t *) data+1);
  varuint_decode((uint8_t *) data+1, byte_length, (uint16_t *)&length);

  // Check if tr is correct with type
  if((type==2 || type==3) && (tr==1)){
    return E_TR;
  }

  // Check if length is null when tr is 1
  if((tr==1) && (length!=0)){
    return E_LENGTH;
  }
  // Check if length is relevant
  if((unsigned)(len) < (unsigned)(length+byte_length+14)){ // MODIFIED 14 -> 10
    return E_UNCONSISTENT;
  }

  // Seqnum is third or fourth byte
  uint8_t seqnum;
  memcpy(&seqnum, data+byte_length+1, 1);

  // Timestamp is fourth or fifth bytes and is 4 bytes long
  uint32_t timestamp;
  memcpy(&timestamp, data+byte_length+2, 4);

  // Get and check crc
  uint32_t crc;
  memcpy(&crc, data+byte_length+6, 4);
  crc = ntohl(crc);
  // Recompute crc only if tr==0
  if(tr==0){
    if(crc != (uint32_t)crc32(0L, (Bytef *)data, byte_length+6)){
      return E_CRC;
    }
  }

  // Get payload only if there is one
  char payload[length];
  if(length > 0){
    memcpy(payload, data+byte_length+10, length);
  }

  // Get and check crc2
  uint32_t crc2;
  memcpy(&crc2, data+byte_length+10+length, 4);
  crc2 = ntohl(crc2);
  // Recompute crc2 only if tr==0
  if(tr==0){
    if(crc2 != (uint32_t)crc32(0L, (Bytef *)data+byte_length+10, length)){
      return E_CRC;
    }
  }

  // Set pkt
  pkt_status_code t = pkt_set_type(pkt, type);
  if(t != PKT_OK){
    return t;
  }
  pkt_status_code r = pkt_set_tr(pkt, tr);
  if(r != PKT_OK){
    return r;
  }
  pkt_status_code w = pkt_set_window(pkt, window);
  if(w != PKT_OK){
    return w;
  }
  pkt_status_code l = pkt_set_length(pkt, length);
  if(l != PKT_OK){
    return l;
  }
  pkt_status_code s = pkt_set_seqnum(pkt, seqnum);
  if(s != PKT_OK){
    return s;
  }
  pkt_status_code ti = pkt_set_timestamp(pkt, timestamp);
  if(ti != PKT_OK){
    return ti;
  }
  pkt_status_code c = pkt_set_crc1(pkt, crc);
  if(c != PKT_OK){
    return c;
  }
  pkt_status_code p = pkt_set_payload(pkt, payload, length);
  if(p != PKT_OK){
    return p;
  }
  return PKT_OK;
}

pkt_status_code pkt_encode(const pkt_t* pkt, char *buf, size_t *len)
{
  // Check if length is correct
  size_t byte_length = varuint_predict_len(pkt->length);
  if(pkt->length>512){
    return E_LENGTH;
  }
  memset(buf, 0, *len);
  if((unsigned)(pkt->length+byte_length+10) > (unsigned)(*len)){ // MODIFIED 14 -> 10
    return E_NOMEM;
  }

  // Check if tr is correct with type
  if((pkt->type==2 || pkt->type==3) && (pkt->tr==1)){
    return E_TR;
  }

  // Check if length is null when tr is 1
  if((pkt->tr==1) && (pkt->length!=0)){
    return E_LENGTH;
  }

  // Construct the header
  // Combine type, tr and window
  uint8_t typeTrWindow = pkt_get_type(pkt)<<6 | pkt_get_tr(pkt)<<5 | pkt_get_window(pkt);
  memcpy(buf, &typeTrWindow, 1);

  // Encode length
  int err = varuint_encode(pkt->length,(uint8_t *) buf+1, byte_length);
  if(err == -1){
    return E_LENGTH;
  }

  // Set seqnum
  memcpy(buf+byte_length+1, &(pkt->seqnum), 1);

  // Set timestamp
  memcpy(buf+byte_length+2, &(pkt->timestamp), 4);

  // Compute and set crc
  uint32_t crc = crc32(0L, (Bytef *)buf, 6+byte_length);
  crc = htonl(crc);
  memcpy(buf+byte_length+6, &crc, 4);

  // Set payload
  if(pkt->length){
    memcpy(buf+byte_length+10, pkt->payload, pkt->length);
  }

  // Set crc2
  if(pkt->tr!=1){
    if(pkt->length){
      uint32_t crc2 = crc32(0L, (Bytef *)buf+10+byte_length, pkt->length);
      crc2 = htonl(crc2);
      memcpy(buf+byte_length+10+pkt->length, &crc2, 4);
    }
  }

  // POST-len
  *len = byte_length+10+pkt->length; // MODIFIED 14 -> 10
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
  return pkt->crc;
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
  if(type!=1 && type!=2 && type!=3){
    return E_TYPE;
  }
  pkt->type = type;
  return PKT_OK;
}

pkt_status_code pkt_set_tr(pkt_t *pkt, const uint8_t tr)
{
  if(tr>1){
    return E_TR;
  }
  pkt->tr = tr;
  return PKT_OK;
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window)
{
  if(window>31){
    return E_WINDOW;
  }
  pkt->window = window;
  return PKT_OK;
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum)
{
  pkt->seqnum = seqnum;
  return PKT_OK;
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length)
{
  if(length>512){
    return E_LENGTH;
  }
  pkt->length = length;
  return PKT_OK;
}

pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp)
{
  pkt->timestamp = timestamp;
  return PKT_OK;
}

pkt_status_code pkt_set_crc1(pkt_t *pkt, const uint32_t crc1)
{
  pkt->crc = crc1;
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
    if(pkt->payload){
      free(pkt->payload);
    }
    pkt->payload = (char *)malloc(length);
    if(pkt->payload == NULL){
      return E_NOMEM;
    }
    memcpy(pkt->payload, data, length);
    pkt_status_code err=pkt_set_length(pkt, length);
    if(err==E_LENGTH) {
      return E_LENGTH;
    }
    return PKT_OK;
  }


  ssize_t varuint_decode(const uint8_t *data, const size_t len, uint16_t *retval)
  {
    if(len>2){
      return -1;
    }
    // Length is 1 byte
    if(len==1){
      *retval = *data;
      return 1;
    }
    // Length is 2 bytes
    else {
      memcpy(retval, data, 2);
      *retval = ntohs(*retval & htons(32767));
      return 2;
    }
  }


  ssize_t varuint_encode(uint16_t val, uint8_t *data, const size_t len)
  {
    // Invalid length
    if(val>0x8000){
      return -1;
    }
    // Length is 1 byte
    if(len==1){
      *data = (uint8_t) val;
      return 1;
    }
    // Length is 2 bytes
    else {
      uint16_t encoded = htons(32768) | htons(val);
      memcpy(data, &encoded, 2);
      return 2;
    }
  }

  size_t varuint_len(const uint8_t *data)
  {
    // First bit is 0 -> length is 1 byte
    if((0x80 & *data)!=0x80){
      return 1;
    }
    // First bit is 1 -> length is 2 bytes
    else {
      return 2;
    }
  }


  ssize_t varuint_predict_len(uint16_t val)
  {
    // Val is greater than 15 bits max value -> invalid value
    if(val>=0x8000){
      return -1;
    }
    // Val is lower or equal to 7 bits max value -> size is 1 byte
    if(val<0x80){
      return 1;
    }
    // Otherwise size is 2 bytes
    else {
      return 2;
    }
  }


  ssize_t predict_header_length(const pkt_t *pkt)
  {
    // Length is greater than 15 bits max value -> invalid value
    if(pkt->length>=0x8000){
      return -1;
    }
    // Length is lower or equal to 7 bits max value -> size is 3 bytes
    if(pkt->length<0x80){
      return 7;
    }
    // Otherwise size is 4 bytes
    else {
      return 8;
    }
  }
