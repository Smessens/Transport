#include "packet_interface.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <netdb.h>
#include <math.h>

/* Extra #includes */
/* Your code will be inserted here */

/* Extra code */
/* Your code will be inserted here */

pkt_t* pkt_new()
{
    pkt_t* paquet = (pkt_t*) malloc(sizeof(pkt_t));

    if (paquet == NULL) return NULL;


    paquet->payload = (char*) malloc(sizeof(char)*MAX_PAYLOAD_SIZE);
    if (&(paquet->payload) == NULL) return NULL;

    return paquet;
}

void pkt_del(pkt_t *pkt)
{
    free(pkt->payload);
    free(pkt);
}

pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt)
{
    int place = 0;

    if (len < 7) return E_NOHEADER;

    uint8_t type = (uint8_t) data[place] >> 6;

    if (pkt_set_type(pkt, (ptypes_t) type) != PKT_OK) return E_TYPE;

    uint8_t TR = (uint8_t) ((((uint8_t) data[place]) & 0x3F)  >> 5);
    if (pkt_set_tr(pkt, TR) != PKT_OK) return E_TR;

    if (pkt_get_type(pkt) == PTYPE_DATA && pkt_get_tr(pkt) == 1) return E_UNCONSISTENT;

    uint8_t window = (uint8_t) (data[place] & 0x1f);
    if (pkt_set_window(pkt, window) != PKT_OK) return E_WINDOW;

    place ++;

    size_t buffSize = 2;
    uint8_t* buff = malloc(buffSize);
    if (buff == NULL) return E_LENGTH;
    memcpy(buff, &data[place], buffSize);
    if (varuint_len(buff) == (size_t) -1) return E_LENGTH;
    else if (varuint_len(buff) == (ssize_t) 1) {
      buffSize = 1;
      place --;
    }

    uint16_t *valRet = malloc(sizeof(uint16_t));
    if (valRet == NULL) return E_LENGTH;
    ssize_t err = varuint_decode(buff, buffSize, valRet);
    if (err == -1) return E_LENGTH;
    if (pkt_set_length(pkt, *valRet) != PKT_OK) return E_LENGTH;

    place +=2;

    free(buff);
    free(valRet);

    uint8_t seqnum = (uint8_t) data[place];
    if (pkt_set_seqnum(pkt, seqnum) != PKT_OK) return E_SEQNUM;

    place ++;

    uint32_t tmstamp;
    memcpy(&tmstamp, &data[place], 4);
    if (pkt_set_timestamp(pkt, tmstamp) != PKT_OK) return E_NOHEADER;

    place += 4;

    uint32_t CRC1;
    memcpy(&CRC1, &data[place], 4);
    CRC1 = ntohl(CRC1);
    if (pkt_set_crc1(pkt, CRC1) != PKT_OK) return E_CRC;

    uLong CRC;
    char *header = malloc(predict_header_length(pkt)*sizeof(char));
    if (header == NULL) return E_CRC;

    memcpy(header, data, predict_header_length(pkt));

    CRC = crc32(crc32(0L, Z_NULL, 0), (const unsigned char*) header, predict_header_length(pkt));

    if (CRC1 != CRC) return E_CRC;

    place += 4;
    free(header);

    if (pkt_get_tr(pkt) == 1) return PKT_OK;

    char *payload = malloc((size_t) pkt_get_length(pkt));
    if (payload == NULL) return E_NOMEM;

    memcpy(payload, &data[place], pkt_get_length(pkt));
    if (pkt_set_payload(pkt, payload, (size_t) pkt_get_length(pkt)) != PKT_OK) return E_NOMEM;

    place += pkt_get_length(pkt);

    uint32_t CRC2;
    memcpy(&CRC2,&data[place], 4);
    CRC2 = ntohl(CRC2);
    if (pkt_set_crc2(pkt, CRC2) != PKT_OK) return E_CRC;

    CRC = crc32(crc32(0L, Z_NULL, 0), (const unsigned char *) payload, (size_t) pkt_get_length(pkt));
    if (CRC != CRC2) return E_CRC;

    free(payload);

    return PKT_OK;
}

pkt_status_code pkt_encode(const pkt_t* pkt, char *buf, size_t *len)
{
    if (*len < (predict_header_length(pkt) + (size_t) 8 + (size_t) pkt_get_length(pkt))) return E_NOMEM;
    uint16_t length = pkt->length;
    int place = 0;
    uint8_t type = (uint8_t) pkt_get_type(pkt) << 6;
    uint8_t TR = pkt_get_tr(pkt) << 5;
    uint8_t window = pkt_get_window(pkt);
    //uint8_t firstOctet = type + TR + window;
    buf[place] = type + TR + window;

    place ++;

    ssize_t L = varuint_predict_len(length);
    if (L == -1) return E_LENGTH;
    else if (L == 1){
      uint8_t lengthToCpy = (uint8_t) length;
      memcpy(&buf[place], &lengthToCpy, 1);
      place ++;
    }
    else {
      uint8_t *buffLen = malloc(2);
      if (buffLen == NULL) return E_NOMEM;
      if (varuint_encode(length, buffLen, 2) == -1) return -1;
      memcpy(&buf[place], (char *) buffLen, 2);
      place += 2;
      free(buffLen);
    }

    buf[place] = (char) pkt_get_seqnum(pkt);
    place ++;

    uint32_t tmstamp = pkt_get_timestamp(pkt);
    memcpy(&buf[place], &tmstamp, 4);
    place += 4;

    uLong CRC;
    char *header = malloc(predict_header_length(pkt)*sizeof(char));
    if (header == NULL) return E_CRC;

    memcpy(header, buf, predict_header_length(pkt));
    CRC = htonl(crc32(crc32(0L, Z_NULL, 0), (const unsigned char *) header, predict_header_length(pkt)));
    memcpy(&buf[place], &CRC, 4);
    place += 4;

    if (pkt_get_type(pkt) == PTYPE_DATA) {
      memcpy(&buf[place], pkt_get_payload(pkt), length);
      place += length;
      uLong CRC2 = htonl(crc32(crc32(0L, Z_NULL, 0), (const unsigned char *) pkt_get_payload(pkt), length));
      memcpy(&buf[place], &CRC2, 4);
      place +=4;
    }

    *len = place;

    return PKT_OK;

}

ptypes_t pkt_get_type  (const pkt_t* pkt)
{
    return (ptypes_t) pkt->type;
}

uint8_t  pkt_get_tr(const pkt_t* pkt)
{
    return pkt->TR;
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
    if(pkt->TR == 0){
        return pkt->length;
    }
    return 0;
}

uint32_t pkt_get_timestamp   (const pkt_t* pkt)
{
    return pkt->timestamp;
}

uint32_t pkt_get_crc1   (const pkt_t* pkt)
{
    return pkt->CRC1;
}

uint32_t pkt_get_crc2   (const pkt_t* pkt)
{
    return pkt->CRC2;
}

const char* pkt_get_payload(const pkt_t* pkt)
{
    return pkt->payload;
}


pkt_status_code pkt_set_type(pkt_t *pkt, const ptypes_t type)
{
    if (type > 3 || type < 1) return E_TYPE;

    pkt->type = type;
    return PKT_OK;
}

pkt_status_code pkt_set_tr(pkt_t *pkt, const uint8_t tr)
{
    if (tr > 1) return E_TR;

    pkt->TR = tr;
    return PKT_OK;
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window)
{
    if (window > MAX_WINDOW_SIZE) return E_WINDOW;

    pkt->window = window;
    return PKT_OK;
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum)
{
    pkt->seqnum = seqnum % MAX_SEQUENCE_NUMBER;
    return PKT_OK;
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length)
{
    if (length > MAX_PAYLOAD_SIZE) return E_LENGTH;

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
    pkt->CRC1 = crc1;
    return PKT_OK;
}

pkt_status_code pkt_set_crc2(pkt_t *pkt, const uint32_t crc2)
{
    pkt->CRC2 = crc2;
    return PKT_OK;
}

pkt_status_code pkt_set_payload(pkt_t *pkt, const char *data, const uint16_t length)
{
    if (length != pkt->length && length > MAX_PAYLOAD_SIZE) return E_LENGTH;

    memcpy(pkt->payload, data, length);
    pkt->length = length;
    return PKT_OK;
}


ssize_t varuint_decode(const uint8_t *data, const size_t len, uint16_t *retval)
{
    if (len == 1)
    {
        *retval = (uint16_t) data[0];
        return 1;
    }
    else{
        uint16_t v;
        memcpy(&v, data, 2);
        v = ntohs(v);
        v = v - pow(2,15);
        *retval = v;
        return 2;
    }
}


ssize_t varuint_encode(uint16_t val, uint8_t *data, const size_t len)
{
    if ( (uint8_t) len < (uint8_t) varuint_predict_len(val)) return -1;

    uint16_t v;
    if (val < (uint8_t) pow(2,7)-1)
    {
        data[0] = (uint8_t) val;
        return 1;
    }
    else{
        v = val + pow(2,15);
        v = htons(v);
        memcpy(data, &v, 2);
        return 2;
    }
}

size_t varuint_len(const uint8_t *data)
{
    return (data[0] >> 7)+1;
}


ssize_t varuint_predict_len(uint16_t val)
{
    if (val >= pow(2,15))
    {
        return -1;
    }
    if (val > pow(2,7)-1)
    {
        return 2;
    }
    return 1;
}


ssize_t predict_header_length(const pkt_t *pkt)
{
    uint16_t data = pkt->length;
    size_t len = varuint_len((uint8_t*) &data);
    uint16_t* retval = malloc(2);
    if (retval == NULL) return -1;

    int l = (int) varuint_decode((uint8_t*) &data, len, retval);
    if (l == -1) return -1;

    return (ssize_t) l + 6;
}
