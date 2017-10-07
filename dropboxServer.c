#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "dropboxServer.h"

#define SERVER_PORT 4000
#define SERVER_BACKLOG 5


int main(){

    int serverSocket, comunicationSocket;
    struct sockaddr_in clientAddress;
    socklen_t clientLength;
    char comunicationBuffer[256];

    //Inicialização do servidor
    if((serverSocket = initServer()) < 0){
        return -1;
    }
    //Esperando por conexões
    listen(serverSocket, SERVER_BACKLOG);
    fprintf(stderr, "Server is listening.\n");



    comunicationSocket = accept(serverSocket, (struct sockaddr*) &clientAddress, &clientLength);

    if(read(comunicationSocket, comunicationBuffer, sizeof(comunicationBuffer)) < 0){
        fprintf(stderr, "Error reading from %d:%d\n", clientAddress.sin_addr.s_addr, clientAddress.sin_port);
        return -1;
    }

    close(comunicationSocket);
    close(serverSocket);
    return 0;
}

//--------------------------------------------------FUNÇÕES EXTRAS

/*
*   Faz o socket() e bind() do servidor
*   Retorno: socket ou -1 (em caso de erro)
*/
int initServer(){

    int serverSocket;
    struct sockaddr_in serverAddress;
    //Inicializando socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == -1){
        fprintf(stderr, "Error initializing socket\n");
        return -1;
    }
    //Inicializando struct do socket do servidor
    serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(SERVER_PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serverAddress.sin_zero), 8);
    //Faz o bind
    if(bind(serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr, "Error binding server\n");
        return -1;
    }
    return serverSocket;
}
