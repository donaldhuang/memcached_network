CC	:= g++
CFLAGS	:= -L/usr/local/libevent/lib/ -levent -pthread
OBJECTS	:= main.cpp cq.o conn.o WorkerThreads.o

tpm: $(OBJECTS)
	$(CC) $(OBJECTS) -I/usr/local/libevent/include/ -o tpm $(CFLAGS)
cq.o: cq.cpp cq.h
	$(CC) cq.cpp -I/usr/local/libevent/include/ -c
conn.o: conn.cpp conn.h libevent_thread.h
	$(CC) conn.cpp -I/usr/local/libevent/include/ -c
WorkerThreads.o: WorkerThreads.cpp WorkerThreads.h
	$(CC) WorkerThreads.cpp -I/usr/local/libevent/include/ -c

clean:
	rm tpm *.o
