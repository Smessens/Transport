CFLAGS = -Wall -W -Werror -std=gnu99 -g

all:receiver sender

sender:	sender.o packet_implem.o queue.o
	gcc $(CFLAGS) -o sender sender.o packet_implem.o queue.o -lz

sender.o:	src/sender.c
	gcc $(CFLAGS) -c src/sender.c

receiver:	receiver.o packet_implem.o queue.o
	gcc $(CFLAGS) -o receiver receiver.o packet_implem.o queue.o -lz

receiver.o:	src/receiver.c
	gcc $(CFLAGS) -c src/receiver.c

tests: all
	./tests/run_tests.sh

#suitetests.o packet_implem.o queue.o
suitetests: suitetests.o packet_implem.o queue.o
	gcc $(CFLAGS) -o suitetests suitetests.o packet_implem.o queue.o -lz
	./suitetests
suitetests.o: tests/suitetests.c
	gcc $(CFLAGS) -c tests/suitetests.c


queue: queue.o packet_implem.o
	gcc $(CFLAGS) -o queue queue.o packet_implem.o -lz

queue.o: src/queue.c src/queue.h src/packet_interface.h
	gcc $(CFLAGS) -c src/queue.c
packet_implem.o:	src/packet_implem.c src/packet_interface.h
	gcc $(CFLAGS) -c src/packet_implem.c -lz

# Place the zip in the parent repository of the project
ZIP_NAME="../projet1_Grognard_Marques.zip"

# A zip target, to help you have a proper zip file. You probably need to adapt this code.
zip:
	# Generate the log file stat now. Try to keep the repository clean.
	git log --stat > gitlog.stat
	zip -r $(ZIP_NAME) Makefile rapport.pdf text.txt src tests gitlog.stat
	# We remove it now, but you can leave it if you want.
	rm gitlog.stat



clean:
	rm -rf *.o receiver sender packet_implem queue suitetests *.log input_file received_file output.txt *.csv logFile
