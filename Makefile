
all: dropboxClient.o dropboxServer.o
	g++ -o clientDropbox mainClient.cpp dropboxClient.o -lpthread
	g++ -o serverDropbox mainServer.cpp dropboxServer.o

debug: dropboxClientDebug
	g++ -o dropboxClient dropboxClient.cpp -Wall -g

dropboxClient.o: dropboxClient.cpp
	g++ -c dropboxClient.cpp -Wall -lpthread

dropboxClientDebug: dropboxClient.cpp
	g++ -c dropboxClient.cpp -Wall -g

dropboxServer.o: dropboxServer.cpp
	g++ -c dropboxServer.cpp -Wall

dropboxServerDebug: dropboxServer.cpp
	g++ -c dropboxServer.cpp -Wall -g

clean:
	rm *.o clientDropbox serverDropbox
