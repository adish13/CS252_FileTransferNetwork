CC=g++

all : client-phase1 client-phase2 client-phase3 client-phase4 client-phase5 

# task 1
client-phase1: client-phase1.cpp
		$(CC) client-phase1.cpp -o client-phase1


# task 2
client-phase2: client-phase2.cpp
		$(CC) client-phase2.cpp -o client-phase2


# task 3
client-phase3: client-phase3.cpp
		$(CC) client-phase3.cpp -o client-phase3 -lcrypto


# task 4
client-phase4: client-phase4.cpp
		$(CC) client-phase4.cpp -o client-phase4 -pthread


# task 5
client-phase5: client-phase5.cpp
		$(CC) client-phase5.cpp -o client-phase5 -pthread -lcrypto

# task 7
.PHONY : clean 
clean:
		rm -f *.o *.a *.so
