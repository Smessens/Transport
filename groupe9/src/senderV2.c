#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <time.h>

#include "log.h"
#include "additionalfunction.h"
#include "packet_interface.h"

//Global Variable
int flagNextBuffer = 0;
int flagPktLenZero = 0;
int flagEndOfTransfer = 0;
int seqNumBuf=0;
int maxSeqNum = 255;
int lastSeqNumOfTransfer = -1;

//variable for stat
int stat_data_sent = 0;
int stat_data_received = 0;
int stat_data_truncated_received = 0;
int stat_ack_sent = 0;
int stat_ack_received = 0;
int stat_nack_sent = 0;
int stat_nack_received = 0;
int stat_packet_ignored = 0;
int stat_packet_retransmitted = 0;
int stat_min_rtt = 100;
int stat_max_rtt = 0;

void printstat(FILE *fdstat){
    fprintf(fdstat, "data_sent:%d\n",stat_data_sent);
    fprintf(fdstat, "data_received:%d\n",stat_data_received);
    fprintf(fdstat, "data_truncated_received:%d\n",stat_data_truncated_received);
    fprintf(fdstat, "ack_sent:%d\n",stat_ack_sent);
    fprintf(fdstat, "ack_received:%d\n",stat_ack_received);
    fprintf(fdstat, "nack_sent:%d\n",stat_nack_sent);
    fprintf(fdstat, "nack_received:%d\n",stat_nack_received);
    fprintf(fdstat, "packet_ignored:%d\n",stat_packet_ignored);
    fprintf(fdstat, "packet_retransmitted:%d\n",stat_packet_retransmitted);
    fprintf(fdstat, "min_rtt:%d\n",stat_min_rtt);
    fprintf(fdstat, "max_rtt:%d\n",stat_max_rtt);
}


int print_usage(char *prog_name) {
    ERROR("Usage:\n\t%s [-f filename] [-s stats_filename] receiver_ip receiver_port", prog_name);
    return EXIT_FAILURE;
}

int printStringNChar(char * string, int n){
    if(string == NULL){
        fprintf(stderr,"[SENDER] Error in printStringNChar(NULL)\n");
        return 0;
    }
    int c = 0;
    for (; (string[c] != '\0') && (c<n); c++){
        printf("%c", string[c]);
    }
    fprintf(stderr,"\n");
    return c;
}

int printDump(char * bufEncode, int len){
    //PRINT BUF
    uint8_t *printest = (unsigned char *)bufEncode;
    dump(printest, len);
    return len;
}


struct bufWindow{
    char* tableau[33];
    size_t tabOfSize[33];
    int next;
    int size;
    int sizeMax;
};

int getTimeMilli(){
    struct timespec *packettime = malloc(sizeof(struct timespec));
    if(packettime == NULL){
        fprintf(stderr, "[SENDER] : error when malloc packetime.\n");
        return 0;
    }
    clock_gettime(CLOCK_REALTIME, packettime);
    time_t seconde = packettime->tv_sec;
    time_t nanoseconde = packettime->tv_nsec;
    free(packettime);
    return seconde * 1000 + nanoseconde/1000000;
}

/**
 * Insert a string into the struct.
 * @pre    isEmpty(bufW) > 0
 * @return new size
 */
int insert(struct bufWindow *bufW, char* pkt, size_t pktSize){
    bufW->tableau[(bufW->next+bufW->size)%bufW->sizeMax] = pkt;
    bufW->tabOfSize[(bufW->next+bufW->size)%bufW->sizeMax] = (int) pktSize;
    bufW->size++;
    return bufW->size;
}
/*
 * This is not a "isEmpty" is more a "sizeof" !
 */
int isEmpty(struct bufWindow bufW){
    return bufW.size;
}

int initBufWindow(struct bufWindow *bufWindow){
    bufWindow->sizeMax = 33;
    bufWindow->size = 0;
    bufWindow->next = 0;
    for (int i = 0; i<bufWindow->sizeMax;i++){
        bufWindow->tableau[i]=NULL;
    }
    return 1;
}

int clearN(struct bufWindow *bufW, int n){
    if(n == 0)return 0;

    for(int i = 0; i < n; i++){
        free(bufW->tableau[(bufW->next+i)%bufW->sizeMax]);
    }
    bufW->size = bufW->size - n;
    bufW->next = (bufW->next+n)%bufW->sizeMax;
    return n;
}
/*
 * return the n° element of the bufW and set the size on sizepkt.
 * NB : this is in like a tab, so the first element start at n = 0
 */
char* seekN(struct bufWindow *bufW, size_t *sizepkt, int n){
    if (bufW->size<n){
        return NULL;
    }
    *sizepkt = bufW->tabOfSize[(bufW->next + n)%bufW->sizeMax];
    return bufW->tableau[((bufW->next)+n)%bufW->sizeMax];
}
/*
 * return the addr of a string contening the next 512 bytes of the file and update the len value to corresponding with the len of the buffer
 */
char* nextBuff(int fd,size_t* len){
    if(flagNextBuffer>0){
        return NULL;
    }
    char *buf = (char *) malloc(512 * sizeof(char));
    if(buf == NULL){
        printf("[SENDER] Malloc failed in NextBuff()\n");
        return NULL;
    }
    size_t readSize_t = read(fd,buf,512);
    if(readSize_t<1){//failure fread
        *len = 0;
        fprintf(stderr,"[SENDER] fread FAILURE in nextBuff\n");
        return NULL;
    }
    if(readSize_t<512){
        flagNextBuffer = 1;
    }
    *len = readSize_t;
    return buf;
}

/*
 * Send n pkt contienning in bufWindow
 * return the number of pkt sent on the sfd file descriptor.
 */
int sendData(int sfd, int n, struct bufWindow *bufWindow, size_t *len ){
    pkt_t *pktPrint = pkt_new();

    for(int i = 0;i< n ; i++){
        void *seek = seekN(bufWindow,len,i);
        if(seek == NULL){
            //fprintf(stderr,"[SENDER] SeekN return NULL\n");
            break;
        }
        //Update the timestamp value
        pkt_decode(seek,*len,pktPrint);

        pkt_set_timestamp(pktPrint,getTimeMilli());
        pkt_encode(pktPrint,seek,len);
        //End of the Timestamp update

        int errno = write(sfd,seek,*len);
        if(errno < 0 ){
            fprintf(stderr,"[SENDER] Write Failure (errno < 0 )\n");
            return -1;
        }
        fprintf(stderr,"*********************************************************************\n");
        fprintf(stderr, "[SENDER] Send a packet : \n");
        printpacket(pktPrint);
        fprintf(stderr,"*********************************************************************\n");
        stat_data_sent++;


    }
    pkt_del(pktPrint);
    return 1;
}
/*
 * return the number of pkt add in the bufWindow
 */
int feedBufWindow(int fd, struct bufWindow *bufWindow, struct pkt *pkt){
    // add missing pkt in the window buffer

    char *buf;
    size_t a= 528;
    size_t *len=&a;
    size_t size = 528;
    int i;
    for(i=0;(0<bufWindow->sizeMax - bufWindow->size)&&(size==528);i++){//remplissage du bufWindow
        buf = nextBuff(fd, len);
        if(buf == NULL){
            if (flagPktLenZero==0){//pkt len Zero not insert yet
                flagPktLenZero++;
                pkt_set_length(pkt, 0);
                pkt_set_type(pkt,PTYPE_DATA);
                pkt_set_seqnum( pkt, seqNumBuf);
                seqNumBuf=(seqNumBuf+1)%(maxSeqNum+1);
                lastSeqNumOfTransfer = seqNumBuf;
                pkt_set_tr(pkt,0);
                pkt_set_window(pkt, 31);
                char * bufEncode = malloc(528);
                size = 528;
                pkt_status_code statusCode = pkt_encode((const pkt_t*)pkt,bufEncode,&size);
                if(statusCode != PKT_OK){
                    fprintf(stderr, "[SENDER] Error when encode packet in feedbuffer for the pkt len = 0: %d\n", statusCode);
                }
                insert(bufWindow,bufEncode,size);
            }
            break;
        }
        pkt_set_length(pkt, *len);
        pkt_set_payload(pkt,buf,*len);
        pkt_set_type(pkt,PTYPE_DATA);
        pkt_set_seqnum( pkt, seqNumBuf);
        seqNumBuf=(seqNumBuf+1)%(maxSeqNum+1);
        pkt_set_tr(pkt,0);
        pkt_set_window(pkt, 31);

        pkt_set_timestamp(pkt, getTimeMilli());

        char * bufEncode = malloc(528);
        size = 528;
        pkt_status_code statusCode = pkt_encode((const pkt_t*)pkt,bufEncode,&size);
        if(statusCode != PKT_OK){
            fprintf(stderr, "[SENDER] Error when encode packet in feedbuffer : %d\n", statusCode);
        }
        free(buf);
        insert(bufWindow,bufEncode,size);
    }
    if(fd <= 1) close(fd);

    return i;
}
/*
 * return the difference of seqNum and seqNumReceiveUint8 corresponding to the protocol TRTP for the seqNum
 */
int isInRange(int seqNum,uint8_t seqNumReceiveUint8){
    int seqNumReceive= seqNumReceiveUint8;
    if(seqNumReceive > seqNum){
        if(seqNum + 32 > seqNumReceive){
            return seqNumReceive-seqNum;
        }
        else{
            return 0;
        }
    }
    else if( ( (seqNum + 32)%(maxSeqNum+1) )>seqNumReceive){
        return  (seqNumReceive +(maxSeqNum+1)) - seqNum;
    } else{
        return 0;
    }
}



int main(int argc, char **argv) {
    // Global Variable
    int opt;
    char *filename = NULL;
    char *stats_filename = NULL;
    char *receiver_ip = NULL;
    char *receiver_port_err;
    uint16_t receiver_port;


    // Getopt to manage argument input

    while ((opt = getopt(argc, argv, "f:s:h")) != -1) {
        switch (opt) {
        case 'f':
            filename = optarg;
            break;
        case 'h':
            return print_usage(argv[0]);
        case 's':
            stats_filename = optarg;
            break;
        default:
            return print_usage(argv[0]);
        }
    }

    if (optind + 2 != argc) {
        ERROR("Unexpected number of positional arguments");
        return print_usage(argv[0]);
    }

    receiver_ip = argv[optind];
    receiver_port = (uint16_t) strtol(argv[optind + 1], &receiver_port_err, 10);
    if (*receiver_port_err != '\0') {
        ERROR("Receiver port parameter is not a number");
        return print_usage(argv[0]);
    }

    //resolve receiver_ip
    struct sockaddr_in6 addr;
    const char *err = real_address(receiver_ip, &addr);
    if (err) {
        fprintf(stderr, "Could not resolve receiver_ip %s: %s\n", receiver_ip, err);
        return EXIT_FAILURE;
    }

    //create socket
    int sfd = create_socket(NULL, -1, &addr, receiver_port);
    if (sfd < 0) {
        fprintf(stderr, "[SENDER] Failed to create the socket!\n");
        close(sfd);
        return EXIT_FAILURE;
    }

    int fd;
    if(filename != NULL){ // Case if -f filename is in input argument
        fd = open((char *)filename,O_RDONLY); // open filename in read only mode
        if(fd == -1){ // test fopen failure
            fprintf( stderr,"[SENDER] Cannot open file %s\n", filename );
            return -1;
        }
    }
    else{ // no file to send, we have to send the content on the standart input
        fd = STDIN_FILENO;
    }
    pkt_t *pkt = pkt_new();
    char *buf = NULL;
    size_t a = 528;
    size_t *len= &a;
    int lastSeqNumAck=0;
    struct bufWindow bufWindow1;
    struct bufWindow *bufWindow=&bufWindow1;
    initBufWindow(bufWindow);

    int loop = 1;

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    fd_set setread;


    int lenread;
    pkt_t *rec_pkt;
    char pktread[528];
    pkt_status_code rec_status;
    int numberWindow = 1;
    int flagSendData = 1;
    int timeoutCount = 0;


    int numberOfAck = 1;
    int maxFd;


    //feedBuffer at the beginning
    feedBufWindow(fd, bufWindow, pkt);
    //Send the first Data
    sendData(sfd,1,bufWindow,len);


    while((loop == 1 || isEmpty(bufWindow1)>0)&& (timeoutCount < 5)){
        FD_ZERO(&setread);
        FD_SET(sfd, &setread);

        maxFd=sfd+1;

        // Wait response or Timeout
        int selectInt = select(maxFd, &setread, NULL, NULL, &tv);
        if(selectInt == 0){
            timeoutCount++;
            tv.tv_sec = 5;
            fprintf(stderr,"[SENDER] Timeout in Main, timeoutCount = %d\n",timeoutCount);
        }
        else{
            timeoutCount = 0;
        }
        // manage the response
        if(FD_ISSET(sfd, &setread)) {
            lenread = read(sfd, (void *) pktread, 528);
            if (lenread == EOF || lenread == 0) {
                loop = 0;
            }
            else {
                //decode packet
                rec_pkt = pkt_new();
                rec_status = pkt_decode((const char *) pktread, (int) lenread, rec_pkt);
                fprintf(stderr,"*********************************************************************\n");
                fprintf(stderr,"[SENDER] Packet receive : \n");
                printpacket(rec_pkt);
                fprintf(stderr,"*********************************************************************\n");
                if ((rec_status == PKT_OK) && (pkt_get_type((const pkt_t*)rec_pkt) == PTYPE_ACK)) { // The basic case ( pkt valid & ACK type )
                    stat_ack_received++;

                    if(pkt_get_seqnum(rec_pkt) == lastSeqNumOfTransfer){
                        flagEndOfTransfer = 1;
                        fprintf(stderr,"[SENDER] Receive the Ack for the last pkt.\n");
                        time_t ping = getTimeMilli() - pkt_get_timestamp(rec_pkt);

                        //for stat
                        if(ping > stat_max_rtt){
                            stat_max_rtt = ping;
                        }
                        if(ping < stat_min_rtt){
                            stat_min_rtt = ping;
                        }
                        break;
                    }

                    // find the number of pkt ack
                    int diffAck;
                    if(isInRange(lastSeqNumAck,pkt_get_seqnum((const pkt_t*)rec_pkt))>0){ // test if the seqnum receive is in range of window
                        diffAck = isInRange(lastSeqNumAck,pkt_get_seqnum((const pkt_t*)rec_pkt));
                    } else{ // ACK receive out of range window => invalid PKT
                        diffAck = 0;
                    }

                    numberOfAck = diffAck;
                    lastSeqNumAck=(lastSeqNumAck+numberOfAck)%(maxSeqNum+1);
                    // compute the ping

                    time_t ping = getTimeMilli() - pkt_get_timestamp(rec_pkt);

                    //for stat
                    if(ping > stat_max_rtt){
                        stat_max_rtt = ping;
                    }
                    if(ping < stat_min_rtt){
                        stat_min_rtt = ping;
                    }

                    //tv.tv_usec = 3 * stat_max_rtt * 1000;

                    clearN(bufWindow,numberOfAck);
                    // add missing pkt in the window buffer
                    //lastSeqNum= (lastSeqNum + feedBufWindow(fd,bufWindow, pkt, lastSeqNum))%maxSeqNum;
                    feedBufWindow(fd,bufWindow,pkt);

                    numberWindow = pkt_get_window(rec_pkt);


                }
                else if((rec_status == PKT_OK) && (pkt_get_type((const pkt_t*)rec_pkt) == PTYPE_NACK)){
                    stat_nack_received++;
                    // have to resend all packet able to be send in window range
                    if((pkt_get_seqnum(pkt)>=lastSeqNumAck||pkt_get_seqnum(pkt)<=(lastSeqNumAck+bufWindow->sizeMax)%maxSeqNum)){ // NACK in Window Range
                        // send nbr of window readed in the pkt begin from the seqnum
                        sendData(sfd,pkt_get_window(rec_pkt),bufWindow,len);
                        stat_packet_retransmitted+= pkt_get_window(rec_pkt);

                    }
                }
                else{
                    stat_packet_ignored++;
                    fprintf(stderr,"[SENDER] Receive a invalid packet\n");
                }
                pkt_del(rec_pkt);
            }
            flagSendData = 1;
        }
        else{ // when we must send a pkt because FD is chosen or
            feedBufWindow(fd, bufWindow, pkt);

        }
        if( (timeoutCount>0) || (flagSendData>0)){// Timeout case
            int retransmit = sendData(sfd, numberWindow, bufWindow, len);
            if(timeoutCount>0){
                stat_packet_retransmitted+= retransmit;
            }
            flagSendData = 0;
        }

        // to finish send a PTYPE_DATA with length = 0 with the good seqnum
        /*
         * if(*len <528){// test if the last pkt was the end of the file
            //send a PTYPE_DATA with length = 0 with the good seqnum
            buf =(char *) malloc(528 * sizeof(char));
            if(buf == NULL){
                fprintf(stderr,"[SENDER] Malloc error \n");
            }

            pkt = pkt_new();
            pkt_set_type(pkt,PTYPE_DATA);
            pkt_set_length(pkt,0);
            pkt_set_seqnum(pkt, seqNumBuf);
            pkt_set_timestamp(pkt,getTimeMilli());
            *len = 528;
            fprintf(stderr,"*********************************************************************\n");
            fprintf(stderr,"[SENDER] Send a packet :\n");
            printpacket(pkt);
            fprintf(stderr,"*********************************************************************\n");
            pkt_status_code statusCode = pkt_encode(pkt, buf, len);
            if (statusCode>0) fprintf(stderr,"[SENDER] Status code : %d \n",statusCode);
            if(isEmpty(bufWindow1)<bufWindow1.sizeMax) insert(bufWindow,buf,*len);
            }
         */

            /*
              int errno2 = sendData(sfd, 1, bufWindow, len);
            if(errno2 < 0 ){
                fprintf(stderr,"[SENDER] Write Failure (errno < 0 )\n");
                return -1;
            }
             */



    }

    /*
     * "3way and shake"
     * s'assurer que le receiver ai reçu le length = 0 en sequence ( ou timeout ) et puis close
     */

    fprintf(stderr,"End of the first loop.\n");
    fprintf(stderr,"(loop [%d] == 1 || isEmpty(bufWindow1) [%d]>0)&& (timeoutCount[%d] < 5) \n",loop,isEmpty(bufWindow1),timeoutCount);

    int AckReceive=0;
    //timeoutCount = 0; // reset the timeoutCount


    while(AckReceive == 0&&timeoutCount<5){

        if(flagEndOfTransfer > 0){
            break;
        }

        FD_ZERO(&setread);
        FD_SET(sfd, &setread);

        select(sfd+1, &setread,NULL, NULL, &tv);
        if(tv.tv_usec == 0){
            fprintf(stderr,"[SENDER] Timeout : No Ack receive.!\n");
            timeoutCount++;
        }
        if(buf == NULL){
            buf = malloc(528);
        }
        int errorSendData = sendData(sfd, 1, bufWindow, len);
        if(errorSendData <1)
        {
            fprintf(stderr, "[SENDER] Error when send last packet (len == 0).\n");
            break;
        }
        if(FD_ISSET(sfd, &setread)) {
            int length = read(sfd, (void *)pktread, 528);

            if(pkt_decode((const char *)pktread,(size_t )length,pkt) == PKT_OK){
                if(pkt_get_seqnum(pkt) == (seqNumBuf+1)%(maxSeqNum+1)){ // check if the pkt receive have the good seqNum
                    fprintf(stderr, "[SENDER] Receive last ack for end of the transfer.\n");

                    AckReceive = 1;
                }

            }
        }

    }
    pkt_del(pkt);

    FILE *ffd;
    if(stats_filename != NULL){
        ffd = fopen(stats_filename,"w");
        if (ffd==NULL){
            fprintf(stderr,"[SENDER] Could not open %s.\n", stats_filename);
        }
    }
    else{
        ffd = stderr;
    }

    fprintf(stderr, "[SENDER] Print statistics.\n");
    printstat(ffd);

    close(sfd);
    fprintf(stderr,"[SENDER] EXIT_SUCCESS\n");
    return EXIT_SUCCESS;
}