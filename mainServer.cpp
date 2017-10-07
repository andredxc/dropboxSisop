#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "dropboxServer.h"

#define SERVER_BACKLOG 20


int main(){

    int comunicationSocket, isRunning=1;
    struct sockaddr_in clientAddress;
    socklen_t clientLength;
    char comunicationBuffer[256];
    DropboxServer server;

    //Inicialização do servidor
    if(server.initialize() < 0){
        return -1;
    }
    //Esperando por conexões
    fprintf(stderr, "Server is listening.\n");
    
    listen(server.getSocket(), SERVER_BACKLOG);
    comunicationSocket = accept(server.getSocket(), (struct sockaddr*) &clientAddress, &clientLength);

    if(read(comunicationSocket, comunicationBuffer, sizeof(comunicationBuffer)) < 0){
        fprintf(stderr, "Error reading from %d:%d\n", clientAddress.sin_addr.s_addr, clientAddress.sin_port);
        return -1;
    }

    close(comunicationSocket);
    close(server.getSocket());
    return 0;
}
