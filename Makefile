
all: dropboxClient.o dropboxServer.o
	gcc -o dropboxClient dropboxClient.c
	gcc -o dropboxServer dropboxServer.c

debug: dropboxClientDebug
	gcc -o dropboxClient dropboxClient.c -Wall -g

dropboxClient.o: dropboxClient.c
	gcc -c dropboxClient.c -Wall

dropboxClientDebug: dropboxClient.c

	gcc -c dropboxClient.c -Wall -g
dropboxServer.o: dropboxServer.c
	gcc -c dropboxServer.c -Wall

dropboxServerDebug: dropboxServer.c
	gcc -c dropboxServer.c -Wall -g

clean:
	rm *.o dropboxClient dropboxServer
