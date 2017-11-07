#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "dropboxServer.h"

int main(){

    DropboxServer server;
    int comunicationSocket, isRunning=1;

    //Inicialização do servidor
    if(server.initialize() < 0){
        return -1;
    }

    //Esperando por conexões e disparando threads
    while(isRunning){
        fprintf(stderr, "Server is listening.\n");
        comunicationSocket = server.listenAndAccept();
        server.handleConnection(&comunicationSocket);
    }

    close(server.getSocket());
    return 0;
}
