#include "additionalfunction.h"

#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>

int create_socket(struct sockaddr_in6 *source_addr,
                  int src_port,
                  struct sockaddr_in6 *dest_addr,
                  int dst_port){
    //Create Socket
    int fdsocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if(fdsocket==-1){
        fprintf(stderr, "Error when creating socket\n");
        return -1;
    }

    //Bind socket
    if(source_addr != NULL && src_port > 0){
        source_addr->sin6_port = htons(src_port);
        if(bind(fdsocket, (struct sockaddr *) source_addr, sizeof(struct sockaddr_in6))==-1){
            fprintf(stderr, "Error when binding socket\n");
            return -1;
        }
    }

    //Connect socket
    if(dest_addr != NULL && dst_port > 0){
        dest_addr->sin6_port = htons(dst_port);
        if(connect(fdsocket, (struct sockaddr *) dest_addr, sizeof(struct sockaddr_in6))==-1){
            fprintf(stderr, "Error when connecting socket\n");
            return -1;
        }
    }

    return fdsocket;
}

const char * real_address(const char *address, struct sockaddr_in6 *rval){
    struct addrinfo hints, *res;

    //Set hints
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = 0;
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;
    hints.ai_next = NULL;

    //getadrr and test error
    int e = getaddrinfo(address, NULL, &hints, &res);
    if(e != 0){
        return gai_strerror(e);
    }

    //copy address in rval
    memcpy(rval, res->ai_addr, res->ai_addrlen);

    freeaddrinfo(res);
    return NULL;
}

int wait_for_client(int sfd){
    struct sockaddr_in6 adress;
    socklen_t lenadress = sizeof(struct sockaddr_in6);
    memset(&adress, 0, lenadress);
    int err;

    //recvfrom
    err = recvfrom(sfd, NULL, 0, MSG_PEEK, (struct sockaddr *)&adress, &lenadress);
    if(err == -1){
        return -1;
    }

    //connect
    err = connect(sfd, (struct sockaddr *)&adress, lenadress);
    if(err == -1){
        return -1;
    }
    return 0;
}


//*******************************************************************
//ADD FONCTION FOR RECEIVE
//*******************************************************************
queue_receive* new_queue(){
    queue_receive *nq = (queue_receive *)malloc(sizeof(queue_receive));
    if(nq == NULL){
        fprintf(stderr, "[RECEIVE] Error when malloc a new queue.\n");
        return NULL;
    }
    nq->head = NULL;
    nq->size = 0;
    return nq;
}

node_receive* new_node(int seq, uint32_t stamptime, char* payload, int len){
    node_receive *nnr =  (node_receive *)malloc(sizeof(node_receive));
    if(nnr == NULL){
        fprintf(stderr, "[RECEIVE] Error when malloc a new node.\n");
        return NULL;
    }
    nnr->seqnum = seq;
    nnr->pay = (char *) malloc(len);
    nnr->timestamp = stamptime;
    if(nnr->pay == NULL){
        fprintf(stderr, "[RECEIVE] Error when malloc payload of the new node.\n");
        return NULL;
    }
    memcpy(nnr->pay, payload, len);
    nnr->lenpay = len;
    return nnr;
}

void node_free(node_receive *nodeReceive){
    if(nodeReceive != NULL){
        if(nodeReceive->pay != NULL){
            free(nodeReceive->pay);
        }
        free(nodeReceive);
    }
}

void queue_free(queue_receive *queueReceive){
    if(queueReceive != NULL){
        while(queueReceive->size > 1){
            node_receive* current = queueReceive->head;
            queueReceive->head = queueReceive->head->next;
            node_free(current);
        }
        free(queueReceive);
    }
}

int push_order(queue_receive *queu, node_receive *newnode, int seqwait){
    if(queu->size == 0){
        queu->head = newnode;
        queu->size = 1;
        fprintf(stderr, "[RECEIVE] Push new node in buff.\n");
        return 1;
    }

    int nseq = newnode->seqnum;
    if(nseq >= seqwait){
        nseq = nseq - seqwait;
    }
    else{
        nseq = seqwait + nseq;
    }

    int nexseq;

    //if newnod is the new head
    nexseq = queu->head->seqnum;
    if(nexseq >= seqwait){
        nexseq = nexseq - seqwait;
    }
    else{
        nexseq = seqwait + nexseq;
    }
    if(nseq < nexseq){
        newnode->next = queu->head;
        queu->head = newnode;
        queu->size = queu->size + 1;
        fprintf(stderr, "[RECEIVE] Push new node in buff.\n");
        return 1;
    }
    else if (nseq == nexseq){
        return 0;
    }

    node_receive *before = NULL;
    node_receive *current = queu->head;

    while(current->next != NULL){
        nexseq = current->seqnum;
        if(nexseq >= seqwait){
            nexseq = nexseq - seqwait;
        }
        else{
            nexseq = seqwait + nexseq;
        }
        if(nseq < nexseq){
            before->next = newnode;
            newnode->next = current;
            queu->size = queu->size + 1;
            fprintf(stderr, "[RECEIVE] Push new node in buff.\n");
            return 1;
        }
        else if (nseq == nexseq){
            return 0;
        }
        before = current;
        current = current->next;
    }

    //check last node
    nexseq = current->seqnum;
    if(nexseq >= seqwait){
        nexseq = nexseq - seqwait;
    }
    else{
        nexseq = seqwait + nexseq;
    }
    if(nseq < nexseq){
        before->next = newnode;
        newnode->next = current;
        queu->size = queu->size + 1;
        fprintf(stderr, "[RECEIVE] Push new node in buff.\n");
        return 1;
    }
    else if (nseq == nexseq){
        return 0;
    }
    else{
        current->next = newnode;
        queu->size = queu->size + 1;
        fprintf(stderr, "[RECEIVE] Push new node in buff.\n");
        return 1;
    }
}

node_receive* pop_queue(queue_receive* queueReceive, int seqwait) {
    if(queueReceive->size != 0) {
        if (queueReceive->head->seqnum == seqwait) {
            node_receive *first = queueReceive->head;
            queueReceive->head = queueReceive->head->next;
            queueReceive->size = queueReceive->size - 1;
            return first;

        }
    }
    return NULL;
}
