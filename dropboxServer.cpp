#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "dropboxServer.h"

//--------------------------------------------------FUNÇÕES EXTRAS

/*
*   Constructor
*/
DropboxServer::DropboxServer(){
}

/*
*   Atende à conexão de um cliente
*/
void* DropboxServer::connectionHandler(void* args){

    return NULL;
}

/*
*   Faz o socket() e bind() do servidor
*   Retorno: socket ou -1 (em caso de erro)
*/
int DropboxServer::initialize(){

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

int DropboxServer::getSocket(){
    return serverSocket;
}
