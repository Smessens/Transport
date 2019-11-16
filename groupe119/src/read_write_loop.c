#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdlib.h>

#include "read_write_loop.h"
#include "packet_interface.h"


int isInWindow(int n, int startWindow, int lengthWindow) {
    if(startWindow + lengthWindow-1 <= MAX_SEQUENCE_NUMBER) return (n >= startWindow && n < startWindow + lengthWindow);

    else return (n >= startWindow || n < (startWindow + lengthWindow)%(MAX_SEQUENCE_NUMBER));
}

int passedTime(struct timeval pastTV) {

    struct timeval now;

    gettimeofday(&now, NULL);

    int seconds = now.tv_sec - pastTV.tv_sec;
    int milliseconds = (now.tv_usec - pastTV.tv_usec)/1000;

    if(milliseconds < 0) {
        seconds--;
        milliseconds = 1000 - milliseconds;
    }

    return 1000*seconds + milliseconds;
}

int min(int a, int b) {if(a <= b) return a; else return b;}
int max(int a, int b) {if(a >= b) return a; else return b;}

void read_write_loop_sender(const int sfd) {

  int RTT = 2000;

  int timeout = (RTT *3);

  int senderWindow = MAX_WINDOW_SIZE;
  int receiverWindow =  1;

  pkt_t *emptyPkt = pkt_new();
  pkt_set_type(emptyPkt, 0);

  pkt_t *toSend = (pkt_t *) malloc(sizeof(pkt_t) * 2*MAX_WINDOW_SIZE);
  for(int i= 0; i < 2*MAX_WINDOW_SIZE; i++) memcpy(toSend + i, emptyPkt, sizeof(pkt_t));

    pkt_t *sent = (pkt_t *) malloc(sizeof(pkt_t) * MAX_WINDOW_SIZE);
    for(int i= 0; i < MAX_WINDOW_SIZE; i++) memcpy(sent + i, emptyPkt, sizeof(pkt_t));

    pkt_del(emptyPkt);

    struct timeval timeSent[MAX_SEQUENCE_NUMBER];
    struct timeval zero;
    zero.tv_sec = 0;
    zero.tv_usec = 0;
    for(int i= 0; i < MAX_SEQUENCE_NUMBER; i++) {
     	memcpy((timeSent + i), &zero, sizeof(struct timeval));;
    }

    int startToSend = 0;
    int startSent = 0;
    int nbToSend = 0;
    int nbSent = 0;
    int seqnum = 0;
    int windowBegin = 0;

    int disconnect = -2;

	  fd_set rfds;
	  fd_set wfds;

    int endOfFile = 0;

    while(1) {
      while(nbSent > 0 && passedTime(timeSent[startSent]) >= timeout) {

        pkt_t * sent_failure = &(sent[startSent]);

        if(isInWindow(sent_failure->seqnum, windowBegin, senderWindow)) {
          struct timeval zeroTime;
          zeroTime.tv_sec = 0;
          zeroTime.tv_usec = 0;
          timeSent[pkt_get_seqnum(sent_failure)] = zeroTime;

          toSend[(startToSend+nbToSend)%(2*MAX_WINDOW_SIZE)] = *sent_failure;

          nbToSend++;
        }

        nbSent--;
        startSent = (startSent+1)%MAX_WINDOW_SIZE;
      }

      FD_ZERO(&rfds);
      FD_ZERO(&wfds);

      if(!endOfFile && isInWindow(seqnum, windowBegin, senderWindow)) FD_SET(fileno(stdin), &rfds);

      if(nbSent < min(senderWindow, receiverWindow) && nbToSend > 0) FD_SET(sfd, &wfds);

      if(nbSent != 0) FD_SET(sfd, &rfds);

      struct timeval tv;
      if(nbSent > 0) {
        int timer = timeout-passedTime(timeSent[pkt_get_seqnum(&sent[startSent])]);
        timer = max(timer, 0);

        tv.tv_sec = timer/1000;
        tv.tv_usec = 1000*(timer%1000);
      }
      else {
        tv.tv_sec = 10;
        tv.tv_usec = 0;
      }

		  select(sfd+1, &rfds, &wfds, NULL, &tv);

      while(nbSent > 0 && passedTime(timeSent[startSent]) >= timeout) {

        pkt_t * sent_failure = &(sent[startSent]);

        if(isInWindow(sent_failure->seqnum, windowBegin, senderWindow)) {

          struct timeval zeroTime;
          zeroTime.tv_sec = 0;
          zeroTime.tv_usec = 0;
          timeSent[pkt_get_seqnum(sent_failure)] = zeroTime;

          toSend[(startToSend+nbToSend)%(2*MAX_WINDOW_SIZE)] = *sent_failure;

          nbToSend++;
        }

        nbSent--;
        startSent = (startSent+1)%MAX_WINDOW_SIZE;
      }

      if(FD_ISSET(fileno(stdin), &rfds) && isInWindow(seqnum, windowBegin, senderWindow)) {

        char * in_buf = (char *) malloc(512);
        int size=0;
        memset(in_buf, '\0', 512);
        if((size=read(fileno(stdin), in_buf, 512)) == -1){
          fprintf(stderr, "ERROR READING STDIN\n");
          break;
        }

        if(size == 0) endOfFile = 1;

        pkt_t* new_pkt = toSend+(startToSend+nbToSend)%(2*MAX_WINDOW_SIZE);
        pkt_set_type(new_pkt, PTYPE_DATA);
        pkt_set_window(new_pkt, senderWindow);
        pkt_set_seqnum(new_pkt, seqnum);
        pkt_set_length(new_pkt, size);
        pkt_set_payload(new_pkt, in_buf, size);

        free(in_buf);
        nbToSend++;

        seqnum = (seqnum + 1)%(MAX_SEQUENCE_NUMBER+1);
		    continue;
      }


      if(FD_ISSET(sfd, &wfds) && nbToSend>0) {

			pkt_t * pkt_send = toSend+startToSend;


      while(nbToSend>0 && !isInWindow(pkt_send->seqnum, windowBegin, senderWindow)) {
        nbToSend--;
        startToSend = (startToSend+1)%(2*MAX_WINDOW_SIZE);
        pkt_send = toSend+startToSend;
      }

      if(nbToSend==0) {
        continue;
      }
      struct timespec spec;

      clock_gettime(CLOCK_REALTIME, &spec);

      pkt_set_timestamp(pkt_send, spec.tv_nsec);
      pkt_set_window(pkt_send, senderWindow);

      size_t len = 524;
      char * data = (char *) malloc(len);

      pkt_status_code err = pkt_encode(pkt_send, data, &len);

      if(err != PKT_OK) {
        fprintf(stderr, "ERROR ENCODING, ERROR N° %d\n", (int) err);
        free(data);
        continue;
      }

      if(write(sfd, data, len) == -1) {
        if(errno == ECONNREFUSED) {
          break;
        }
				fprintf(stderr, "ERROR WRITTING SFD\n");
				fprintf(stderr, "%s\n", strerror(errno));
        free(data);
				break;
			}

      free(data);

      pkt_t * pkt_sent = sent+(startSent+nbSent)%(MAX_WINDOW_SIZE);
      memcpy(pkt_sent, pkt_send, sizeof(pkt_t));

      if(pkt_get_length(pkt_sent)==0) {
        disconnect=pkt_get_seqnum(pkt_sent);
      }

      gettimeofday(& (timeSent[(startSent+nbSent)%(MAX_WINDOW_SIZE)]), NULL);
      nbSent++;

      nbToSend--;
      startToSend = (startToSend+1)%(2*MAX_WINDOW_SIZE);
		  continue;
    }

    if(FD_ISSET(sfd, &rfds) && nbSent != 0) {
      char * data = (char *) malloc(524);
      pkt_t * pkt_ack = pkt_new();

			int size=0;
			memset(data, '\0', 524);

			if((size=read(sfd, data, 524)) == -1) {
				fprintf(stderr, "ERROR READING SOCKET\n");
				break;
			}

      pkt_status_code err = pkt_decode(data, size, pkt_ack);

      if(err != PKT_OK) {
        fprintf(stderr, "ERROR DECODING, ERROR N° %d\n", (int) err);
        pkt_del(pkt_ack);
        free(data);
        continue;
      }

      if(pkt_get_type(pkt_ack) != PTYPE_ACK) {
        fprintf(stderr, "ERROR : NOT AN ACK\n");
        pkt_del(pkt_ack);
        free(data);
        continue;
      }

      if(isInWindow(pkt_ack->seqnum, windowBegin, senderWindow)) windowBegin = pkt_ack->seqnum;

      receiverWindow = pkt_ack->window;
      int round_trip = passedTime(timeSent[pkt_ack->seqnum]);
      timeout = min(1.1*timeout, max(0.9*timeout, (round_trip + timeout)/2));

      pkt_del(pkt_ack);
      free(data);

      if((windowBegin+MAX_SEQUENCE_NUMBER-1)%MAX_SEQUENCE_NUMBER == disconnect) {
        break;
      }

      while(nbSent > 0 && !isInWindow(sent[startSent].seqnum, windowBegin, senderWindow)) {
        nbSent--;
        startSent = (startSent+1)%MAX_WINDOW_SIZE;
      }
    }
  }
  free(toSend);
  free(sent);
}
