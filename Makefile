SERVER_DIR =  ./Server/
CLIENT_DIR = ./Client/

all: dropboxClient.o dropboxServer.o
	g++ -o client $(CLIENT_DIR)mainClient.cpp dropboxClient.o -lpthread
	g++ -o server $(SERVER_DIR)mainServer.cpp dropboxServer.o -lpthread
	mv dropboxServer.o $(SERVER_DIR)
	mv dropboxClient.o $(CLIENT_DIR)

debug: dropboxClientDebug dropboxServerDebug
	g++ -o client $(CLIENT_DIR)mainClient.cpp dropboxClient.o -lpthread
	g++ -o server $(SERVER_DIR)mainServer.cpp dropboxServer.o -lpthread
	mv dropboxServer.o $(SERVER_DIR)
	mv dropboxClient.o $(CLIENT_DIR)

dropboxClient.o: $(CLIENT_DIR)dropboxClient.cpp
	g++ -c $(CLIENT_DIR)dropboxClient.cpp -Wall -lpthread

dropboxClientDebug: $(CLIENT_DIR)dropboxClient.cpp
	g++ -c $(CLIENT_DIR)dropboxClient.cpp -Wall -g -lpthread

dropboxServer.o: $(SERVER_DIR)dropboxServer.cpp
	g++ -c $(SERVER_DIR)dropboxServer.cpp -Wall -lpthread

dropboxServerDebug: $(SERVER_DIR)dropboxServer.cpp
	g++ -c $(SERVER_DIR)dropboxServer.cpp -Wall -g -lpthread
	
clean:
	rm $(CLIENT_DIR)*.o $(SERVER_DIR)*.o client server
