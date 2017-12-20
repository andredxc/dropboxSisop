SERVER_DIR =  ./Server/
CLIENT_DIR = ./Client/
UTIL_DIR = ./Util/

all: dropboxUtil.o dropboxClient.o dropboxServer.o #clientProxy.o
	g++ -o client $(CLIENT_DIR)mainClient.cpp dropboxClient.o dropboxUtil.o -lpthread -lssl -lcrypto
	g++ -o server $(SERVER_DIR)mainServer.cpp dropboxServer.o dropboxUtil.o -lpthread -lssl -lcrypto
	mv dropboxServer.o $(SERVER_DIR)
	mv dropboxClient.o $(CLIENT_DIR)
	mv dropboxUtil.o $(UTIL_DIR)
	#mv clientProxy.o $(CLIENT_DIR)
	#g++ -o proxy $(CLIENT_DIR)mainProxy.cpp clientProxy.o dropboxUtil.o -lpthread

debug: dropboxClientDebug dropboxServerDebug dropboxUtilDebug clientProxy.o
	g++ -o client $(CLIENT_DIR)mainClient.cpp dropboxClient.o dropboxUtil.o -lpthread -lssl -lcrypto
	g++ -o proxy $(CLIENT_DIR)mainProxy.cpp clientProxy.o dropboxUtil.o -lpthread
	g++ -o server $(SERVER_DIR)mainServer.cpp dropboxServer.o dropboxUtil.o -lpthread -lssl -lcrypto
	mv dropboxServer.o $(SERVER_DIR)
	mv dropboxClient.o $(CLIENT_DIR)
	mv clientProxy.o $(CLIENT_DIR)

dropboxClient.o: $(CLIENT_DIR)dropboxClient.cpp
	g++ -c $(CLIENT_DIR)dropboxClient.cpp -Wall -lpthread -lssl -lcrypto

clientProxy.o: $(CLIENT_DIR)clientProxy.cpp
	g++ -c $(CLIENT_DIR)clientProxy.cpp -Wall -lpthread -lssl -lcrypto

dropboxClientDebug: $(CLIENT_DIR)dropboxClient.cpp
	g++ -c $(CLIENT_DIR)dropboxClient.cpp -Wall -g -lpthread -lssl -lcrypto

dropboxServer.o: $(SERVER_DIR)dropboxServer.cpp
	g++ -c $(SERVER_DIR)dropboxServer.cpp -Wall -lpthread -lssl -lcrypto

dropboxServerDebug: $(SERVER_DIR)dropboxServer.cpp
	g++ -c $(SERVER_DIR)dropboxServer.cpp -Wall -g -lpthread -lssl -lcrypto

dropboxUtil.o: $(UTIL_DIR)dropboxUtil.cpp
	g++ -c $(UTIL_DIR)dropboxUtil.cpp -Wall -lssl -lcrypto

dropboxUtilDebug: $(UTIL_DIR)dropboxUtil.cpp
	g++ -c $(UTIL_DIR)dropboxUtil.cpp -Wall -g -lssl -lcrypto

clean:
	rm $(CLIENT_DIR)*.o $(SERVER_DIR)*.o $(UTIL_DIR)*.o proxy client server

certificate:
	openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout KeyFile.pem -out CertFile.pem
