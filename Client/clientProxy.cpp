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


int ClientProxy::initialize_clientConnection(int port){

    struct sockaddr_in proxyAddress;
    const SSL_METHOD *method;

    //Inicializa SSL
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();

    method = SSLv23_server_method();
    _ctx = SSL_CTX_new(method);
    if (_ctx == NULL){
      ERR_print_errors_fp(stderr);
      abort();
    }

    //Carrega certificados
    SSL_CTX_use_certificate_file(_ctx, "CertFile.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(_ctx, "KeyFile.pem", SSL_FILETYPE_PEM);

    //Inicializa socket que recebe dados do cliente (caso não consiga, interrompe execução e retorna erro)
    _clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if(_clientSocket == -1){
        printf("ClientProxy - Error initializing socket\n");
        return -1;
    }

    //Inicializa struct do socket
    proxyAddress.sin_family = AF_INET; // communication domain do socket criado
    proxyAddress.sin_port = htons(port); // porta do socket
    proxyAddress.sin_addr.s_addr = INADDR_ANY; // container genérico
    bzero(&(proxyAddress.sin_zero), 8); // completa os 16 bits de serverAddress com 8 0's (trabalha-se com 16 bits, mas
                                         // sin_family tem 2 bits, sin_port tem 2 e sin_addr tem 4)
    //Faz o bind (atribui identidade ao socket)
    if(bind(_clientSocket, (struct sockaddr*) &proxyAddress, sizeof(proxyAddress)) < 0){
        printf("ClientProxy - Error binding server\n");
        return -1;
    }

    return _clientSocket;
}

/*Faz listen(), accept() e retorna o socket de comunicação*/
int ClientProxy::listenAndAccept(){

    struct sockaddr_in clientAddress;
    socklen_t clientLength;
    int newSocket;
    char receiveBuffer[CP_MAX_MSG_SIZE];

    clientLength = sizeof(struct sockaddr_in);
    listen(_clientSocket, SERVER_BACKLOG);
    newSocket = accept(_clientSocket, (struct sockaddr*) &clientAddress, &clientLength);
    if(newSocket > 0){
        _isClientConnected = true;
        fprintf(stderr, "ClientProxy - Client connected.\n");
    }

   //Amarra Socket com SSL
    _sslClient = SSL_new(_ctx);
    SSL_set_fd(_sslClient, newSocket);
    int ssl_err = SSL_accept(_sslClient);
    if(ssl_err <= 0){
        //Erro aconteceu, fecha o SSL
        printf("[main] Erro com SSL\n");
        exit(1);
    }

    /// Loga com o cliente

    fprintf(stderr, "DropboxServer - Starting thread with comunication socket = %d\n", newSocket);
    // Recebe o userId do cliente
    bzero(receiveBuffer, sizeof(receiveBuffer));
    if(SSL_read(_sslClient, receiveBuffer, sizeof(receiveBuffer)) <= 0){
        fprintf(stderr, "Socket %d - Error receiving userId\n", newSocket);
        close_clientConnection();
        return NULL;
    }
    // adquire nome para usos futuros (novas conexões, por exemplo)
    _clientName = std::string(receiveBuffer);

    fprintf(stderr, "User %s logging in \n", receiveBuffer);

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

/*Cria a thread para atender a comunicação com um cliente, encapsula a chamada a pthread_create*/
void* ClientProxy::handle_clientConnection(void *arg){

    char buffer[CP_MAX_MSG_SIZE];
    int isRunning = 1;
    ClientProxy *proxy = (ClientProxy*) arg;
    int communicationSocket;
    std::pair<std::string, int> serverHostPort;
    const char * serverChar;

    while(isRunning){
        // Recebe informação do cliente e manda ao servidor
        if(proxy->getServerConnected() == true && proxy->getClientConnected() == true){
            if(SSL_read(proxy->get_communicationSocket(), buffer, sizeof(buffer)) <= 0)
            {
                fprintf(stderr, "ClientProxy - Connection with Client died.\n");
                proxy->close_serverConnection();
                proxy->close_clientConnection();
                proxy->set_error(1);
                pthread_exit(NULL);
            }
            else{
                if(SSL_write(proxy->getServerSocket(), buffer, sizeof(buffer)) <= 0){
                    proxy->close_serverConnection();
                    fprintf(stderr, "ClientProxy - Trying to connect to Server.\n");
                }
            }
        }
        if(proxy->get_error()){
            pthread_exit(NULL);
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
        // Recebe informação do servidor e manda ao cliente
        if(proxy->getServerConnected() == true && proxy->getClientConnected() == true){
            if(SSL_read(proxy->getServerSocket(), buffer, sizeof(buffer)) <= 0)
            {
                fprintf(stderr, "ClientProxy - Connection with Server not available\n");
                proxy->close_serverConnection();
            }
            else{
                if(SSL_write(proxy->get_communicationSocket(), buffer, sizeof(buffer)) <= 0){
                    fprintf(stderr, "ClientProxy - Connection with Client died.\n");
                    proxy->close_serverConnection();
                    proxy->close_clientConnection();
                    proxy->set_error(1);
                    pthread_exit(NULL);
                    return NULL;
                }
            }
        }
        else if(proxy->getClientConnected() == true){
            proxy->reset_serverList();
            while(!proxy->compare_itEnd() && !proxy->getServerConnected()){
                serverHostPort = proxy->get_currentServerName();
                serverChar = (serverHostPort.first).c_str(); // converte string a array de char
                fprintf(stderr, "ClientProxy - Trying to connect to Server (host: %s, port: %d).\n", serverChar, serverHostPort.second);
                proxy->connect_server((char *)serverChar, serverHostPort.second);
                proxy->increment_currentServer();
            }
            if(proxy->compare_itEnd()){
                fprintf(stderr, "ClientProxy - Couldn't connect to any Server.\n");
                proxy->close_serverConnection();
                proxy->close_clientConnection();
                proxy->set_error(1);
                pthread_exit(NULL);
                return NULL;
            }
            fprintf(stderr, "ClientProxy - Connected to Server.\n");
        }
        if(proxy->get_error()){
            pthread_exit(NULL);
        }
    }
    return NULL;
}

std::pair<std::string, int> ClientProxy::get_currentServerName(){ return *_it; }
void ClientProxy::reset_serverList(){
    _it = _server_list.begin();
}
void ClientProxy::increment_currentServer(){
    _it++;
}
bool ClientProxy::compare_itEnd(){
    if(_it == _server_list.end()) return true;
    return false;
}

/* Termina a conexão */
void ClientProxy::close_serverConnection(){

    if(!sendInteger(_sslServer, CP_CLIENT_END_CONNECTION)){
        fprintf(stderr, "ClientProxy - Error sending close connection message\n");
    }
    else{
        fprintf(stderr, "ClientProxy - Closing connection with server\n");
    }
    _isServerConnected = false;
}

/*Fecha a conexão com um cliente*/
void ClientProxy::close_clientConnection(){

  	printf("ClientProxy - Closing connection with client socket %d\n", _communicationSocket);
    _isClientConnected = false;
  	close(_communicationSocket);
}

void ClientProxy::lock_socket(){ pthread_mutex_lock(&_communicationMutex); }
void ClientProxy::unlock_socket(){ pthread_mutex_unlock(&_communicationMutex); }

int ClientProxy::getClientConnected(){
    return _isClientConnected;
}

int ClientProxy::getServerConnected(){
    return _isServerConnected;
}

SSL *ClientProxy::getServerSocket(){
    return _sslServer;
}

int ClientProxy::get_error(){ return _error; }

void ClientProxy::set_error(int error){ _error = error; }

SSL *ClientProxy::get_communicationSocket(){
    return _sslClient;
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
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    char buffer[CP_MAX_MSG_SIZE];

    // Inicializa o SSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method = SSLv23_client_method();

    ctx = SSL_CTX_new(method);
    if (ctx == NULL){
      ERR_print_errors_fp(stderr);
      abort();
    }

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
    fprintf(stderr, "ClientProxy - Connected to Server at %s on port %d\n", inet_ntoa(serverAddress.sin_addr), ntohs(serverAddress.sin_port));

    _sslServer = SSL_new(ctx);
    SSL_set_fd(_sslServer,_serverSocket);
    if(SSL_connect(_sslServer) == -1){
        ERR_print_errors_fp(stderr);
        return -1;
    }
    X509 *cert;
    char *line;
    cert = SSL_get_peer_certificate(_sslServer);
    if (cert != NULL){
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("\nSubject: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n\n", line);
    }

    // Loga com cliente
    bzero(buffer, sizeof(buffer));
    strncpy(buffer, (char *) _clientName.c_str(), sizeof(buffer));
    if(SSL_write(_sslServer, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "ClientProxy - Error logging in with client %s\n", (char *)_clientName.c_str());
        return -1;
    }

    bzero(buffer, sizeof(buffer));
    if(SSL_read(_sslServer, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "ClientProxy - Error logging in with client %s\n", (char *)_clientName.c_str());
        return -1;
    }
    if(_first_login && SSL_write(get_communicationSocket(), buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "ClientProxy - Error logging in with client %s\n", (char *)_clientName.c_str());
        return -1;
    }
    _first_login = false;

    // Usuário logou
    if(atoi(buffer) == CP_LOGIN_SUCCESSFUL){
        fprintf(stderr, "ClientProxy - User %s successfully logged in.\n", (char *)_clientName.c_str());
    }
    // Usuário não conseguiu logar
    else if(atoi(buffer) == CP_LOGIN_FAILED){
        fprintf(stderr, "ClientProxy - Error logging in with client %s\n", (char *)_clientName.c_str());
        return -1;
    }
    else{
    // Falha no login
        fprintf(stderr, "ClientProxy - Unknown Error (unexpected message)\n");
        return -1;
    }

    _isServerConnected = true;

    return _serverSocket;
}


void ClientProxy::lockClientSocket(){

    pthread_mutex_lock(&_clientMutex);
}

void ClientProxy::unlockClientSocket(){

    pthread_mutex_unlock(&_clientMutex);
}

void ClientProxy::lockServerSocket(){

    pthread_mutex_lock(&_serverMutex);
}

void ClientProxy::unlockServerSocket(){

    pthread_mutex_unlock(&_serverMutex);
}
