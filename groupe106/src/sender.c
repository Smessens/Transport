#include "packet_interface.h"
#include "queue.h"
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
#include <sys/stat.h>
#include <stdbool.h>
#include "log.h"
#define true 1
#define false 0

#define RTT 1000  //time before the package is sent back in ms

bool LOG = false; //set to true if you activate the -l option this option allows to display the print that are useful for debugging the program

// these variables are used to output statistics from our program
long DATA_SENT = 0;
long DATA_RECEIVED = 0;
long DATA_TRUNCATED = 0;
long ACK_SENT = 0;
long ACK_RECEIVED = 0;
long NACK_SENT = 0;
long NACK_RECEIVED = 0;
long PACKET_IGNORED = 0;
int min_rtt = 9999999;
int max_rtt = 0;
long PACKET_RETRANSMITTED = 0;

// This function is called in case of wrong argument in the execution command of the function
int print_usage(char *prog_name) {
    ERROR("Usage:\n\t%s [-f filename] [-s stats_filename] receiver_ip receiver_port", prog_name);
    return EXIT_FAILURE;
}

/* This function create a struct sockaddr_in6 which contains
// the IPV6 address of the receiver
// @receiver_ip -> IPV6 address of the receiver
// @addr -> structure with the address of the receiver
// RETURN 1 if succeed, 0 if fail
*/
int real_address(char * receiver_ip, struct sockaddr_in6 * addr){

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  hints.ai_flags = 0;

  struct addrinfo * res;
  int err = getaddrinfo(receiver_ip, NULL, &hints, &res);
  if(err!=0){
    return 0;
  }
  struct sockaddr_in6 *result = (struct sockaddr_in6 *) (res->ai_addr);
  *addr = *result;
  freeaddrinfo(res);
  return 1;
}

/* This function create and connect the socket which will be use
// between the sender and the receiver
// @receiver_addr -> structure socaddr of the receiver
// @receiver_port -> The port on which the sender and the receiver communicate
// RETURN the socket if succeed, 0 if the creeation of the socket failed
// and -1 if the connection failed
*/
int create_socket(struct sockaddr_in6 *receiver_addr, int receiver_port){
  int sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);             // create a socket using IPv6 addresses
  if (sock == -1) {
    return 0;
  }

  if(receiver_addr!=NULL && receiver_port>0){
    receiver_addr->sin6_port = htons(receiver_port);
    int connection = connect(sock, (struct sockaddr *) receiver_addr,sizeof(struct sockaddr_in6));
    if(connection==-1){
      return -1;
    }
  }
  return sock;
}


/*
* This function will loop until the last message is send to the receiver or he can't find the receiver
* @sfd -> the file descriptor of the socket
* @fd -> the file descriptor where the message is read to be sent to the receiver
* RETURN void
*/
void writedata(const int sfd, int fd){
  fd_set rfds;
  int seqnumack=0;// number of the last ack receive
  char * buf = (char*)malloc(512); // read the message that must be sent
  if(buf==NULL){
    if(LOG)ERROR("BUF NULL");
  }
  char * bufresult = (char*) malloc(10); // to receive ack and nack
  if(bufresult==NULL){
    if(LOG)ERROR("BUF NULL1");
  }
  char * bufpkt = (char*) malloc(528); // to send the packet
  if(bufpkt==NULL){
    if(LOG)ERROR("BUF NULL2");
  }
  pkt_t *pktrec = pkt_new();
  struct Queue * dataSend; // Stock the packet that are already sent
  dataSend = createQueue();
  int window = 0; // size of the receiver window
  int seqnum = -1;
  int minSeqQueue = 0;
  int stop=1;
  int countlast=0;
  int countfirst=1;
  int boolfirst=0; //Allow to see if there is a receiver
  // loop until the last message is send to the receiver or he can't find the receiver
  while(1){
    struct timeval tv = {20,0};
    memset((void*) buf, 0, 512); //Il faudra changer le 512 car c'est aps juste
    // we set the select to look at the file descriptor of the socket and the file decriptor wich contains the texte to send
    FD_ZERO(&rfds);
    FD_SET(fd,&rfds); // To send
    FD_SET(sfd,&rfds); //For the ack
    int retval = select(sfd+1+fd, &rfds, NULL, NULL, &tv);

    if(retval==-1){ //error
      ERROR("the function select dont work");
    }

    if(retval==0) break; // we don't send or receive message in the last 20 second

    // the reciver still has room in his window and we have not yet finished the file to send
    if(FD_ISSET(fd,&rfds) && getSize(dataSend)<=window && stop){
      pkt_t *pkt = pkt_new();
      int size;
      if(fd==-1){
        size = read(0, buf,512); // we read on STDIN
      }
      else{
        size = read(fd, buf,512); // we read in the txt file
        if(size==0){
          stop=0; // we finished the file
        }
      }
      if(size>=0){ //No error with the read
        seqnum = (seqnum+1)%256;
        if(pkt_set_type(pkt,1)!=PKT_OK) ERROR("set pkt type");
        if(pkt_set_tr(pkt,0)!=PKT_OK) ERROR("set pkt tr");
        if(pkt_set_window(pkt,31)!=PKT_OK) ERROR("set pkt window");
        if(pkt_set_length(pkt,size)!=PKT_OK) ERROR("set length");
        if(pkt_set_seqnum(pkt,seqnum)!=PKT_OK) ERROR("set seqnum");
        if(pkt_set_timestamp(pkt,42)!=PKT_OK) ERROR("set timestamp");
        if(pkt_set_payload(pkt,buf,size)!=PKT_OK) ERROR("set payload");
        memset((void*)bufpkt,0,528);
        size_t len =  strlen(bufpkt);
        if(pkt_encode(pkt,bufpkt,&len)!=PKT_OK) ERROR("set encode"); // we put the pkt in the buffer
        addQueue(dataSend,pkt); // this queu is use to see the packet that are already sent once but waiting for there ack
        int errwrite = write(sfd, bufpkt, len); // sent the packet
        if(errwrite<0){
          ERROR("The function write dont work");
        }
        //pkt_del(pkt);
        DATA_SENT++;
      }
    }
    if(FD_ISSET(sfd,&rfds)){ // We have receive an ack or a nack
      ssize_t rcv = recv(sfd, bufresult, 10, 0);
      if(rcv==-1){
        ERROR("erreur du recv");
      }
      pkt_status_code errdec = pkt_decode(bufresult,10,pktrec);
      if(errdec==PKT_OK){ // we see if the packet is correct
        ptypes_t type = pkt_get_type(pktrec);
        if(type==PTYPE_ACK){ //we have receive an ack
          boolfirst=1;
          int seqToDel = pkt_get_seqnum(pktrec);
          if(LOG)ERROR("J'ai recu un ack : %d mon seqnumack: %d",seqToDel,seqnumack);
          if((seqToDel>=seqnumack && seqnumack+32>=seqToDel)|| seqToDel+255<=seqnumack+31 ){ // we check if we have already receive this ack
            ACK_RECEIVED++;
            seqnumack=seqToDel;
            window = pkt_get_window(pktrec);
            pkt_t *pkta;
            if(LOG)ERROR("Seqnum du ack recu: %d et dernier paquet recu par le receiver: %d",seqToDel, seqToDel-1);
            while(minSeqQueue!=seqToDel){ // we remove the packet that have been confirmed has received by the receiver
              int acktime = getTimee(dataSend,minSeqQueue); // for stats
              if(acktime!=-1){
                if(acktime>max_rtt) max_rtt = acktime;
                if(acktime<min_rtt) min_rtt = acktime;
              }
              pkta = deQueueReceiver(dataSend, minSeqQueue);
              if(pkt_get_timestamp(pktrec)!=pkt_get_timestamp(pkta)){
                ERROR("timestamp different: %d et : %d",pkt_get_timestamp(pktrec), pkt_get_timestamp(pkta));
              }
              minSeqQueue = (minSeqQueue + 1) % 256;
              pkt_del(pkta);
            }

            if(!stop && isEmpty(dataSend)) // we have sent all the packet and receive all the ack
              break;
          }
          else{ // stats
            PACKET_IGNORED++;
          }

        }
        if(type==PTYPE_NACK){ // nack
          boolfirst=1;
          int seqNack = pkt_get_seqnum(pktrec);
          struct Elem * currentnack = getFront(dataSend);
          while(currentnack!= NULL && pkt_get_seqnum(getPackett(currentnack))!=seqNack){ // we get the right packet
            currentnack = getNext(currentnack);
          }
          if(currentnack!=NULL){
            memset((void*)bufpkt,0,528);
            size_t len =  strlen(bufpkt);
            if(pkt_encode(getPackett(currentnack),bufpkt,&len)!=PKT_OK) ERROR("set encode"); // we put the packet in a buf
            if(LOG)ERROR("Seqnum du packet NACK a renvoye %d",pkt_get_seqnum(getPackett(currentnack)));
            int errwrite = write(sfd, bufpkt, len);// we send the packet
            if(errwrite<0){
              ERROR("The function write dont work NACK");
            }
            struct timespec newTime2;
            int err = clock_gettime(CLOCK_MONOTONIC,&newTime2);
            if(err==-1){
              ERROR("getTime");
            }
            int time = (newTime2.tv_sec-getTimeElem(currentnack).tv_sec)*1000 + (newTime2.tv_nsec-getTimeElem(currentnack).tv_nsec)/1000000; //for stats
            addTime(currentnack,time);
            DATA_SENT++;
            NACK_RECEIVED++;
            PACKET_RETRANSMITTED++;
            setTime(currentnack); // we reset the timer of the packet
          }
        }
      }
      else{if(LOG)ERROR("CORROMPU ACK, valeur du seqnumack: %d ",seqnumack);}
    }
    // resend the packet if the timer has expired
    struct timespec newTime;
    int err = clock_gettime(CLOCK_MONOTONIC,&newTime);
    if(err==-1){
      ERROR("getTime");
    }
    struct Elem * current = getFront(dataSend);
    // this is to close the program in case we don t have receive the ack for the last packet
    // the strategy to close the sender is explained in the attached report
    if(countlast==10){
      pkt_del(getPackett(current));
      break;
    }
    if(countfirst==10){
      pkt_del(getPackett(current));
      if(LOG) ERROR("Aucun receiver detecte");
      break;
    }
    while(current!=NULL){ // Resend the packet
      int time = (newTime.tv_sec-getTimeElem(current).tv_sec)*1000 + (newTime.tv_nsec-getTimeElem(current).tv_nsec)/1000000; // get the time
      if(time>RTT){ // we have to resend this packet
        int erroclock = clock_gettime(CLOCK_MONOTONIC,&newTime);
        if(erroclock==-1) ERROR("Time");
        // this is to close the program in case we don t have receive the ack for the last packet
        // the strategy to close the sender is explained in the attached report
        if(pkt_get_length(getPackett(current))==0 && getSize(dataSend)==1){
          countlast ++;
        }
        if(!boolfirst) countfirst++;
        memset((void*)bufpkt,0,528); //buffer use to resend the packet
        size_t len =  strlen(bufpkt);
        if(pkt_encode(getPackett(current),bufpkt,&len)!=PKT_OK) ERROR("set encode");
        if(LOG)ERROR("Seqnum du packet a renvoye %d",pkt_get_seqnum(getPackett(current)));
        int errwrite = write(sfd, bufpkt, len); // we send the packet
        if(errwrite<0){
          ERROR("The function write dont work");
        }
        addTime(current, time);// for stats
        DATA_SENT++;
        PACKET_RETRANSMITTED++;
        setTime(current);
      }
      current = getNext(current); // we reset the timer of the packet
    }
  }
  free(buf);
  free(bufresult);
  free(bufpkt);
  pkt_del(pktrec);
  delQueue(dataSend);
}

int main(int argc, char **argv) {
    int opt;
    int flagcsv=0;
    char *filename = NULL;
    char *stats_filename = NULL;
    char *receiver_ip = NULL;
    char *receiver_port_err;
    uint16_t receiver_port;
    while ((opt = getopt(argc, argv, "f:s:h:l")) != -1) {
        switch (opt) {
        case 'f':
            filename = optarg;
            if(LOG)ERROR("Filename: %s", filename);
            //return print_usage(argv[0]);
            break;
        case 'h':
            return print_usage(argv[0]);
        case 's':
            stats_filename = optarg;
            flagcsv=1;
            break;
        case 'l':
        ERROR("je iens dans -l ");
            LOG = true;
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
    if(LOG) ERROR("receiver_ip: %s\n", receiver_ip);
    receiver_port = (uint16_t) strtol(argv[optind + 1], &receiver_port_err, 10);
    if(LOG) ERROR("receiver_port: %d\n", receiver_port);
    if (*receiver_port_err != '\0') {
        ERROR("Receiver port parameter is not a number");
        return print_usage(argv[0]);
    }

    // First step Find the address IPV6 of the receiver
    struct sockaddr_in6 addr;
    int err = real_address(receiver_ip, &addr);
    if(err == 0){
      ERROR("Impossible to find the receiver address: %s\n", receiver_ip);
      return EXIT_FAILURE;
    }

    // second step Create and connect the socket to the receiver
    int sock = create_socket(&addr, receiver_port);
    if(sock==0){
      ERROR("Failure during the creation of the socket");
      return EXIT_FAILURE;
    }
    if(sock==-1){
      ERROR("Failure during the connection of the socket");
      return EXIT_FAILURE;
    }

    int fd=0; // STDIN

    // third step check if we have a file and if we have one open it
    if(filename!=NULL){
      fd = open(filename, O_RDONLY);
      if(fd==-1){
        ERROR("erreur open fichier");
        return EXIT_FAILURE;
      }
    }

    // fourth step write the data from the STDIN or from the file
    writedata(sock, fd);

    // fifth step check if we have a file and if we have one close it
    if(filename!=NULL){
      if(close(fd)==-1){
        ERROR("erreur du close");
        return EXIT_FAILURE;
      }
    }

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
    char  minrtt[50];
    sprintf(minrtt,"min_rtt:%d\n",min_rtt);
    char  maxrtt[50];
    sprintf(maxrtt,"max_rtt:%d\n",max_rtt);
    char  packdupl[50];
    sprintf(packdupl,"packet_retransmitted:%ld\n",PACKET_RETRANSMITTED);

    if(!flagcsv){ // print the stats on the error output
      ERROR("%s",datasent);
      ERROR("%s",datarec);
      ERROR("%s" ,datatrunc);
      ERROR("%s" ,acksent);
      ERROR("%s", ackrec);
      ERROR("%s" ,nacksent);
      ERROR("%s", nackrec);
      ERROR("%s", packign);
      ERROR("%s", minrtt);
      ERROR("%s", maxrtt);
      ERROR("%s", packdupl);
    }
    else{ // print the stas in a csv file
      int csvopen = open(stats_filename, O_WRONLY |O_CREAT,S_IRWXU);
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
      erreurcsv = write(csvopen, minrtt,strlen(minrtt));
      if(erreurcsv==-1) ERROR("write");
      erreurcsv = write(csvopen, maxrtt,strlen(maxrtt));
      if(erreurcsv==-1) ERROR("write");
      erreurcsv = write(csvopen, packdupl,strlen(packdupl));
      if(erreurcsv==-1) ERROR("write");
      if(close(csvopen)==-1) ERROR("close csv");
    }

    if(LOG)ERROR("Je suis a la fin du programme sender et je retourne succes");
    return EXIT_SUCCESS;
}
