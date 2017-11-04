#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/stat.h>
#include "dropboxServer.h"
#include "../Util/dropboxUtil.h"

/*Utilizada na criação da thread pois a função tem que ser static*/
struct classAndSocket{
    DropboxServer* instance;
    int* socket;
};

/*Constructor*/
DropboxServer::DropboxServer(){
    pthread_mutex_init(&_logInMutex, NULL);
    pthread_mutex_init(&_logOutMutex, NULL);
}

//--------------------------------------------------FUNÇÕES DEFINIDAS NA ESPECIFICAÇÃO

/*Sincroniza a parsta sync_dir com o cliente*/
void DropboxServer::sync_server(int socket, char* userId){

    char dirPath[512];
    int errorCode;

    //Cria os diretórios referentes ao sync_dir
    if(access(SERVER_SYNC_DIR_PATH, F_OK) == -1){
        //Diretório dos sync_dir não existe
        if((errorCode = mkdir(SERVER_SYNC_DIR_PATH, S_IRWXU)) != 0){
            fprintf(stderr, "Socket %d - Error %d creating %s\n", socket, errorCode, SERVER_SYNC_DIR_PATH);
            return;
        }
    }
    snprintf(dirPath, sizeof(dirPath), "%ssync_dir_%s", SERVER_SYNC_DIR_PATH, userId);
    if(access(dirPath, F_OK) == -1){
        //Diretório do sync_dir desse usuário não existe
        fprintf(stderr, "Socket %d - Creating directory \'%s\'\n", socket, dirPath);
        if((errorCode = mkdir(dirPath, S_IRWXU)) != 0){
            fprintf(stderr, "Socket %d - Error %d creating %s\n", socket, errorCode, dirPath);
            return;
        }
    }
    //Sincroniza diretórios

}

/*Recebe um arquivo do cliente*/
void DropboxServer::receive_file(int socket, char* userId, char* file){

    char buffer[CP_MAX_MSG_SIZE], filePath[512];
    int fileSize, iterations, sizeReceived, sizeToReceive, i;
    FILE *newFile;

    //Define o path do arquivo
    snprintf(filePath, sizeof(filePath), "%ssync_dir_%s/%s", SERVER_SYNC_DIR_PATH, userId, basename(file));
    //Cria o arquivo
    if(!(newFile = fopen(filePath, "w"))){
        fprintf(stderr, "Socket %d - Error creating file \'%s\'\n", socket, filePath);
        return;
    }
    //Recebe o tamanho do arquivo
    bzero(buffer, sizeof(buffer));
    if(read(socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "Socket %d - Error receiving file size\n", socket);
        return;
    }
    fileSize = atoi(buffer);
    if(!sendInteger(socket, CP_CLIENT_SEND_FILE_SIZE_ACK)){
        fprintf(stderr, "Socket %d - Error sending file size ack\n", socket);
        return;
    }
    //Recebe o arquivo
    sizeReceived = 0;
    iterations = (fileSize%CP_MAX_MSG_SIZE) > 0 ? fileSize/CP_MAX_MSG_SIZE + 1 : fileSize/CP_MAX_MSG_SIZE;
    for(i = 0; i < iterations; i++){

        fprintf(stderr, "Socket %d - Receiving file, iteration %d of %d\n", socket, i+1, iterations);
        sizeToReceive = (fileSize - sizeReceived) > CP_MAX_MSG_SIZE ? CP_MAX_MSG_SIZE : (fileSize - sizeReceived);
        bzero(buffer, sizeof(buffer));
        if(read(socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "Socket %d - Error receiving part of file %d\n", socket, i+1);
        }
        fwrite((void*) buffer, sizeToReceive, 1, newFile);
        if(!sendInteger(socket, CP_FILE_PART_RECEIVED)){
            fprintf(stderr, "Socket %d - Error sending ack for part %d of %d\n", socket, i+1, iterations);
        }
        sizeReceived += sizeToReceive;
    }
    fclose(newFile);
    if(!receiveExpectedInt(socket, CP_SEND_FILE_COMPLETE)){
        fprintf(stderr, "Socket %d - Error receiving CP_SEND_FILE_COMPLETE\n", socket);
        return;
    }
    if(!sendInteger(socket, CP_SEND_FILE_COMPLETE_ACK)){
        fprintf(stderr, "Socket %d - Error sending CP_SEND_FILE_COMPLETE_ACK\n", socket);
        return;
    }
    bzero(buffer, sizeof(buffer));
    if(read(socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "Socket %d - Error receiving file's M time\n", socket);
    }
    if(sizeReceived == fileSize){
        fprintf(stderr, "Socket %d - File received successfully\n", socket);
    }
    //Preenche as estruturas internas do servidor
    if(!assignNewFile(file, buffer, fileSize, userId)){
        fprintf(stderr, "Socket %d - Error assigning new file \'%s\'\n", socket, file);
    }
}

/*Envia um arquivo para o cliente*/
void DropboxServer::send_file(char* file){
}
//--------------------------------------------------FUNÇÕES EXTRAS

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
    int socket;
    bool isRunning = true;
    char receiveBuffer[CP_MAX_MSG_SIZE], userId[MAXNAME];

    DropboxServer *server = arg.instance;
    socket = *arg.socket;

    fprintf(stderr, "DropboxServer - Starting thread with comunication socket = %d\n", socket);

    //Recebe o userId do cliente
    bzero(receiveBuffer, sizeof(receiveBuffer));
    if(read(socket, receiveBuffer, sizeof(receiveBuffer)) < 0){
        fprintf(stderr, "Socket %d - Error receiving userId\n", socket);
        server->closeConnection(socket);
        free(args);
        return NULL;
    }
    //Valida o userId recebido
    if(server->logInClient(socket, receiveBuffer)){
		//Logou com sucesso
		fprintf(stderr, "Socket %d - User \"%s\" just logged in\n", socket, receiveBuffer);
		sendInteger(socket, CP_LOGIN_SUCCESSFUL);
        strncpy(userId, receiveBuffer, sizeof(userId));
    }
    else{
		//Falha no login
		fprintf(stderr, "Socket %d - Error logging user %s in\n", socket, receiveBuffer);
        sendInteger(socket, CP_LOGIN_FAILED);
        server->closeConnection(socket);
        free(args);
        return NULL;
	}
    //Espera pela mensagem informando se o cliente encontrou o sync_dir
    fprintf(stderr, "Socket %d - Waiting for sync dir message\n", socket);
    bzero(receiveBuffer, sizeof(receiveBuffer));
    if(read(socket, receiveBuffer, sizeof(receiveBuffer)) < 0){
        fprintf(stderr, "Socket %d - Error receiving sync_dir information\n", socket);
        server->logOutClient(socket, userId);
        server->closeConnection(socket);
        free(args);
        return NULL;
    }
    if(atoi(receiveBuffer) == CP_SYNC_DIR_FOUND){
    }
    else if(atoi(receiveBuffer) == CP_SYNC_DIR_NOT_FOUND){
    }
    else{
        fprintf(stderr, "Socket %d - Error didn't receive sync dir message\n", socket);
        server->logOutClient(socket, userId);
        server->closeConnection(socket);
        free(args);
        return NULL;
    }
    server->sync_server(socket, userId);
    //Fica no aguardo de mensagens do cliente
    while(isRunning){
        //Recebe comando do cliente
        bzero(receiveBuffer, sizeof(receiveBuffer));
        if(read(socket, receiveBuffer, sizeof(receiveBuffer)) < 0){
            fprintf(stderr, "Socket %d - Error receiving comand from client\n", socket);
        }
        else{
            switch(atoi(receiveBuffer)){
                case CP_CLIENT_END_CONNECTION:
                    fprintf(stderr, "Socket %d - CP_CLIENT_END_CONNECTION received\n", socket);
                    server->closeConnection(socket);
                    break;
                case CP_CLIENT_SEND_FILE:
                    fprintf(stderr, "Socket %d - CP_CLIENT_SEND_FILE received\n", socket);
                    if(!sendInteger(socket, CP_CLIENT_SEND_FILE_ACK)){
                        fprintf(stderr, "Socket %d - Error sending CP_CLIENT_SEND_FILE_ACK\n", socket);
                        continue;
                    }
                    //Recebe o nome do arquivo
                    bzero(receiveBuffer, sizeof(receiveBuffer));
                    if(read(socket, receiveBuffer, sizeof(receiveBuffer)) < 0){
                        fprintf(stderr, "Socket %d - Error receiving file name\n", socket);
                    }
                    else{
                        server->receive_file(socket, userId, receiveBuffer);
                    }
                    break;
                default:
                    fprintf(stderr, "Socket %d - Unrecognized message %d\n", socket, atoi(receiveBuffer));
                    break;
            }
        }
    }
    free(args);
	return NULL;
}

/*Atribui um novo arquivo a um usuário*/
bool DropboxServer::assignNewFile(char* fileName, char* fileMTime, int fileSize, char* userId){

    uint i, j;
    bool done = false;

    //Encontra a estrutura CLIENT corespondente a userId
    for (i = 0; i < _clients.size(); i++){
        if(strcmp(userId, _clients.at(i).userId) == 0){
            //Encontrou o cliente correspontente
            j = 0;
            while(!done && j < MAXFILES){
                //Encontra o primeiro espaço vazio em file_info[]
                if(strlen(_clients.at(i).file_info[j].name) == 0){
                    //Encontrou um espaço vazio
                    strncpy(_clients.at(i).file_info[j].name, basename(fileName), sizeof(_clients.at(i).file_info[j].name));
                    strncpy(_clients.at(i).file_info[j].extension, getFileExtension(basename(fileName)),
                            sizeof(_clients.at(i).file_info[j].extension));
                    strncpy(_clients.at(i).file_info[j].last_modified, fileMTime,
                            sizeof(_clients.at(i).file_info[j].last_modified));
                    _clients.at(i).file_info[j].size = fileSize;
                    done = true;
                    printClient(_clients.at(i), true);
                    return true;
                }
                j++;
            }
        }
    }
    return false;
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
        for(i = 0; i < MAXFILES; i++){
            bzero(newClient.file_info[i].name, sizeof(newClient.file_info[i].name));
        }
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
    //TODO: Deslogar o usuário
	close(socket);
}

int DropboxServer::getSocket(){ return _serverSocket; }
