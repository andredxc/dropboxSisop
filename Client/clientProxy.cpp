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
#include <arpa/inet.h>
#include <dirent.h>
#include <string>
#include <time.h>
#include "clientProxy.h"
#include "../Util/dropboxUtil.h"
#include <sys/ioctl.h>
#include <fcntl.h> /* Added for the nonblocking socket */
#include <iostream>
#include <time.h>


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

/*Faz listen(), accept() e retorna o socket de comunicação*/
int ClientProxy::listenAndAccept(){

    struct sockaddr_in clientAddress;
    socklen_t clientLength;
    int newSocket;

    clientLength = sizeof(struct sockaddr_in);
    listen(_clientSocket, SERVER_BACKLOG);

    newSocket = accept(_clientSocket, (struct sockaddr*) &clientAddress, &clientLength);

    fprintf(stderr, "ClientProxy - Client connected.\n");

    return newSocket;
}

pthread_t *ClientProxy::clientWatcher(){
    pthread_t *messagefromClient = (pthread_t *) malloc(sizeof(pthread_t));
    pthread_create(messagefromClient, NULL, handle_clientConnection, (void *)this);
    return messagefromClient;
}

pthread_t *ClientProxy::serverWatcher(){
    pthread_t *messagefromServer = (pthread_t *) malloc(sizeof(pthread_t));
    pthread_create(messagefromServer, NULL, handle_serverConnection, (void *)this);
    return messagefromServer;
}


// Vê se socket ainda está ativo
int ClientProxy::check_socket(int socket){

    int error = 0;
    socklen_t len = sizeof(error);
    int retval = getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &len);

    // Verifica se conseguiu condição de erro
    if (retval != 0) {
        fprintf(stderr, "ClientProxy - error getting socket error code: %d\n", retval);
        return 0;
    }
    // Verifica se ocorreu erro
    else if (error != 0) {
      fprintf(stderr, "ClientProxy - socket error: %d\n", error);
      return 0;
    }
    return 1;
}

// TODO: if not functional, communicationSocket = proxy_listenAndAccept;


/*Cria a thread para atender a comunicação com um cliente, encapsula a chamada a pthread_create*/
void* ClientProxy::handle_clientConnection(void *arg){

    char buffer[CP_MAX_MSG_SIZE];
    int isRunning = 1;
    ClientProxy *proxy = (ClientProxy*) arg;
    int communicationSocket;

    while(isRunning){
        // Recebe informação do cliente e manda ao servidor
        //TODO: proxy->check_socket(communicationSocket);
        if(read(proxy->get_communicationSocket(), buffer, sizeof(buffer)) <= 0)
        {
            fprintf(stderr, "ClientProxy - Connection with Client not available. Waiting for new connection.\n");
            proxy->close_serverConnection();
            if((communicationSocket = proxy->listenAndAccept()) <= 0)
            {
                fprintf(stderr, "ClientProxy - Connection with Client not available. Couldn't connect with client.\n");
                return NULL;
            }
            else proxy->set_communicationSocket(communicationSocket);
            proxy->unlock_socket();
        }
        else if(write(proxy->getServerSocket(), buffer, sizeof(buffer)) <= 0){
            fprintf(stderr, "ClientProxy - Error sending information from Client to Server.\n");
        }
    }
    return NULL;
}

/*Cria a thread para atender a comunicação com um cliente, encapsula a chamada a pthread_create*/
void* ClientProxy::handle_serverConnection(void *arg){

    char buffer[CP_MAX_MSG_SIZE];
    int isRunning = 1;
    ClientProxy *proxy = (ClientProxy*) arg;
    int communicationSocket;
    std::pair<std::string, int> serverHostPort;
    const char * serverChar;

    while(isRunning){
        //std::cerr << "CLIENT: " << proxy->check_socket(proxy->getClientSocket()) << "\n";
        //std::cerr << "SERVER: " << proxy->check_socket(proxy->getServerSocket()) << "\n";
        //TODO: proxy->check_socket(communicationSocket);
        // Recebe informação do servidor e manda ao cliente
        if(read(proxy->getServerSocket(), buffer, sizeof(buffer)) <= 0)
        {
            fprintf(stderr, "ClientProxy - Trying to connect to Server.\n");
            do{
            proxy->close_serverConnection();
            serverHostPort = proxy->get_currentServerName();
            serverChar = (serverHostPort.first).c_str(); // converte string a array de char
            proxy->increment_currentServer();
          } while(proxy->connect_server((char *)serverChar, serverHostPort.second) <= 0);
            fprintf(stderr, "ClientProxy - Connected to Server.\n");
        }
        //TODO: proxy->check_socket(communicationSocket);
        else if(write(proxy->get_communicationSocket(), buffer, sizeof(buffer)) <= 0){
            fprintf(stderr, "ClientProxy - Connection with Client not available. Waiting for new connection.\n");
            proxy->lock_socket();
            //proxy->closeConnection(proxy->get_communicationSocket());
            if((communicationSocket = proxy->listenAndAccept()) <= 0)
            {
                fprintf(stderr, "ClientProxy - Connection with Client not available. Couldn't connect with client.\n");
                return NULL;
            }
            else proxy->set_communicationSocket(communicationSocket);
            proxy->unlock_socket();
        }
    }
    return NULL;
}

std::pair<std::string, int> ClientProxy::get_currentServerName(){ return *_it; }
void ClientProxy::increment_currentServer(){
    _it++;
    if(_it == _server_list.end()) _it = _server_list.begin();
}

/* Termina a conexão */
void ClientProxy::close_serverConnection(){

    if(!sendInteger(_serverSocket, CP_CLIENT_END_CONNECTION)){
        fprintf(stderr, "ClientProxy - Error sending close connection message\n");
    }
    else{
        fprintf(stderr, "ClientProxy - Closing connection with server\n");
    }
    _isConnected = false;
}

void ClientProxy::lock_socket(){ pthread_mutex_lock(&_Mutex); }
void ClientProxy::unlock_socket(){ pthread_mutex_unlock(&_Mutex); }

int ClientProxy::getClientSocket(){
    return _clientSocket;
}

int ClientProxy::getServerSocket(){
    return _serverSocket;
}

int ClientProxy::get_communicationSocket(){
    return _communicationSocket;
}
void ClientProxy::set_communicationSocket(int communicationSocket){
    _communicationSocket = communicationSocket;
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
