#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "dropboxServer.h"

int main(int argc, char** argv){

    DropboxServer server;
    int comunicationSocket, isRunning=1;

    //Inicialização do servidor
    int port;
    if(argc > 1)port = atoi(argv[1]);
    else port = SERVER_PORT;
    if(server.initialize(port) < 0){
        return -1;
    }
    server.connectToServers();
    //Esperando por conexões e disparando threads
    while(isRunning){
        fprintf(stderr, "Server is listening.\n");
        server.connectToLeader();
        comunicationSocket = server.listenAndAccept();
        server.handleConnection(&comunicationSocket);
    }

    close(server.getSocket());
    return 0;
}
