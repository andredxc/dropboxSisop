#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "dropboxServer.h"

struct classAndSocket{
    DropboxServer* instance;
    int* socket;
};

//--------------------------------------------------FUNÇÕES EXTRAS

/*
*   Constructor
*/
DropboxServer::DropboxServer(){}

/*
*   Cria a thread para atender a comunicação com um cliente, encapsula a chamad a pthread_create
*/
void DropboxServer::handleConnection(int* socket){

	pthread_t comunicationThread;
    struct classAndSocket* arg = (struct classAndSocket*) malloc(sizeof(struct classAndSocket));

    arg->socket = socket;
    arg->instance = this;
	pthread_create(&comunicationThread, NULL, handleConnectionThread, arg);
}

/*
*	Thread que realiza a comunicação entre o servidor e o cliente
*/
void* DropboxServer::handleConnectionThread(void* args){

	struct classAndSocket arg = *(struct classAndSocket*)args;
    bool isRunning = true;
    int n, socket;
    char receiveBuffer[SERVER_MAX_MSG_SIZE];

    DropboxServer server = *arg.instance;
    socket = *arg.socket;

    fprintf(stderr, "DropboxServer - Starting thread with comunication socket = %d\n", socket);

    //Recebe o userId do cliente
    bzero(receiveBuffer, strlen(receiveBuffer));
    if(read(socket, receiveBuffer, sizeof(receiveBuffer)) < 0){
        fprintf(stderr, "DropboxServer - Error receiving userId\n");
        return NULL;
    }
    fprintf(stderr, "RECEIVE BUFFER: %s\n", receiveBuffer);
    if(!server.logInClient(socket, receiveBuffer)){
        fprintf(stderr, "DropboxServer - Error logging user %s in\n", receiveBuffer);
        return NULL;
    }
    //Recebe a mensagem informando se o cliente encontrou o sync_dir
    fprintf(stderr, "DropboxServer - Waiting for synd dir message\n");
    bzero(receiveBuffer, sizeof(receiveBuffer));
    if(read(socket, receiveBuffer, sizeof(receiveBuffer)) < 0){
        fprintf(stderr, "DropboxServer - Error receiving sync_dir information\n");
        return NULL;
    }
    if(atoi(receiveBuffer) == CP_SYNC_DIR_FOUND){
        fprintf(stderr, "DropboxServer - Sync dir found\n");
    }
    else if(atoi(receiveBuffer) == CP_SYNC_DIR_NOT_FOUND){
        fprintf(stderr, "DropboxServer - Sync dir not found\n");
    }
    else{
        fprintf(stderr, "DropboxServer - Error didn't receive sync dir message\n");
        return NULL;
    }
    //Fica no aguardo de mensagens do cliente
    while(isRunning){
        //Recebe comando do cliente
        bzero(receiveBuffer, sizeof(receiveBuffer));
        n = read(socket, receiveBuffer, sizeof(receiveBuffer)) ;
        if(n < 0){
            fprintf(stderr, "DropboxServer - Error receiving comand from client on socket %d\n", socket);
        }
        else{
            fprintf(stderr, "DropboxServer - Comand received from client: \'%s\'\n", receiveBuffer);
        }
    }

    free(args);
	return NULL;
}

/*
*   Adiciona um usuário a estrutura de clientes do servidor
*/
bool DropboxServer::logInClient(int socket, char* userId){

    uint i;
    bool foundUser = false;
    CLIENT newClient;

    fprintf(stderr, "logInClient %d and %s\n", socket, userId);

    //Verifica se usuário não atingiu o limite de conexões
    for (i = 0; i < _clients.size(); i++) {
        if(strcmp(userId, _clients.at(i).userId) == 0){
            foundUser = true;
            if(_clients.at(i).devices[0] > 0 && _clients.at(i).devices[1] > 0){
                fprintf(stderr, "DropboxServer - User already has 2 connections\n");
                return false;
            }
            break;
        }
    }
    if(foundUser){
        //Usuário velho
        _clients.at(i).logged_in = 1;
        if(_clients.at(i).devices[0] < 0){
            _clients.at(i).devices[0] = socket;
        }
        else if(_clients.at(i).devices[1] < 0){
            _clients.at(i).devices[1] = socket;
        }
        else{
            fprintf(stderr, "DropboxServer - Internal error, one position sould be empty\n");
            return false;
        }
    }
    else{
        //Usuário novo
        newClient.devices[0] = socket;
        newClient.devices[1] = -1;
        newClient.logged_in = 1;
        strncpy(newClient.userId, userId, sizeof(newClient.userId));
        _clients.push_back(newClient);

    }
    return true;
}

/*
*   Faz liste(), accept() e retorna o socket de comunicação
*/
int DropboxServer::listenAndAccept(){

    struct sockaddr_in clientAddress;
    socklen_t clientLength;
    int newSocket;

    clientLength = sizeof(struct sockaddr_in);
    listen(_serverSocket, SERVER_BACKLOG);
    newSocket = accept(_serverSocket, (struct sockaddr*) &clientAddress, &clientLength);

    if(newSocket == -1){
        fprintf(stderr, "DropboxServer - Error accepting connection\n");
    }

    return newSocket;
}

/*
*   Faz o socket() e bind() do servidor
*   Retorno: socket ou -1 (em caso de erro)
*/
int DropboxServer::initialize(){

    struct sockaddr_in serverAddress;
    //Inicializando socket
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(_serverSocket == -1){
        fprintf(stderr, "DropboxServer - Error initializing socket\n");
        return -1;
    }
    //Inicializando struct do socket do servidor
    serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(SERVER_PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serverAddress.sin_zero), 8);
    //Faz o bind
    if(bind(_serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr, "DropboxServer - Error binding server\n");
        return -1;
    }
    return _serverSocket;
}



int DropboxServer::getSocket(){
    return _serverSocket;
}
