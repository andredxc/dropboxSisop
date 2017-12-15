#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/inotify.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/stat.h>
#include <iostream>
#include <arpa/inet.h>
#include <dirent.h>
#include <string>
#include <time.h>
#include "clientProxy.h"
#include "../Util/dropboxUtil.h"

int ClientProxy::initialize_clientConnection(){

    struct sockaddr_in proxyAddress;

    //Inicializa socket que recebe dados do cliente (caso não consiga, interrompe execução e retorna erro)
    _clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(_clientSocket == -1){
        printf("ClientProxy - Error initializing socket\n");
        return -1;
    }

    //Inicializa struct do socket
    proxyAddress.sin_family = AF_INET; // communication domain do socket criado
    proxyAddress.sin_port = htons(PROXY_PORT); // porta do socket
    proxyAddress.sin_addr.s_addr = INADDR_ANY; // container genérico
    bzero(&(proxyAddress.sin_zero), 8); // completa os 16 bits de serverAddress com 8 0's (trabalha-se com 16 bits, mas
                                         // sin_family tem 2 bits, sin_port tem 2 e sin_addr tem 4)
    //Faz o bind (atribui identidade ao socket)
    if(bind(_clientSocket, (struct sockaddr*) &proxyAddress, sizeof(proxyAddress)) < 0){
        printf("ClientProxy - Error binding server\n");
        return -1;
    }

    //Recupera as estruturas do cliente
    //recoverData();

    return _clientSocket;
}

/*Faz liste(), accept() e retorna o socket de comunicação*/
int ClientProxy::listenAndAccept(){

    struct sockaddr_in clientAddress;
    socklen_t clientLength;
    int newSocket;

    clientLength = sizeof(struct sockaddr_in);
    listen(_clientSocket, SERVER_BACKLOG);
    newSocket = accept(_clientSocket, (struct sockaddr*) &clientAddress, &clientLength);

    if(newSocket == -1){
        printf("ClientProxy - Error accepting connection\n");
    }

    return newSocket;
}

/*Cria a thread para atender a comunicação com um cliente, encapsula a chamada a pthread_create*/
int ClientProxy::handle_clientConnection(int* socket){

    char buffer[CP_MAX_MSG_SIZE], filePath[512];

    pthread_t comunicationThread;
    // struct classAndSocket* arg = (struct classAndSocket*) malloc(sizeof(struct classAndSocket));

    //if(read(socket, buffer, sizeof(buffer)) < 0){
    //    fprintf(stderr, "Socket %d - Error receiving file size\n", socket);
    //    return;
    //}
    //fileSize = atoi(buffer);
    //if(!sendInteger(socket, CP_CLIENT_SEND_FILE_SIZE_ACK)){
    //    fprintf(stderr, "Socket %d - Error sending file size ack\n", socket);
    //    return;
    //}
    std::cout << socket;
    return -1;//socket;
}

int ClientProxy::get_clientSocket(){ return _serverSocket; }

/*Estabelece a conexão entre host (endereço servidor) e port (porta da conexão)*/
int ClientProxy::connect_server(char* host, int port){

    struct hostent *server;
    struct sockaddr_in serverAddress;

    // tenta adquirir IP address (caso não consiga, interrompe execução e retorna erro)
    server = gethostbyname(host);
    if(server == NULL){
        fprintf(stderr, "ClientProxy - Server \'%s\' doesn't exist\n", host);
        return -1;
    }

    //Inicializa socket (caso não consiga, interrompe execução e retorna erro)
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(_serverSocket == -1){
        fprintf(stderr, "ClientProxy - Error creating socket\n");
        return -1;
    }
    //Inicializa struct do socket
    serverAddress.sin_family = AF_INET; // communication domain do socket criado
    serverAddress.sin_port = htons(port); // porta do socket
    serverAddress.sin_addr = *((struct in_addr*) server->h_addr); // endereço IP para o socket
    bzero(&(serverAddress.sin_zero), 8); // completa os 16 bits de serverAddress com 8 0's (trabalha-se com 16 bits, mas
                                         // sin_family tem 2 bits, sin_port tem 2 e sin_addr tem 4)
    // tente conectar ao servidor (caso não consiga, interrompe execução e retorna erro)
    if(connect(_serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr, "ClientProxy - Couldn't connect to server at %s on port %d\n", inet_ntoa(serverAddress.sin_addr), ntohs(serverAddress.sin_port));
        close(_serverSocket);
        return -1;
    }
    _isConnected = true;
    return _serverSocket;
}

int ClientProxy::send_message(int message)
{
    // Envia o tamanho do arquivo
    //if(!sendInteger(_socket, fileSize)){
    //    fprintf(stderr, "DropboxClient - Error sending file size\n");
    //    fclose(file);
    //    return;
    //}

  //  if(!receiveExpectedInt(_socket, CP_CLIENT_SEND_FILE_SIZE_ACK)){
    //    fprintf(stderr, "DropboxClient - Error receiving size confirmation from server\n");
    //    fclose(file);
    //    return;
    //}
    return -1;
}
