SERVER_DIR =  ./Server/
CLIENT_DIR = ./Client/
UTIL_DIR = ./Util/

all: dropboxUtil.o dropboxClient.o dropboxServer.o
	g++ -o client $(CLIENT_DIR)mainClient.cpp dropboxClient.o dropboxUtil.o -lpthread
	g++ -o server $(SERVER_DIR)mainServer.cpp dropboxServer.o dropboxUtil.o -lpthread
	mv dropboxServer.o $(SERVER_DIR)
	mv dropboxClient.o $(CLIENT_DIR)
	mv dropboxUtil.o $(UTIL_DIR)

debug: dropboxClientDebug dropboxServerDebug dropboxUtilDebug
	g++ -o client $(CLIENT_DIR)mainClient.cpp dropboxClient.o dropboxUtil.o -lpthread
	g++ -o server $(SERVER_DIR)mainServer.cpp dropboxServer.o dropboxUtil.o -lpthread
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

dropboxUtil.o: $(UTIL_DIR)dropboxUtil.cpp
	g++ -c $(UTIL_DIR)dropboxUtil.cpp -Wall

dropboxUtilDebug: $(UTIL_DIR)dropboxUtil.cpp
	g++ -c $(UTIL_DIR)dropboxUtil.cpp -Wall -g

clean:
	rm $(CLIENT_DIR)*.o $(SERVER_DIR)*.o $(UTIL_DIR)*.o client server
