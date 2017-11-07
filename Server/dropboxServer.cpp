#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
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

    char dirPath[512], buffer[CP_MAX_MSG_SIZE];
    int i, errorCode, numberOfFiles, userIndex;

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
    numberOfFiles = countUserFiles(userId);
    userIndex = findUserIndex(userId);
    fprintf(stderr, "Socket %d - Number of files: %d, userIndex: %d\n", socket, numberOfFiles, userIndex);
    if(!sendInteger(socket, numberOfFiles)){
        fprintf(stderr, "Socket %d - Error sending number of files\n", socket);
        return;
    }

    //Manda as informações de todos os arquivos
    for(i = 0; i < numberOfFiles; i++){

        //Envia o nome do arquivo
        bzero(buffer, sizeof(buffer));
        strncpy(buffer, _clients.at(userIndex).file_info[i].name, sizeof(buffer));
        if(write(socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "Socket %d - Error sending file %d name (%s)\n", socket, i+1, buffer);
        }

        //Envia o M time do arquivo
        bzero(buffer, sizeof(buffer));
        strncpy(buffer, _clients.at(userIndex).file_info[i].last_modified, sizeof(buffer));
        if(write(socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "Socket %d - Error sending file %d M time (%s)\n", socket, i+1, buffer);
        }

        //Espera resposta indicando o que fazer
        bzero(buffer, sizeof(buffer));
        if(read(socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "Socket %d - Error receiving answer from client\n", socket);
            //TODO
        }

        switch (atoi(buffer)) {
            case CP_SYNC_FILE_OK:
                //Arquivo está sincronizado
                fprintf(stderr, "Socket %d - Received CP_SYNC_FILE_OK\n", socket);
                break;
            case CP_CLIENT_SEND_FILE:
                //Cliente vai fazer upload do arquivo
                fprintf(stderr, "Socket %d - Received CP_CLIENT_SEND_FILE\n", socket);
                if(!sendInteger(socket, CP_CLIENT_SEND_FILE_ACK)){
                    fprintf(stderr, "Socket %d - Error sending CP_CLIENT_SEND_FILE_ACK\n", socket);
                    continue;
                }
                //Espera pelo nome do arquivo
                bzero(buffer, sizeof(buffer));
                if(read(socket, buffer, sizeof(buffer)) < 0){
                    fprintf(stderr, "Socket %d - Error receiving file name from client\n", socket);
                }
                else{
                    receive_file(socket, userId, buffer);
                }
                break;
            case CP_SYNC_DOWNLOAD_FILE:
                //Arquivo deve ser baixado do servidor
                fprintf(stderr, "Socket %d - Received CP_SYNC_DOWNLOAD_FILE\n", socket);
                break;
            case CP_SYNC_FILE_NOT_FOUND:
                //Arquivo não se encontra no client
                fprintf(stderr, "Socket %d - Received CP_SYNC_FILE_NOT_FOUND\n", socket);
                break;
            default: fprintf(stderr, "Socket %d - Unrecognized answer at sync_server\n", socket); break;
        }
    }
}

/*Recebe um arquivo do cliente*/
void DropboxServer::receive_file(int socket, char* userId, char* file){

    char buffer[CP_MAX_MSG_SIZE], filePath[512];
    int fileSize, iterations, sizeReceived, sizeToReceive, i;
    FILE *newFile;

    // Define o path do arquivo
    snprintf(filePath, sizeof(filePath), "%ssync_dir_%s/%s", SERVER_SYNC_DIR_PATH, userId, basename(file));

    // Cria o arquivo
    if(!(newFile = fopen(filePath, "w"))){
        fprintf(stderr, "Socket %d - Error creating file \'%s\'\n", socket, filePath);
        return;
    }

    // Recebe o tamanho do arquivo
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

    // Recebe o arquivo
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
    //Recebe confirmação de término do envio do arquivo
    if(!receiveExpectedInt(socket, CP_SEND_FILE_COMPLETE)){
        fprintf(stderr, "Socket %d - Error receiving CP_SEND_FILE_COMPLETE\n", socket);
        return;
    }
    if(!sendInteger(socket, CP_SEND_FILE_COMPLETE_ACK)){
        fprintf(stderr, "Socket %d - Error sending CP_SEND_FILE_COMPLETE_ACK\n", socket);
        return;
    }

    //Recebe o M time do arquivo no cliente
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
    fprintf(stderr, "Socket %d - File \'%s\' received successfully\n", socket, basename(file));
}

/*Envia um arquivo para o cliente*/
void DropboxServer::send_file(int socket, char* userId, char* filePath){

    char fserverPath[512];
    int fileSize, iterations, sizeSent, sizeToSend;
    char buffer[CP_MAX_MSG_SIZE], mTime[CP_MAX_MSG_SIZE];
    FILE *file;

    snprintf(fserverPath, sizeof(fserverPath), "%ssync_dir_%s/%s", SERVER_SYNC_DIR_PATH, userId, basename(filePath));

    fprintf(stderr, "Downloading file: %s \n", fserverPath);

    // Abre o arquivo para leitura
    if(!(file = fopen(fserverPath, "r"))){
        // Erro na abertura
        fprintf(stderr, "DropboxServer - Error opening file \'%s\'\n", filePath);
    }
    // Envia confirmação da existência do arquivo
    if(!sendInteger(socket, file != NULL)){
        fprintf(stderr, "DropboxServer - Error confirming file existance\n");
        return;
    }
    if(file == NULL) return;

    // Descobre o tamanho do arquivo
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    // Envia o tamanho do arquivo
    if(!sendInteger(socket, fileSize)){
        fprintf(stderr, "DropboxServer - Error sending file size\n");
        fclose(file);
        return;
    }
    if(!receiveExpectedInt(socket, CP_CLIENT_GET_FILE_SIZE_ACK)){
        fprintf(stderr, "DropboxServer - Error receiving size confirmation from client\n");
        fclose(file);
        return;
    }

    // Começa o envio do arquivo
    iterations = (fileSize%CP_MAX_MSG_SIZE) > 0 ? fileSize/CP_MAX_MSG_SIZE + 1 : fileSize/CP_MAX_MSG_SIZE;
    sizeSent = 0;
    for(int i = 0; i < iterations; i++){

        fprintf(stderr, "DropboxServer - Sending file, iteration %d of %d\n", i+1, iterations);
        sizeToSend = (fileSize - sizeSent) > CP_MAX_MSG_SIZE ? CP_MAX_MSG_SIZE : (fileSize - sizeSent);
        bzero(buffer, sizeof(buffer));
        fread((void*) buffer, sizeToSend, 1, file);
        if(write(socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "DropboxServer - Error sending part %d of file\n", i);
        }
        if(!receiveExpectedInt(socket, CP_FILE_PART_RECEIVED)){
            fprintf(stderr, "DropboxServer - Error receving ack for part %d of %d\n", i+1, iterations);
        }
        sizeSent += sizeToSend;
    }
    fclose(file);

    // Envia mesagem informando fim do arquivo
    if(!sendInteger(socket, CP_SEND_FILE_COMPLETE)){
        fprintf(stderr, "DropboxServer - Error sending file send completion message\n");
        return;
    }
    // Recebe ack
    if(!receiveExpectedInt(socket, CP_SEND_FILE_COMPLETE_ACK)){
        fprintf(stderr, "DropboxServer - Error receiving ack for file completion\n");
        return;
    }

    // Exibe mensagem ao servidor
    if(sizeSent == fileSize){
        fprintf(stderr, "DropboxServer - File sent \'%s\' successfully\n", basename(filePath));
    }

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

    // Recebe o userId do cliente
    bzero(receiveBuffer, sizeof(receiveBuffer));
    if(read(socket, receiveBuffer, sizeof(receiveBuffer)) < 0){
        fprintf(stderr, "Socket %d - Error receiving userId\n", socket);
        server->closeConnection(socket);
        free(args);
        return NULL;
    }
    // Valida o userId recebido
    if(server->logInClient(socket, receiveBuffer)){
		// Logou com sucesso
		fprintf(stderr, "Socket %d - User \"%s\" just logged in\n", socket, receiveBuffer);
		sendInteger(socket, CP_LOGIN_SUCCESSFUL);
        strncpy(userId, receiveBuffer, sizeof(userId));
    }
    else{
		// Falha no login
		fprintf(stderr, "Socket %d - Error logging user %s in\n", socket, receiveBuffer);
        sendInteger(socket, CP_LOGIN_FAILED);
        server->closeConnection(socket);
        free(args);
        return NULL;
    }
    // Fica no aguardo de mensagens do cliente
    while(isRunning){
        // Recebe comando do cliente
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
                case CP_CLIENT_GET_FILE:
                    fprintf(stderr, "Socket %d - CP_CLIENT_GET_FILE received\n", socket);
                    if(!sendInteger(socket, CP_CLIENT_GET_FILE_ACK)){
                        fprintf(stderr, "Socket %d - Error sending CP_CLIENT_GET_FILE_ACK\n", socket);
                        continue;
                    }
                    //Recebe o nome do arquivo
                    bzero(receiveBuffer, sizeof(receiveBuffer));
                    if(read(socket, receiveBuffer, sizeof(receiveBuffer)) < 0){
                        fprintf(stderr, "Socket %d - Error receiving file name\n", socket);
                    }
                    else{
                        server->send_file(socket, userId, receiveBuffer);
                    }
                    break;
                case CP_SYNC_CLIENT:
                    server->sync_server(socket, userId);
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

    uint j;
    bool done = false;
    int userIndex, fileIndex;

    userIndex = findUserIndex(userId);
    fileIndex = findUserFile(userId, fileName);

    if(userIndex < 0){
        return false;
    }

    if(fileIndex < 0){
        //Arquivo novo
        j = 0;
        while(!done && j < MAXFILES){
            //Encontra o primeiro espaço vazio em file_info[]
            if(strlen(_clients.at(userIndex).file_info[j].name) == 0){
                //Encontrou um espaço vazio
                fileIndex = j;
                done = true;
            }
            j++;
        }
    }

    if(fileIndex >= 0){
        //Encontrou algum espaço vazio
        strncpy(_clients.at(userIndex).file_info[fileIndex].name, basename(fileName), sizeof(_clients.at(userIndex).file_info[fileIndex].name));
        strncpy(_clients.at(userIndex).file_info[fileIndex].extension, getFileExtension(basename(fileName)),
        sizeof(_clients.at(userIndex).file_info[fileIndex].extension));
        strncpy(_clients.at(userIndex).file_info[fileIndex].last_modified, fileMTime,
        sizeof(_clients.at(userIndex).file_info[fileIndex].last_modified));
        _clients.at(userIndex).file_info[fileIndex].size = fileSize;
        printClient(_clients.at(userIndex), true);
        return true;
    }
    else{
        //Não encontrou nenhum espaço vazio
        fprintf(stderr, "DropboxServer - Couldn't find a space for file \'%s\'\n", fileName);
        return false;
    }
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
            printf("Socket %d - Internal error, one position should be empty\n", socket);
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

/* Conta o número de arquivos de um determinado usuário */
int DropboxServer::countUserFiles(char* userId){

    int i, userIndex, fileCount = 0;

    userIndex = findUserIndex(userId);
    if(userIndex >= 0){
        //Encontrou o usuário no vetor
        for(i = 0; i < MAXFILES; i++){
            //Conta o número de arquivos
            if(strlen(_clients.at(userIndex).file_info[i].name) > 0){
                fileCount++;
            }
        }
        return fileCount;
    }
    //Não encontrou o usuário no vetor
    return -1;
}

/* Encontra o índice correspondente a userId no vetor de clientes */
int DropboxServer::findUserIndex(const char* userId){

    uint i;

    for(i = 0; i < _clients.size(); i++){
        if(strcmp(_clients.at(i).userId, userId) == 0){
            //Encontrou o userId
            return i;
        }
    }
    //Não encontrou o userId
    return -1;
}

/* Recupera as estruturas internas do servidor com base nos diretórios locais */
void DropboxServer::recoverData(){

    DIR *serverSyncDir, *curSyncDir;
    struct dirent *entry, *curEntry;
    char curSyncDirPath[512], curFilePath[512];
    CLIENT clientBuffer;
    int i, fileCounter;

    if((serverSyncDir = opendir(SERVER_SYNC_DIR_PATH)) != NULL){
        //Abre o diretório em que estão todos os sync_dir_
            while((entry = readdir(serverSyncDir)) != NULL){
                // Para cada arquivo no diretório
                if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
                    //Ignora os arquivos . e ..
                    continue;
                }

                // Para todos os arquivos úteis
                snprintf(curSyncDirPath, sizeof(curSyncDirPath), "%s%s", SERVER_SYNC_DIR_PATH, entry->d_name);
                if((curSyncDir = opendir(curSyncDirPath)) != NULL){
                    //Entrou em algum sync_dir
                    if(sscanf(entry->d_name, "sync_dir_%s", clientBuffer.userId) > 0){
                        //O diretório encontrado é um sync_dir
                        //Inicializa as variáveis
                        clientBuffer.devices[0] = -1;
                        clientBuffer.devices[1] = -1;
                        clientBuffer.logged_in = 0;
                        for(i = 0; i < MAXFILES; i++){
                            bzero(clientBuffer.file_info[i].name, sizeof(clientBuffer.file_info[i].name));
                        }

                        //Recolhe dados sobre os arquivos
                        fileCounter = 0;
                        while((curEntry = readdir(curSyncDir)) != NULL){
                            if(strcmp(curEntry->d_name, ".") == 0 || strcmp(curEntry->d_name, "..") == 0){
                                //Ignora os arquivos . e ..
                                continue;
                            }

                            //Salva os dados dos arquivos úteis
                            snprintf(curFilePath, sizeof(curFilePath), "%s/%s", curSyncDirPath, curEntry->d_name);
                            getFileInfo(curFilePath, &clientBuffer.file_info[fileCounter]);
                            fileCounter++;
                            // strncpy(clientBuffer.file_info[fileCounter].name, curEntry->d_name, sizeof(clientBuffer.file_info[fileCounter].name));
                            // strncpy(clientBuffer.file_info[fileCounter].extension, getFileExtension(curEntry->d_name), sizeof(clientBuffer.file_info[fileCounter].extension));
                            // getMTime(curFilePath, clientBuffer.file_info[fileCounter].last_modified, sizeof(clientBuffer.file_info[fileCounter].last_modified));
                            // getFileSize(curFilePath, &clientBuffer.file_info[fileCounter].size);
                        }

                        //Print para testes
                        printClient(clientBuffer, true);
                        _clients.push_back(clientBuffer);
                    }
                    closedir(curSyncDir);
                }
            }
            closedir(serverSyncDir);
        }
}

/* Retorna o índice para o arquivo em file_info se existente ou -1 caso contrário */
int DropboxServer::findUserFile(char* userId, char* fileName){

    int userIndex;
    uint i;

    userIndex = findUserIndex(userId);

    if(userIndex < 0){
        return -1;
    }

    for(i = 0; i < MAXFILES; i++){
        //Para cada arquivo do usuário
        if(strcmp(_clients.at(userIndex).file_info[i].name, fileName) == 0){
            return i;
        }
    }
    //Não encontrou o arquivo
    return -1;
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

    //Inicializa socket (caso não consiga, interrompe execução e retorna erro)
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(_serverSocket == -1){
        printf("DropboxServer - Error initializing socket\n");
        return -1;
    }

    //Inicializa struct do socket
    serverAddress.sin_family = AF_INET; // communication domain do socket criado
    serverAddress.sin_port = htons(SERVER_PORT); // porta do socket
    serverAddress.sin_addr.s_addr = INADDR_ANY; // container genérico
    bzero(&(serverAddress.sin_zero), 8); // completa os 16 bits de serverAddress com 8 0's (trabalha-se com 16 bits, mas
                                         // sin_family tem 2 bits, sin_port tem 2 e sin_addr tem 4)
    //Faz o bind (atribui identidade ao socket)
    if(bind(_serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){
        printf("DropboxServer - Error binding server\n");
        return -1;
    }

    //Recupera as estruturas do servidor
    recoverData();

    return _serverSocket;
}

/*Fecha a conexão com um cliente*/
void DropboxServer::closeConnection(int socket){

	printf("DropboxServer - Closing connection with socket %d\n", socket);
    //TODO: Deslogar o usuário
	close(socket);
    pthread_exit(NULL);
}

int DropboxServer::getSocket(){ return _serverSocket; }
