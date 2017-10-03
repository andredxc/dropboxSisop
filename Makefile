
dropBox: dropboxClient.o
	gcc -o dropboxClient dropboxClient.c
	# gcc -o dropboxServer dropboxServer.c

dropboxClient.o: dropboxClient.c
	gcc -c dropboxClient.c -Wall

# dropboxServer.o:
# 	gcc -c dropboxServer.c -Wall

clean:
	rm *.o dropboxClient dropboxServer
