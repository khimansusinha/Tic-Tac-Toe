GCC := gcc 

All: client server

server: server.o
	$(GCC) -o server server.o
	rm -rf *.o

server.o: server.c
	$(GCC) -c server.c

client: client.o
	$(GCC) -o client client.o

client.o: client.c
	$(GCC) -c client.c

clean:
	rm -rf *.o server client
