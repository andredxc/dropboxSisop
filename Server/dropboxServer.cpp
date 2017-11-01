#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "dropboxServer.h"

/*Utilizada na criação da thread pois a função tem que ser static*/
struct classAndSocket{
    DropboxServer* instance;
    int* socket;
};

//--------------------------------------------------FUNÇÕES EXTRAS

/*Constructor*/
DropboxServer::DropboxServer(){
    pthread_mutex_init(&_logInMutex, NULL);
    pthread_mutex_init(&_logOutMutex, NULL);
}

/*Cria a thread para atender a comunicação com um cliente, encapsula a chamad a pthread_create*/
void DropboxServer::handleConnection(int* socket){

	pthread_t comunicationThread;
    struct classAndSocket* arg = (struct classAndSocket*) malloc(sizeof(struct classAndSocket));

    arg->socket = socket;
    arg->instance = this;
	pthread_create(&comunicationThread, NULL, handleConnectionThread, arg);
}

/*Thread que realiza a comunicação entre o servidor e o cliente*/
void* DropboxServer::handleConnectionThread(void* args){

	struct classAndSocket arg = *(struct classAndSocket*)args;
    bool isRunning = true;
    int n, socket;
    char receiveBuffer[CP_MAX_MSG_SIZE], userId[MAXNAME];

    DropboxServer *server = arg.instance;
    socket = *arg.socket;

    printf("DropboxServer - Starting thread with comunication socket = %d\n", socket);

    //Recebe o userId do cliente
    bzero(receiveBuffer, sizeof(receiveBuffer));
    if(read(socket, receiveBuffer, sizeof(receiveBuffer)) < 0){
        printf("Socket %d - Error receiving userId\n", socket);
        server->closeConnection(socket);
        free(args);
        return NULL;
    }
    fprintf(stderr, "USERID: %s\n", receiveBuffer);
    //Valida o userId recebido
    if(server->logInClient(socket, receiveBuffer)){
		//Logou com sucesso
		printf("Socket %d - User \"%s\" just logged in\n", socket, receiveBuffer);
		sendInteger(socket, CP_LOGIN_SUCCESSFUL);
        strncpy(userId, receiveBuffer, sizeof(userId));
    }
    else{
		//Falha no login
		printf("Socket %d - Error logging user %s in\n", socket, receiveBuffer);
        sendInteger(socket, CP_LOGIN_FAILED);
        server->closeConnection(socket);
        free(args);
        return NULL;
	}
    //Espera pela mensagem informando se o cliente encontrou o sync_dir
    printf("Socket %d - Waiting for sync dir message\n", socket);
    bzero(receiveBuffer, sizeof(receiveBuffer));
    if(read(socket, receiveBuffer, sizeof(receiveBuffer)) < 0){
        printf("Socket %d - Error receiving sync_dir information\n", socket);
        server->logOutClient(socket, userId);
        server->closeConnection(socket);
        free(args);
        return NULL;
    }
    if(atoi(receiveBuffer) == CP_SYNC_DIR_FOUND){
        printf("Socket %d - Sync dir found\n", socket);
    }
    else if(atoi(receiveBuffer) == CP_SYNC_DIR_NOT_FOUND){
        printf("Socket %d - Sync dir not found\n", socket);
    }
    else{
        printf("Socket %d - Error didn't receive sync dir message\n", socket);
        server->logOutClient(socket, userId);
        server->closeConnection(socket);
        free(args);
        return NULL;
    }
    //Fica no aguardo de mensagens do cliente
    while(isRunning){
        //Recebe comando do cliente
        bzero(receiveBuffer, sizeof(receiveBuffer));
        n = read(socket, receiveBuffer, sizeof(receiveBuffer)) ;
        if(n < 0){
            printf("Socket %d - Error receiving comand from client\n", socket);
        }
        else{
            printf("Socket %d - Comand received from client: \'%s\'\n", socket, receiveBuffer);
        }
    }

    free(args);
	return NULL;
}

/*Adiciona um usuário a estrutura de clientes do servidor*/
bool DropboxServer::logInClient(int socket, char* userId){

    uint i;
    bool foundUser = false;
    CLIENT newClient;

    pthread_mutex_lock(&_logInMutex);
    //Verifica se usuário não atingiu o limite de conexões
    for (i = 0; i < _clients.size(); i++) {
        if(strcmp(userId, _clients.at(i).userId) == 0){
            foundUser = true;
            if(_clients.at(i).devices[0] > 0 && _clients.at(i).devices[1] > 0){
                printf("Socket %d - User already has 2 connections\n", socket);
                pthread_mutex_unlock(&_logInMutex);
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
            printf("Socket %d - Internal error, one position sould be empty\n", socket);
            pthread_mutex_unlock(&_logInMutex);
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
    pthread_mutex_unlock(&_logInMutex);
    return true;
}

/*Remove um usuário da estrutura de clientes do servidor*/
void DropboxServer::logOutClient(int socket, char* userId){

    uint i;

    //Verifica se usuário não atingiu o limite de conexões
    for (i = 0; i < _clients.size(); i++) {
        if(strcmp(userId, _clients.at(i).userId) == 0){
            pthread_mutex_lock(&_logOutMutex);
            if(_clients.at(i).devices[0] == socket){
                _clients.at(i).devices[0] = -1;
            }
            else if(_clients.at(i).devices[1] == socket){
                _clients.at(i).devices[1] = -1;
            }
            else{
                printf("Internal error on logout\n");
            }
            pthread_mutex_unlock(&_logOutMutex);
            return;
        }
    }
    printf("Internal error 2 on logout\n");
}

/*Faz liste(), accept() e retorna o socket de comunicação*/
int DropboxServer::listenAndAccept(){

    struct sockaddr_in clientAddress;
    socklen_t clientLength;
    int newSocket;

    clientLength = sizeof(struct sockaddr_in);
    listen(_serverSocket, SERVER_BACKLOG);
    newSocket = accept(_serverSocket, (struct sockaddr*) &clientAddress, &clientLength);

    if(newSocket == -1){
        printf("DropboxServer - Error accepting connection\n");
    }

    return newSocket;
}

/*Faz o socket() e bind() do servidor, retorna -1 ou o valor do socket*/
int DropboxServer::initialize(){

    struct sockaddr_in serverAddress;
    //Inicializando socket
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(_serverSocket == -1){
        printf("DropboxServer - Error initializing socket\n");
        return -1;
    }
    //Inicializando struct do socket do servidor
    serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(SERVER_PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serverAddress.sin_zero), 8);
    //Faz o bind
    if(bind(_serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){
        printf("DropboxServer - Error binding server\n");
        return -1;
    }
    return _serverSocket;
}

/*Fecha a conexão com um cliente*/
void DropboxServer::closeConnection(int socket){

	printf("DropboxServer - Closing connection with socket %d\n", socket);
	close(socket);
}

int DropboxServer::getSocket(){ return _serverSocket; }
