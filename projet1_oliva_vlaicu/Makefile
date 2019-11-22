#Flags
GCC = gcc 

CFLAGS += -Wall # Enable the 'all' set of warnings
#CFLAGS += -Werror # Treat all warnings as error
#FLAGS += -Wextra # Enable additional warnings
CFLAGS += -Wuninitialized

LDFLAGS += -lz

all : receiver

objects1 = receiver.o packet_implem.o functions.o buffer.o
objects2 = packet_implem.o functions.o buffer.o tests.o

receiver : $(objects1)
	$(GCC) -o receiver $(objects1) $(CFLAGS) $(LDFLAGS)

receiver.o : src/receiver.c
	$(GCC) -c src/receiver.c $(CFLAGS)

packet_implem.o : src/packet_implem.c src/packet_interface.h
	$(GCC) -c src/packet_implem.c $(CFLAGS)

functions.o :  src/functions.c src/functions.h
	$(GCC) -c src/functions.c $(CFLAGS)

buffer.o : src/buffer.c src/buffer.h
	$(GCC) -c src/buffer.c $(CFLAGS)

tests : $(objects2)
	$(GCC) -o tests/tests $(objects2) -lcunit $(CFLAGS) $(LDFLAGS)
	./tests/tests

tests.o : tests/tests.c
	$(GCC) -c tests/tests.c $(CFLAGS)

clean :
	rm -rf receiver tests/tests *.o src/*.o tests/*.o 

.PHONY: clean tests
