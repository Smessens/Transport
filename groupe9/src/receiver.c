#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "additionalfunction.h"
#include "packet_interface.h"

//variable for stat
int stat_data_sent;
int stat_data_received;
int stat_data_truncated_received;
int stat_ack_sent;
int stat_ack_received;
int stat_nack_sent;
int stat_nack_received;
int stat_packet_ignored;
int stat_packet_duplicated;



queue_receive* bufreceive;


int print_usage(char *prog_name) {
    ERROR("Usage:\n\t%s [-s stats_filename] listen_ip listen_port", prog_name);
    return EXIT_FAILURE;
}

void printstat(FILE *fdstat){
    fprintf(fdstat, "data_sent:%d\n",stat_data_sent);
    fprintf(fdstat, "data_received:%d\n",stat_data_received);
    fprintf(fdstat, "data_truncated_received:%d\n",stat_data_truncated_received);
    fprintf(fdstat, "ack_sent:%d\n",stat_ack_sent);
    fprintf(fdstat, "ack_received:%d\n",stat_ack_received);
    fprintf(fdstat, "nack_sent:%d\n",stat_nack_sent);
    fprintf(fdstat, "nack_received:%d\n",stat_nack_received);
    fprintf(fdstat, "packet_ignored:%d\n",stat_packet_ignored);
    fprintf(fdstat, "packet_duplicated:%d\n",stat_packet_duplicated);
}


//write in std, remove in the buffer
//return last seqnum writed, -1 if nothing writed
int write_rec(int seq){
    int err;
    int ret = -1;
    node_receive* popfirst;
    popfirst = pop_queue(bufreceive, seq);
    while(popfirst != NULL){
        err = write(STDOUT_FILENO, popfirst->pay, popfirst->lenpay);
        if(err < 0){
            fprintf(stderr, "RECEIVER : ERROR WRITING PAYLOAD\n");
            return ret;
        }
        fprintf(stderr, "[RECEIVE] The payload of seqnum %d is writed.\n", seq);
        ret = seq;
        if(seq == 255){
            seq = 0;
        }
        else{
            seq++;
        }
        node_free(popfirst);
        popfirst = pop_queue(bufreceive, seq);
    }
    return ret;
}

//function add the receive pkt in buffer
//return : the last segnum writed, -1 if no payload writed
int addbuffer(int seqlast, int seqrec, int win, pkt_t* pktrec){
    int seqw;
    if(seqlast == 255){
        seqw = 0;
    }
    else{
        seqw = seqlast+1;
    }

    node_receive *nn;
    nn = new_node(seqrec, pkt_get_timestamp((const pkt_t*)pktrec), (char *)pkt_get_payload((const pkt_t*)pktrec), (int) pkt_get_length((const pkt_t*)pktrec));
    int addbuf;
    int ret;
    //if the seq received equal the seq waited
    if(seqrec == seqlast+1 || (seqrec == 0 && seqlast == 255)){
        fprintf(stderr, "[RECEIVE] The seqnum received is the seqnum waited.\n");
        addbuf = push_order(bufreceive, nn, seqw);
        fprintf(stderr, "[RECEIVE] The payload of seqnum %d is add in the buffer.\n", seqrec);
        ret = write_rec(seqw);
        return ret;
    }
        //if seqnum receive is in windows
    else if ((seqrec > seqlast && seqrec <= seqlast + win) || (seqlast+win > 256 && seqrec <=(seqlast+win)%256) ){
        fprintf(stderr, "[RECEIVE] The seqnum received is in the window.\n");
        addbuf = push_order(bufreceive, nn, seqw);
        if(addbuf == 0){
            fprintf(stderr, "[RECEIVE] The payload is already in the buffer.\n");
            stat_packet_duplicated++;
            node_free(nn);
        }
        else{
            fprintf(stderr, "[RECEIVE] The payload of seqnum %d is add in the buffer.\n", seqrec);
        }
        return seqlast;
    }
    node_free(nn);
    return -1;
}

//Send ACK/NACK in SENDER
//return -1 if it fails, 0 otherwise
void send_ack(pkt_t* pack, int socketfd){
    char buffer_ack[32];
    size_t  len_buffer_ack = 32;
    pkt_status_code ack_encode = pkt_encode(pack, buffer_ack, &len_buffer_ack);
    if(ack_encode != PKT_OK){
        fprintf(stderr, "Receive : error encode ACK/NACK\n");
    }
    if(write(socketfd, buffer_ack, len_buffer_ack) < 0){
        fprintf(stderr, "Receive : error write ack\n");
    }


    fprintf(stderr, "*********************************************************************\n");
    fprintf(stderr, "[Receive] SEND ACK/NACK :\n");
    printpacket(pack);
    fprintf(stderr, "*********************************************************************\n");

    pkt_del(pack);
}



void receive_data(int sockfd){
    //for stat
    stat_data_sent = 0;
    stat_data_received = 0;
    stat_data_truncated_received = 0;
    stat_ack_sent = 0;
    stat_ack_received = 0;
    stat_nack_sent = 0;
    stat_nack_received = 0;
    stat_packet_ignored = 0;
    stat_packet_duplicated  = 0;

    //retransmission time
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    //init the buffer
    bufreceive = new_queue();
    fprintf(stderr, "[RECEIVE] The receive buffer is init.\n");

    pkt_t* rec_pkt;
    pkt_t* ack_pkt;
    pkt_status_code rec_status;
    char pktread[528];
    fd_set setread;
    int seqlast = 255;
    int seqreceived;
    int wind = 1;
    //int senderwin = 0;

    struct timespec lasttime;
    int errtime = clock_gettime(CLOCK_MONOTONIC, &lasttime);
    if(errtime != 0){
        fprintf(stderr, "[RECEIVER] Error with timer.\n");
    }

    int loop = 0;
    while(loop == 0){
        FD_ZERO(&setread);
        FD_SET(sockfd, &setread);

        select(sockfd+1, &setread, NULL, NULL, &tv);

        struct timespec thistime;
        errtime = clock_gettime(CLOCK_MONOTONIC, &thistime);
        if(errtime != 0){
            fprintf(stderr, "[RECEIVER] Error with timer.\n");
        }
        //check timeout transmission
        int timeout = 10;
        if(thistime.tv_sec - lasttime.tv_sec > timeout){
            fprintf(stderr, "[RECEIVER] Disconnect beacause no transmission since %d s.\n", timeout);
            loop = 1;
        }

        //wind = 31-bufreceive->size;


        //We receive packet
        if(FD_ISSET(sockfd, &setread)) {

            fprintf(stderr, "[RECEIVE] We receive data.\n");
            int lenread = read(sockfd, (void *) pktread, 528);
            fprintf(stderr, "[RECEIVE] We read %d byte(s).\n", lenread);
            if(lenread == -1){
                fprintf(stderr, "[RECEIVE] error read : %d\n", errno);
            }
            else if(lenread == EOF || lenread == 0) {
                fprintf(stderr, "[RECEIVE] End of the transfer.\n");
                loop = 1;
            }
            else {
                //MAJ lasttime
                errtime = clock_gettime(CLOCK_MONOTONIC, &lasttime);
                if(errtime != 0){
                    fprintf(stderr, "[RECEIVER] Error with timer.\n");
                }

                //decode packet
                rec_pkt = pkt_new();
                rec_status = pkt_decode((const char *) pktread, (int) lenread, rec_pkt);
                if ((rec_status == PKT_OK) && (pkt_get_type(rec_pkt) == PTYPE_DATA)) {
                    stat_data_received++;

                    fprintf(stderr, "*********************************************************************\n");
                    fprintf(stderr, "[RECEIVE] The data received is a correct packet DATA TYPE :.\n");
                    printpacket(rec_pkt);
                    fprintf(stderr, "*********************************************************************\n");



                    //Send NACK when receive truncated packet
                    if (pkt_get_tr((const pkt_t*) rec_pkt) == 1) {
                        fprintf(stderr, "[RECEIVE] The data received is a truncated packet.\n");
                        stat_data_truncated_received++;

                        ack_pkt = pkt_new();
                        pkt_set_type(ack_pkt, PTYPE_NACK);
                        pkt_set_seqnum(ack_pkt, pkt_get_seqnum((const pkt_t*) rec_pkt));
                        pkt_set_timestamp(ack_pkt, pkt_get_timestamp((const pkt_t*) rec_pkt));
                        pkt_set_window(ack_pkt, wind);

                        send_ack(ack_pkt, sockfd);
                        stat_nack_sent++;
                    }
                    else {
                        seqreceived = pkt_get_seqnum((const pkt_t*) rec_pkt);
                        fprintf(stderr, "[RECEIVE] Seqnum received : %d.\n", seqreceived);
                        fprintf(stderr, "[RECEIVE] The last seq that i m write is : %d.\n", seqlast);

                        int seqw;
                        if(seqlast == 255){
                            seqw = 0;
                        }
                        else{
                            seqw = seqlast + 1;
                        }
                        //If length == 0 --> END OF TRANSFER
                        if (pkt_get_length((const pkt_t*) rec_pkt) == 0 && seqreceived==seqw) {
                            //Send ACK
                            fprintf(stderr, "[RECEIVE] The data received have a 0 len so end of the transfer.\n");
                            ack_pkt = pkt_new();

                            int seqa;
                            if(seqreceived == 255){
                                seqa = 0;
                            }else{
                                seqa = seqreceived + 1;
                            }
                            pkt_set_seqnum(ack_pkt, (const uint8_t) seqa);
                            pkt_set_timestamp(ack_pkt, pkt_get_timestamp((const pkt_t*) rec_pkt));
                            pkt_set_type(ack_pkt, PTYPE_ACK);
                            pkt_set_window(ack_pkt, wind);

                            send_ack(ack_pkt, sockfd);
                            stat_ack_sent++;
                            loop = 1;
                            fprintf(stderr, "[RECEIVE] SEND ACK FOR END OF TRANSFER (Seqnum : %d).\n", seqreceived);
                        }
                        else{
                            //senderwin = pkt_get_window((const pkt_t*) rec_pkt);

                            //ADD dans le buffer

                            int seqack = addbuffer(seqlast,seqreceived, wind, rec_pkt);
                            if(seqack != -1){
                                //Send ACK
                                ack_pkt = pkt_new();
                                seqlast = seqack;
                                if(seqlast== 255){
                                    seqack = 0;
                                }
                                else{
                                    seqack++;
                                }
                                pkt_set_seqnum(ack_pkt, (const uint8_t) seqack);
                                pkt_set_timestamp(ack_pkt, pkt_get_timestamp((const pkt_t*) rec_pkt));
                                pkt_set_type(ack_pkt, PTYPE_ACK);
                                pkt_set_window(ack_pkt, wind);

                                send_ack(ack_pkt, sockfd);
                                fprintf(stderr, "[Receive] SEND ACK DONE\n");
                                stat_ack_sent++;
                            }
                            else{
                                fprintf(stderr, "[RECEIVE] The packet received cannot be add in the buffer.\n");
                            }
                        }
                    }
                }
                else{
                    fprintf(stderr, "[RECEIVE] The data received is NOT a correct packet DATA TYPE.\n");
                    stat_packet_ignored++;
                }
                pkt_del(rec_pkt);
            }
        }
    }
    //end of transfer --> free buffer
    queue_free(bufreceive);
    fprintf(stderr, "[RECEIVE] FREE BUFFER.\n");
}


int main(int argc, char **argv) {
    int opt;

    char *stats_filename = NULL;
    char *listen_ip = NULL;
    char *listen_port_err;
    uint16_t listen_port;

    while ((opt = getopt(argc, argv, "s:h")) != -1) {
        switch (opt) {
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

    listen_ip = argv[optind];
    listen_port = (uint16_t) strtol(argv[optind + 1], &listen_port_err, 10);
    if (*listen_port_err != '\0') {
        ERROR("Receiver port parameter is not a number");
        return print_usage(argv[0]);
    }

    // This is not an error per-se.
    ERROR("Receiver has following arguments: stats_filename is %s, listen_ip is %s, listen_port is %u",
          stats_filename, listen_ip, listen_port);

    DEBUG("You can only see me if %s", "you built me using `make debug`");

    //resolve listen_ip
    struct sockaddr_in6 addr;
    const char *err = real_address(listen_ip, &addr);
    if (err) {
        fprintf(stderr, "Could not resolve listen_ip %s: %s\n", listen_ip, err);
        return EXIT_FAILURE;
    }
    fprintf(stderr, "[RECEIVE] Have real listen adress.\n");

    //create socket
    int sfd = create_socket(&addr, listen_port, NULL, -1);
    if (sfd < 0) {
        fprintf(stderr, "Receiver : Failed to create the socket!\n");
        close(sfd);
        return EXIT_FAILURE;
    }
    fprintf(stderr, "[RECEIVE] Create socket.\n");
    if (sfd > 0 && wait_for_client(sfd) < 0) { /* Connected */
        fprintf(stderr,"Could not connect the socket.\n");
        close(sfd);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "[RECEIVE] A client is connected.\n");

    receive_data(sfd);

    //Filename ?
    FILE *ffd;
    if(stats_filename != NULL){
        ffd = fopen(stats_filename,"w");
        if (ffd==NULL){
            fprintf(stderr,"Could not open %s.\n", stats_filename);
        }
    }
    else{
        ffd = stderr;
    }

    fprintf(stderr, "[RECEIVE] Print statistics.\n");
    //PRINT STATS
    printstat(ffd);


    close(sfd);
    fprintf(stderr, "[RECEIVE] End.\n");
    return EXIT_SUCCESS;
}