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
#include <sys/ioctl.h>
#include <fcntl.h> /* Added for the nonblocking socket */

int ClientProxy::initialize_clientConnection(){

    struct sockaddr_in proxyAddress;

    //Inicializa socket que recebe dados do cliente (caso não consiga, interrompe execução e retorna erro)
    _clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    fcntl(_clientSocket, F_SETFL, O_NONBLOCK); /* Transforma em não bloqueante*/

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

    while((newSocket = accept(_clientSocket, (struct sockaddr*) &clientAddress, &clientLength)) < 0){ }
    fcntl(newSocket, F_SETFL, O_NONBLOCK); /* Transforma em não bloqueante*/

    return newSocket;
}

pthread_t *ClientProxy::communicationWatcher(){
    pthread_t *messagefromClient = (pthread_t *) malloc(sizeof(pthread_t));
    pthread_create(messagefromClient, NULL, handle_connection, (void *)this);
    return messagefromClient;
}

/*Cria a thread para atender a comunicação com um cliente, encapsula a chamada a pthread_create*/
void* ClientProxy::handle_connection(void *arg){

    char buffer[CP_MAX_MSG_SIZE], filePath[512];
    int isRunning = 1;
    ClientProxy *proxy = (ClientProxy*) arg;
    int comunicationSocket;
    comunicationSocket = proxy->listenAndAccept();

    while(isRunning){
        proxy->lock_socket();
        // Recebe informação do cliente

        int iMode = 1;
        fcntl(comunicationSocket, F_SETFL, O_NONBLOCK); /* Transforma em não bloqueante*/
        //Espera por conexões do cliente e dispara threads
        if(recv(comunicationSocket, buffer, sizeof(buffer), 0) < 0){ }
        else{
            if(write(proxy->getServerSocket(), buffer, sizeof(buffer)) < 0){
                fprintf(stderr, "DropboxClient - Error receiving information to Server.\n");
            }
        }
        proxy->unlock_socket();

        proxy->lock_socket();
        if(read(proxy->getServerSocket(), buffer, sizeof(buffer)) < 0){ }
        else{
            if(write(comunicationSocket, buffer, sizeof(buffer)) < 0){
                fprintf(stderr, "DropboxClient - Error sending information to Client.\n");
            }
        }
        proxy->unlock_socket();
    }
}


void ClientProxy::lock_socket(){ pthread_mutex_lock(&_Mutex); }
void ClientProxy::unlock_socket(){ pthread_mutex_unlock(&_Mutex); }

int ClientProxy::getClientSocket(){
    return _clientSocket;
}

int ClientProxy::getServerSocket(){
    return _serverSocket;
}

void ClientProxy::set_clientThreadState(int thread_state){
    _clientThreadState = thread_state;
}

int ClientProxy::get_clientThreadState(){
    return _clientThreadState;
}

void ClientProxy::set_serverThreadState(int thread_state){
    _clientThreadState = thread_state;
}

int ClientProxy::get_serverThreadState(){
    return _serverThreadState;
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

    fcntl(_serverSocket, F_SETFL, O_NONBLOCK); /* Transforma em não bloqueante*/

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
    while(connect(_serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){    }
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
