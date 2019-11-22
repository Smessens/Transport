CFLAGS += -std=c99 # Define which version of the C standard to use
CFLAGS += -Wall # Enable the 'all' set of warnings
CFLAGS += -Werror # Treat all warnings as error
CFLAGS += -Wshadow # Warn when shadowing variables
CFLAGS += -Wextra # Enable additional warnings
CFLAGS += -O2 -D_FORTIFY_SOURCE=2 # Add canary code, i.e. detect buffer overflows
CFLAGS += -fstack-protector-all # Add canary code to detect stack smashing
CFLAGS += -D_POSIX_C_SOURCE=201112L -D_XOPEN_SOURCE # feature_test_macros for getpot and getaddrinfo

LDFLAGS += -rdynamic -lz

all: clean makesender clean_o

# Lance les tests.
test : all
	cd tests && make

debug: CFLAGS += -g -DDEBUG -Wno-unused-parameter -fno-omit-frame-pointer
debug: clean makesender clean_o

tests:
	./tests/receiver -o received_file :: 64342
	valgrind --leak-check=yes --track-origins=yes --show-leak-kinds=all  ./sender -f tests/text.txt ::1 64342

#Crée le sender
makesender: sender read_write_loop create_socket real_address packet_implem
	gcc sender.o read_write_loop.o create_socket.o real_address.o packet_implem.o -o sender $(CFLAGS) $(LDFLAGS)

sender: src/sender.c
	gcc src/sender.c -c $(CFLAGS)

read_write_loop: src/read_write_loop.c src/read_write_loop.h
	gcc src/read_write_loop.c -c $(CFLAGS)

create_socket: src/create_socket.c src/create_socket.h
	gcc src/create_socket.c -c $(CFLAGS)

real_address: src/real_address.c src/real_address.h
	gcc src/real_address.c -c $(CFLAGS)

packet_implem: src/packet_implem.c src/packet_interface.h
	gcc src/packet_implem.c -c $(CFLAGS)

wait_for_client: src/wait_for_client.c src/wait_for_client.h
	gcc src/wait_for_client.c -c $(CFLAGS)

.PHONY: clean clean_o

clean: clean_o cleantest
	@rm -f sender

clean_o:
	@rm -rf *.o

cleantest:
	cd tests && make cleanall

#Crée notre pdf de rapport
pdf:
	@pdflatex rapport.tex
	@pdflatex rapport.tex
	@rm -f rapport.aux
	@rm -f rapport.log
	@rm -f rapport.out
	@rm -f rapport.toc

#Enregistre les stats git
stat:
	@echo 'Registering git stats'
	@git log --stat > gitlog.stat

#Crée l'archive à rendre pour le projet
zip: clean_o stat pdf
	@echo 'Création du .zip'
	@zip projet1_aigret_schellekens.zip Makefile rapport.pdf gitlog.stat -r src -r tests
