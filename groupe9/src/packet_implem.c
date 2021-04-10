#include "packet_interface.h"

/* Extra #includes */
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <zlib.h>

struct __attribute__((__packed__)) pkt {
    ptypes_t type : 2;
    uint8_t tr : 1;
    uint8_t window : 5;
    uint8_t seqnum;
    uint16_t length;
    uint32_t timestamp;
    uint32_t crc1;
    char* payload;
    uint32_t crc2;
};

/* Extra code */
/* Your code will be inserted here */

pkt_t* pkt_new()
{
    pkt_t* pack = (pkt_t*) calloc(1, sizeof(pkt_t));
    if(pack == NULL){
        return NULL;
    }
    pack->payload = NULL;
    return pack;
}

void pkt_del(pkt_t *pkt)
{
    if(pkt != NULL){
        if(pkt->payload != NULL){
            free(pkt->payload);
        }
        free(pkt);
    }
}

void printpacket(pkt_t *pkt){
    fprintf(stderr, "Type : %d\n",pkt_get_type((const pkt_t*) pkt));
    fprintf(stderr, "TR : %d\n",pkt_get_tr((const pkt_t*) pkt));
    fprintf(stderr, "Window : %d\n",pkt_get_window((const pkt_t*) pkt));
    fprintf(stderr, "SeqNum : %d\n",pkt_get_seqnum((const pkt_t*) pkt));
    fprintf(stderr, "Length : %d\n",pkt_get_length((const pkt_t*) pkt));
    fprintf(stderr, "TimeStamp : %d\n",pkt_get_timestamp((const pkt_t*) pkt));
}

pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt)
{
    if(len < 6){
        return E_NOHEADER;
    }

    //TYPE 2   TR 1   WINDOW 5
    uint8_t ttw;
    memcpy(&ttw, data, 1);

    uint8_t ttype = ttw >> 6;
    pkt_set_type(pkt, ttype);
    uint8_t ttr = ttw << 2;
    ttr = ttr >> 7;
    pkt_set_tr(pkt, ttr);
    uint8_t twin = ttw << 3;
    twin = twin >> 3;
    pkt_set_window(pkt, twin);

    ptypes_t type = pkt_get_type(pkt);
    if((type != PTYPE_DATA) && (type != PTYPE_ACK) && (type != PTYPE_NACK)){
        return E_TYPE;
    }

    if(type != PTYPE_DATA && pkt->tr == 1){
        return E_TR;
    }

    if(len < 8 && type == PTYPE_DATA){
        return E_NOHEADER;
    }

    //LENGTH 16
    int isData = 0;
    if(type == PTYPE_DATA){
        isData = 2;
        uint16_t leng;
        memcpy(&leng, data + 1, 2);
        leng = ntohs(leng);
        pkt_set_length(pkt,leng);

        if(pkt->length > MAX_PAYLOAD_SIZE){
            return E_LENGTH;
        }
    }
    else{
        pkt->length = 0;
    }

    //SEQNUM 8
    uint8_t segn;
    memcpy(&segn, data+1+isData, 1);
    pkt_set_seqnum(pkt,segn);

    //TIMESTAMP 32
    uint32_t timest;
    memcpy(&timest, data+2+isData, 4);
    pkt_set_timestamp(pkt,timest);

    //CRC1 32
    uint32_t cr1;
    memcpy(&cr1, data+6+isData, 4);
    pkt_set_crc1(pkt, cr1);
    pkt->crc1 = ntohl(pkt->crc1);

    ttr = pkt->tr;
    pkt->tr = 0;
    uint32_t tcrc1 = 0;
    tcrc1 = crc32(tcrc1, (Bytef *)data, 6+isData);
    if(tcrc1 != pkt->crc1){
        return E_CRC;
    }
    pkt->tr = ttr;

    if(pkt->length == 0){
        return PKT_OK;
    }

    if(pkt->tr == 1){
        return E_UNCONSISTENT;
    }

    //PAYLOAD LENGTH


    pkt_set_payload(pkt, data+12, pkt->length);


    //CRC2 32
    uint32_t cr2;
    memcpy(&cr2, data+12+pkt->length, 4);
    cr2 = ntohl(cr2);
    pkt_set_crc2(pkt, cr2);

    uint32_t tcrc2 = 0;
    tcrc2= crc32(tcrc2, (Bytef *)(data+12), pkt->length);
    if(tcrc2 != pkt->crc2){
        fprintf(stderr, "crc2 %d vs crc2 %d\n",tcrc2, pkt->crc2);
        return E_CRC;
    }

    return PKT_OK;
}

pkt_status_code pkt_encode(const pkt_t* pkt, char *buf, size_t *len)
{
    //CHECK MEMORY
    if(predict_header_length(pkt) == -1){
        return E_LENGTH;
    }
    if(predict_header_length(pkt)+pkt_get_length(pkt)+sizeof(uint32_t) > *len){
        return E_NOMEM;
    }
    if(pkt_get_crc2(pkt) != 0){
        if(*len < 4*sizeof(uint32_t)+pkt_get_length(pkt)){
            return E_NOMEM;
        }
    }

    //TYPE TR WINDOW
    uint8_t result = 0;
    if(pkt_get_type(pkt) ==PTYPE_DATA){
        result += 64;
    }
    if(pkt_get_type(pkt) ==PTYPE_ACK){
        result += 128;
    }
    if(pkt_get_type(pkt) ==PTYPE_NACK){
        result += 192;
    }
    if(pkt_get_tr(pkt) > 0){
        result += 32;
    }
    result += pkt_get_window(pkt);
    memcpy(buf,(char *) &result, 1);
    *len = 1;

    //LENGTH if DATA
    if(pkt_get_type(pkt) == PTYPE_DATA){
        uint16_t nslen = htons(pkt_get_length(pkt));
        memcpy(buf+*len, &nslen, 2);
        *len += 2;
    }

    //SEQNUM
    memcpy(buf+*len, &(pkt->seqnum),1);
    *len += 1;

    //TIMESTAMP
    memcpy(buf+*len, &(pkt->timestamp),4);
    *len += 4;

    //CRC1
    uint8_t ttr = buf[0];
    buf[0] = buf[0] & 0b11011111;
    uint32_t tcrc1 = 0;
    tcrc1 = crc32(tcrc1, (Bytef *)buf, *len);
    buf[0] = ttr;

    uint32_t nlcrc1 = htonl(tcrc1);
    memcpy(buf+*len, &nlcrc1,4);
    *len += 4;

    //PAYLOAD
    memcpy(buf+*len, pkt_get_payload(pkt), pkt_get_length(pkt));
    *len += pkt_get_length(pkt);

    if(pkt_get_payload(pkt)==0){
        return PKT_OK;
    }

    //CRC2
    uint32_t tcrc2 = 0;
    tcrc2= crc32(tcrc2, (Bytef *)(pkt->payload), pkt->length);
    uint32_t nlcrc2 = htonl(tcrc2);
    memcpy(buf+*len, &nlcrc2,4);
    *len += 4;

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
    return pkt->crc1;
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
    if(type != PTYPE_DATA && type != PTYPE_ACK && type != PTYPE_NACK){
        return E_TYPE;
    }
    pkt->type = type;
    return PKT_OK;
}

pkt_status_code pkt_set_tr(pkt_t *pkt, const uint8_t tr)
{
    if(tr != 1 && tr != 0){
        return E_TR;
    }
    pkt->tr = tr;
    return PKT_OK;
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window)
{
    if(window>MAX_WINDOW_SIZE){
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
    if(length>MAX_PAYLOAD_SIZE){
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
    if(length>MAX_PAYLOAD_SIZE){
        return E_LENGTH;
    }

    if(pkt->payload != NULL){
        free(pkt->payload);
    }

    pkt->payload = (char *) malloc(length);
    if (pkt->payload == NULL){
        return E_NOMEM;
    }

    memcpy(pkt->payload, data, length);
    pkt->length = length;
    return PKT_OK;
}

ssize_t predict_header_length(const pkt_t *pkt)
{
    if ((pkt->type == PTYPE_NACK || pkt->type == PTYPE_ACK) && pkt->length != 0 ){
        return -1;
    }
    if(pkt->length > MAX_PAYLOAD_SIZE){
        return -1;
    }
    if(pkt->type == PTYPE_DATA){
        return 8;
    }
    return 6;
}