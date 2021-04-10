#include "packet_interface.h"
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
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
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "log.h"
#define true 1
#define false 0

int setwindow = 31; // size of the window
int window = 0;

bool LOG = false; // set to true if you activate the -l option this option allows to display the print that are useful for debugging the program

// these variables are used to output statistics from our program
long DATA_SENT = 0; //
long DATA_RECEIVED = 0;
long DATA_TRUNCATED = 0;
long ACK_SENT = 0;
long ACK_RECEIVED = 0;//
long NACK_SENT = 0;
long NACK_RECEIVED = 0; //
long PACKET_IGNORED = 0;
long PACKET_DUPLICATED = 0;

// This function is called in case of wrong argument in the execution command of the function
int print_usage(char *prog_name) {
    ERROR("Usage:\n\t%s [-s stats_filename] listen_ip listen_port", prog_name);
    return EXIT_FAILURE;
}

/*
* This function create a struct sockaddr_in6 which contains
* the IPV6 address of the sender
* @sender_ip -> IPV6 address of the sender
* @addr -> structure with the address of the sender
* RETURN 1 if succeed, 0 if fail
*/
int real_address(char * sender_ip, struct sockaddr_in6 * addr){
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  hints.ai_flags = 0;
  struct addrinfo * res;
  int err = getaddrinfo(sender_ip, NULL, &hints, &res);
  if(err!=0){
    return 0;
  }
  struct sockaddr_in6 *result = (struct sockaddr_in6 *) (res->ai_addr);
  *addr = *result;
  freeaddrinfo(res);
  return 1;
}

/*
* This function create and connect the socket which will be use
* between the sender and the receiver
* @sender_addr -> structure socaddr of the sender
* @sender_port -> The port on which the sender and the receiver communicate
* RETURN the socket if succeed, 0 if the creeation of the socket failed
* and -1 if the connection failed
*/
int create_socket(struct sockaddr_in6 *sender_addr, int sender_port){
  int sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP); // create a socket using IPv6 addresses
  if (sock == -1) {
    return 0;
  }
  if(sender_addr!=NULL && sender_port>0){
    sender_addr->sin6_port = htons(sender_port);
    int err = bind(sock,(struct sockaddr *) sender_addr,sizeof(struct sockaddr_in6));
    if(err==-1){
      return -1;
    }
  }
  return sock;
}

/*
* This function create and send a nack to the sender
* @ pkt -> pkt that has been received
* @ sfd -> the socket in wich the nack must be
* RETURN void
*/
void nack(pkt_t *pkt, int sfd){
  NACK_SENT++;
  pkt_t* nack_pkt = pkt_new(); // pkt to send the nack
  if(pkt_set_type(nack_pkt,3)!=PKT_OK) ERROR("set pkt type");
  if(pkt_set_tr(nack_pkt,0)!=PKT_OK) ERROR("set pkt tr");
  if(pkt_set_window(nack_pkt,window)!=PKT_OK) ERROR("set pkt window");
  if(pkt_set_seqnum(nack_pkt,pkt_get_seqnum(pkt))!=PKT_OK) ERROR("set seqnum");
  if(pkt_set_timestamp(nack_pkt,pkt_get_timestamp(pkt))!=PKT_OK) ERROR("set timestamp");
  char * bufpkt = (char*) malloc(10); // the buffer in wich the nack will be stored for the transfert
  memset((void*)bufpkt,0,10);
  size_t len =  strlen(bufpkt);
  if(pkt_encode(nack_pkt,bufpkt,&len)!=PKT_OK) ERROR("set encode");
  if(write(sfd, bufpkt, len)==-1){ // we send the nack to the sender
    ERROR("The function write dont work in the receiver");
  }
  free(bufpkt);
  pkt_del(nack_pkt);
}

/*
* This function create and send an ack to the sender
* @ pkt -> pkt that has been received
* @ sfd -> the socket in wich the nack must be
* RETURN void
*/
void ack(pkt_t *receive_pkt, int sfd, int seq){
  ACK_SENT++;
  pkt_t* ack_pkt = pkt_new(); // pkt to send the nack
  if(pkt_set_type(ack_pkt,2)!=PKT_OK) ERROR("set pkt type");
  if(pkt_set_tr(ack_pkt,0)!=PKT_OK) ERROR("set pkt tr");
  if(pkt_set_window(ack_pkt,window)!=PKT_OK) ERROR("set pkt window");
  if(pkt_set_seqnum(ack_pkt,seq)!=PKT_OK) ERROR("set seqnum");
  if(pkt_set_timestamp(ack_pkt,pkt_get_timestamp(receive_pkt))!=PKT_OK) ERROR("set timestamp");
  char * bufpkt = (char*) malloc(10); // the buffer in wich the nack will be stored for the transfert
  memset((void*)bufpkt,0,10);
  size_t len =  strlen(bufpkt);
  if(pkt_encode(ack_pkt,bufpkt,&len)!=PKT_OK) ERROR("set encode");
  if(write(sfd, bufpkt, len)==-1){ // we send the nack to the sender
    ERROR("The function write dont work in the receiver");
  }
  free(bufpkt);
  pkt_del(ack_pkt);
}

/*
* This function will loop until the last message is received or we haven't received a
* single message from the sender for 30 seconds
* @ sfd -> the socket in wich the nack must be
* RETURN void
*/
void read_loop(const int sfd){
  bool firsttime = true; // the first message is received
  fd_set rfds;
  char * buf = (char*)malloc(528); // this buffer is used to copy de buffer sent by the sender
  pkt_t * pkt;
  pkt_t * pktsearch;
  struct sockaddr_in6 peeraddress;
  socklen_t peer_addr_len = sizeof(struct sockaddr_in6);
  memset(&peeraddress,0,peer_addr_len);
  struct Queue * dataReceive;
  dataReceive = createQueue(); // store packages that do not arrive in sequence
  int seqnum = 0; // the sequence number expected by the receiver
  // this will loop until the last message is received or we haven't received a single message from the sender for 30 seconds
  while(1){
    struct timeval tv = {30,0};
    memset((void*) buf, 0, 528);
    // we set the select to look at the file descriptor of the socket
    FD_ZERO(&rfds);
    FD_SET(sfd, &rfds);
    int retval = select(sfd+1, &rfds, NULL, NULL, &tv);

    if (retval == -1){ //error function select
      ERROR("select()");
      return;
    }

    if(retval==0) { // we haven't received a single message from the sender for 30 seconds
      if(LOG)ERROR("taille de la window a la fin du programme quand tv expire: %d = window au depart: %d",window, setwindow);
      if(LOG)ERROR("taille de la queue qui doit etre 0: %d", getSize(dataReceive));
      printSeq(getFront(dataReceive));
      break;
    }

    if(FD_ISSET(sfd,&rfds)){ // read the message that as been arrived ont the socket
        pkt= pkt_new();
        ssize_t recv = recvfrom(sfd,buf,528,0,(struct sockaddr *) &peeraddress, &peer_addr_len); // we copy the message on the socket in the buffer "buf"

        if(recv==-1){ // error read
          ERROR("receive from");
        }

        if(firsttime){ // first message received from this sender
          int connection = connect(sfd, (struct sockaddr *) &peeraddress, peer_addr_len); // we connect the receiver to the new sender
          if(connection==-1){
            ERROR("connection of the receiver");
          }
          firsttime = false;
        }

        if(recv>0){
          pkt_status_code erreur = pkt_decode(buf,(size_t)528,pkt);
          if(erreur==PKT_OK) { // the package is corect
            DATA_RECEIVED++;

            if(pkt_get_type(pkt)==PTYPE_DATA && pkt_get_length(pkt)==0 && pkt_get_tr(pkt)==0){ // we have receive the last packet
              if(seqnum==pkt_get_seqnum(pkt)){ // we have received all the packet before the last
                seqnum = (seqnum +1 )%256;
                ack(pkt,sfd,seqnum); // we send the ack with the next sequence number we are waiting for
                if(LOG) ERROR("Wallah, c'est ler dernier paquet je te jure en plus son seqnum est: %d", pkt_get_seqnum(pkt));
                pkt_del(pkt);
                break;
              }
              else{
                PACKET_IGNORED++;
              }
              pkt_del(pkt);
            }

            else if(seqnum==pkt_get_seqnum(pkt) && pkt_get_tr(pkt)==0){ // we received a packet in sequence
              int errwrite = write(1,pkt_get_payload(pkt),pkt_get_length(pkt)); // we write the payload of the packet in the standard output
              if(errwrite<0){ // erreur
                ERROR("write()");
              }
              seqnum = (seqnum +1 )%256;
              pkt_t * pktToWrite;
              pktsearch=searchPkt(dataReceive,seqnum);
              while(pktsearch!=NULL){ // we see if we have already received the next packet
                pktToWrite = deQueueReceiver(dataReceive, pkt_get_seqnum(pktsearch));
                int errwrite = write(1,pkt_get_payload(pktToWrite),pkt_get_length(pktToWrite)); // we write the payload of the packet in the standard output
                if(errwrite<0){ // erreur
                  ERROR("write()");
                }
                seqnum = (seqnum +1 )%256;
                window++;
                pktsearch=searchPkt(dataReceive,seqnum);
                pkt_del(pktToWrite);
              }
              if(LOG)ERROR("seqnum du ack envoye apres avoir recu un paquet en sequence: %d", seqnum);
              ack(pkt, sfd,seqnum); // we send the ack with the next sequence number we are waiting for
              pkt_del(pkt);

            }

            else if(pkt_get_tr(pkt)==1){ // Send a NACK
              DATA_TRUNCATED++;
              if(LOG) ERROR("J'ai recu un paquet tronque: %d",pkt_get_seqnum(pkt));
              nack(pkt,sfd); // we send the nack with the sequence number of the packet that has been truncated
              pkt_del(pkt);
            }

            else{ // the packet is not the one that we are waiting for
              pkt_t *pktqueueMathias;
              int seqreceive = pkt_get_seqnum(pkt);
              pktqueueMathias = searchPkt(dataReceive,seqreceive);
              int flagadd = 0; // use to make statistics and free packet that has been already received
              // we check if we already have this packet and if his sequence number is in our window
              if(pktqueueMathias==NULL && ((seqreceive-seqnum>=0 && seqreceive-seqnum<=31) || (abs(seqreceive-seqnum+255)<setwindow && abs(seqreceive-seqnum+255)>=0))){
                if(pkt_get_length(pkt)!=0){ // we don t add the last packet
                  addQueue(dataReceive,pkt);
                  flagadd=1;
                  window--;
                  if(LOG)ERROR("Seqnum recu hors sequence et stocker dans la queue: %d", seqreceive);
                }
              }
              ack(pkt, sfd,seqnum); // we send the ack with the next sequence number we are waiting for
              if(!flagadd){
                PACKET_IGNORED++;
                PACKET_DUPLICATED++;
                pkt_del(pkt);
              }
            }

          }
          else{
            if(LOG) ERROR("Un paquet est corrompu, seqnum attendu : %d ", seqnum);
            pkt_del(pkt); // delete the package if it has been corrupted by the network
          }
        }
    memset((void*) buf, 0, 528);
    }
  }
  free(buf);
  delQueue(dataReceive);
  if(LOG)ERROR("Fin: taille de la window: %d",window);
}

int main(int argc, char **argv) {
    int opt;
    int flagcsv =0;
    char *stats_filename = NULL;
    char *listen_ip = NULL;
    char *listen_port_err;
    uint16_t listen_port;
    while ((opt = getopt(argc, argv, "s:h:w:l")) != -1) {
        switch (opt) {
        case 'h':
            return print_usage(argv[0]);
        case 's':
            stats_filename = optarg;
            flagcsv=1;
            break;
        case 'l':
            ERROR("DAns le -l");
            LOG = true;
            break;
        case 'w':
            ERROR("Avant atoi");
            setwindow=atoi(optarg);
            break;
        default:
            return print_usage(argv[0]);
        }
    }
    window = setwindow;
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

    // First step Find the address IPV6 of the sender
    struct sockaddr_in6 addr;
    int err = real_address(listen_ip, &addr);
    if(err == 0){
      ERROR("Impossible to find the sender address: %s\n", listen_ip);
      return EXIT_FAILURE;
    }

    // second step we create the socket and bind it
    int sock = create_socket(&addr, listen_port);
    if(sock==0){
      ERROR("Failure during the creation of the socket");
      return EXIT_FAILURE;
    }
    if(sock==-1){
      ERROR("Failure during the binding of the socket");
      return EXIT_FAILURE;
    }

    // third step we receive all the packet
    read_loop(sock);

    // final step we close the socket
    if(close(sock)==-1){
      ERROR("erreur du close socket");
      return EXIT_FAILURE;
    }

    //We make the stats
    char  datasent[50] ;
    sprintf(datasent,"data_sent:%ld\n",DATA_SENT);
    char  datarec[50];
    sprintf(datarec,"data_received:%ld\n",DATA_RECEIVED);
    char  datatrunc[50];
    sprintf(datatrunc,"data_truncated_received:%ld\n",DATA_TRUNCATED);
    char  acksent[50];
    sprintf(acksent,"ack_sent:%ld\n",ACK_SENT);
    char  ackrec[50];
    sprintf(ackrec,"ack_receive:%ld\n",ACK_RECEIVED);
    char  nacksent[50];
    sprintf(nacksent,"nack_sent:%ld\n",NACK_SENT);
    char  nackrec[50];
    sprintf(nackrec,"nack_receive:%ld\n",NACK_RECEIVED);
    char  packign[50];
    sprintf(packign,"packet_ignored:%ld\n",PACKET_IGNORED);
    char  packdupl[50];
    sprintf(packdupl,"packet_duplicated:%ld\n",PACKET_DUPLICATED);

    if(!flagcsv){ // print the stats on the error output
    ERROR("%s",datasent);
    ERROR("%s",datarec);
    ERROR("%s" ,datatrunc);
    ERROR("%s" ,acksent);
    ERROR("%s", ackrec);
    ERROR("%s" ,nacksent);
    ERROR("%s", nackrec);
    ERROR("%s", packign);
    ERROR("%s", packdupl);
    }
    else{ // print the stas in a csv file
      int csvopen = open(stats_filename, O_WRONLY | O_CREAT,S_IRWXU);
      if(csvopen==-1){
        ERROR("OPEN csv");
      }
      int erreurcsv;
      erreurcsv = write(csvopen, datasent,strlen(datasent));
      if(erreurcsv==-1) ERROR("write");
      erreurcsv = write(csvopen, datarec,strlen(datarec));
      if(erreurcsv==-1) ERROR("write");
      erreurcsv = write(csvopen, datatrunc,strlen(datatrunc));
      if(erreurcsv==-1) ERROR("write");
      erreurcsv = write(csvopen, acksent,strlen(acksent));
      if(erreurcsv==-1) ERROR("write");
      erreurcsv = write(csvopen, ackrec,strlen(ackrec));
      if(erreurcsv==-1) ERROR("write");
      erreurcsv = write(csvopen, nacksent,strlen(nacksent));
      if(erreurcsv==-1) ERROR("write");
      erreurcsv = write(csvopen, nackrec,strlen(nackrec));
      if(erreurcsv==-1) ERROR("write");
      erreurcsv = write(csvopen, packign,strlen(packign));
      if(erreurcsv==-1) ERROR("write");
      erreurcsv = write(csvopen, packdupl,strlen(packdupl));
      if(erreurcsv==-1) ERROR("write");
      if(close(csvopen)==-1) ERROR("close csv");
    }

    if(LOG)ERROR("Je suis a la fin du programme receiver et je retourne succes");
    return EXIT_SUCCESS;
}
