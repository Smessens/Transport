CFLAGS += -std=gnu99
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wshadow 
CFLAGS += -Wextra
CFLAGS += -O2 -D_FORTIFY_SOURCE=2
CFLAGS += -fstack-protector-all 
CFLAGS += -D_POSIX_C_SOURCE=201112L -D_XOPEN_SOURCE
CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += -D_BSD_SOURCE
CFLAGS += -DTRACE

LDFLAGS= -rdynamic

all : clean receiver cleanPost

leak : all val

tests : clean receiver test1 test2 test3 test4 test5 test6 test7 test8 test9 cleanPost

test1 : tests/tests_pkt_queue.o src/pkt_queue_implem.a src/packet_implem.a
	@gcc -g -lz -lcunit -o testQueue tests/tests_pkt_queue.o src/pkt_queue_implem.a src/packet_implem.a
	./testQueue

test2 : tests/tests_packet.o src/packet_implem.a
	@gcc -g -lz -lcunit -o testPacket tests/tests_packet.o src/packet_implem.a 
	./testPacket

test3 : tests/tests_socket.o src/socket_implem.a src/pkt_queue_implem.a src/packet_implem.a
	@gcc -g -lz -lcunit -o testSocket tests/tests_socket.o src/socket_implem.a src/pkt_queue_implem.a src/packet_implem.a
	./testSocket

test4 :
	./tests/test1.sh

test5 :
	./tests/test2.sh

test6 :
	./tests/test3.sh

test7 :
	./tests/test4.sh

test8 :
	./tests/test5.sh

test9 :
	./tests/testglob.sh

receiver: src/receiver.o src/pkt_queue_implem.a src/packet_implem.a src/socket_implem.a
	@gcc -g -lz -o receiver src/receiver.o src/pkt_queue_implem.a src/packet_implem.a src/socket_implem.a

src/packet_implem.a : src/packet_implem.c
	@gcc -g -c src/packet_implem.c -o src/packet_implem.a

src/pkt_queue_implem.a : src/pkt_queue_implem.c
	@gcc -g -c src/pkt_queue_implem.c -o src/pkt_queue_implem.a

src/socket_implem.a : src/socket_implem.c
	@gcc -g -c src/socket_implem.c -o src/socket_implem.a


.PHONY: clean
clean:
	@rm -f receiver
	@find . -name \*.o -type f -delete
	@find . -name \*.a -type f -delete

.PHONY: cleanPost
cleanPost:
	@find . -name \*.o -type f -delete
	@find . -name \*.a -type f -delete

val:
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./receiver -o %d.jpg :: 12345
