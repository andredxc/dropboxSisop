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
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include "dropboxClient.h"
#include "../Util/dropboxUtil.h"

/*Constructor*/
DropboxClient::DropboxClient(){}

//------------------------------------------------FUNÇÕES DEFINIDAS NA ESPECIFICAÇÃO

/*Estabelece a conexão*/
int DropboxClient::connect_server(char* host, int port){

    struct hostent *server;
    struct sockaddr_in serverAddress;

    server = gethostbyname(host);
    if(server == NULL){
        fprintf(stderr, "DropboxClient - Server \'%s\' doesn't exist\n", host);
        return -1;
    }
    //Inicializando socket
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if(_socket == -1){
        fprintf(stderr, "DropboxClient - Error creating socket\n");
        return -1;
    }
    //Inicializando struct do socket do servidor
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr = *((struct in_addr*) server->h_addr);
    bzero(&(serverAddress.sin_zero), 8);
    //Conectando ao servidor
    if(connect(_socket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr, "DropboxClient - Couldn't connect to server at %s on port %d\n", inet_ntoa(serverAddress.sin_addr), ntohs(serverAddress.sin_port));
        close(_socket);
        return -1;
    }

    return _socket;
}

/*Sincroniza os arquivos entre cliente e servidor*/
void DropboxClient::sync_client(){

    DIR *localSyncDir;
    FILE *curFile;
    struct dirent *entry;
    char syncDirPath[200], curFilePath[200];
    long curFileSize;

    snprintf(syncDirPath, sizeof(syncDirPath), "%s%s/", CLIENT_SYNC_DIR_PATH, _userId);
    if((localSyncDir = opendir(syncDirPath)) != NULL){
        while((entry = readdir(localSyncDir)) != NULL){
            //Para cada arquivo no diretório
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
                continue;
            }
            //Para todos os arquivos úteis
            snprintf(curFilePath, sizeof(curFilePath), "%s%s", syncDirPath, entry->d_name);
            if(!(curFile = fopen(curFilePath, "r"))){
                fprintf(stderr, "DropboxClient - Erro abrindo arquivo %s em\n", curFilePath);
                continue;
            }
            //Descobrindo tamanho do arquivo
            fseek(curFile, 0, SEEK_END);
            curFileSize = ftell(curFile);
            fseek(curFile, 0, SEEK_SET);
            fprintf(stderr, "NOME DO ARQUIVO: %s\n", curFilePath);
            fprintf(stderr, "TAMANHO DO ARQUVO: %ld\n", curFileSize);
        }
        fprintf(stderr, "FIM DOS ARQUIVOS\n");
    }
    closedir(localSyncDir);
}

/*Envia um arquivo*/
void DropboxClient::send_file(char* filePath){

    FILE *file;
    int fileSize, i, iterations, sizeSent, sizeToSend;
    char buffer[CP_MAX_MSG_SIZE];

    //Verifica o tamanho do nome do arquivo
    if(strlen(basename(filePath)) > CP_MAX_MSG_SIZE-1){
        fprintf(stderr, "DropboxClient - Name size limit exceded\n");
        return;
    }
    //Abre o arquivo para leitura
    if(!(file = fopen(filePath, "r"))){
        //Erro na abertura
        fprintf(stderr, "DropboxClient - Error opening file \'%s\'\n", filePath);
        return;
    }
    //Descobre o tamanho do arquivo
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    //Envia pedido de envio de arquivo
    sendInteger(_socket, CP_CLIENT_SEND_FILE);
    if(!receiveExpectedInt(_socket, CP_CLIENT_SEND_FILE_ACK)){
        fprintf(stderr, "DropboxClient - Error receiving confirmation from server\n");
        fclose(file);
        return;
    }
    //Envia o nome do arquivo
    bzero(buffer, sizeof(buffer));
    strncpy(buffer, basename(filePath), sizeof(buffer));
    if(write(_socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error sending filename \'%s\'\n", buffer);
        fclose(file);
        return;
    }
    //Envia o tamanho do arquivo
    if(!sendInteger(_socket, fileSize)){
        fprintf(stderr, "DropboxClient - Error sending file size\n");
        fclose(file);
        return;
    }
    if(!receiveExpectedInt(_socket, CP_CLIENT_SEND_FILE_SIZE_ACK)){
        fprintf(stderr, "DropboxClient - Error receiving size confirmation from server\n");
        fclose(file);
        return;
    }
    //Começa o envio do arquivo
    iterations = (fileSize%CP_MAX_MSG_SIZE) > 0 ? fileSize/CP_MAX_MSG_SIZE + 1 : fileSize/CP_MAX_MSG_SIZE;
    sizeSent = 0;
    for(i = 0; i < iterations; i++){

        sizeToSend = (fileSize - sizeSent) > CP_MAX_MSG_SIZE ? CP_MAX_MSG_SIZE : (fileSize - sizeSent);
        bzero(buffer, sizeof(buffer));
        fread((void*) buffer, sizeToSend, 1, file);
        if(write(_socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "DropboxClient - Error sending part of file %d\n", i);
        }
        sizeSent += sizeToSend;
    }
    fclose(file);
    //Envia mesagem informando fim do arquivo
    if(!sendInteger(_socket, CP_SEND_FILE_COMPLETE)){
        fprintf(stderr, "DropboxClient - Error sending file send complietion message\n");
        return;
    }
    //Recebe ack
    if(!receiveExpectedInt(_socket, CP_SEND_FILE_COMPLETE_ACK)){
        fprintf(stderr, "DropboxClient - Error receiving ack for file completion\n");
        return;
    }
    if(sizeSent == fileSize){
        fprintf(stderr, "DropboxClient - File sent successfully\n");
    }
}

/**/
void DropboxClient::get_file(char* file){}

/**/
void DropboxClient::delete_file(char* file){}

/*Termina a conexão*/
void DropboxClient::close_connection(){

    if(!sendInteger(_socket, CP_CLIENT_END_CONNECTION)){
        fprintf(stderr, "DropboxClient - Error sending close connection message\n");
    }
    else{
        fprintf(stderr, "DropboxClient - Closing connection with server\n");
    }
}
//------------------------------------------------OUTRAS FUNÇÕES

/*Faz o loop que lê comandos do usuário e retorna os argumentos por meio de comandBuffer*/
int DropboxClient::readComand(char* comandBuffer, int bufferSize){

    char comand[MAXCOMANDSIZE];

    while(1){

        printf("> ");
        fgets(comand, MAXCOMANDSIZE, stdin);
        if(comand[strlen(comand)-1] == '\n'){
            //Elimina o \n lido ao fim
            comand[strlen(comand)-1] = '\0';
        }

        if(strncmp(comand, "upload", 6) == 0){
            if(comand[6] == ' '){
                //Copia os argumentos sem o espaço do inicio
                strncpy(comandBuffer, comand, bufferSize);
                return COM_UPLOAD;
            }
            else{
                //Não tem espaço entre comando e argumento
                fprintf(stderr, "DropboxClient - Usage: upload <path/filename>\n");
            }
        }
        else if(strncmp(comand, "download", 8) == 0){
            if(comand[8] == ' '){
                //Copia os argumentos sem o espaço do inicio
                strncpy(comandBuffer, comand, bufferSize);
                return COM_DOWNLOAD;
            }
            else{
                //Não tem espaço entre comando e argumento
                fprintf(stderr, "DropboxClient - Usage: download <filename>\n");
            }
        }
        else if(strcmp(comand, "list_server") == 0){
            strncpy(comandBuffer, comand, bufferSize);
            return COM_LIST_SERVER;
        }
        else if(strcmp(comand, "list_client") == 0){
            strncpy(comandBuffer, comand, bufferSize);
            return COM_LIST_CLIENT;
        }
        else if(strcmp(comand, "get_sync_dir") == 0){
            strncpy(comandBuffer, comand, bufferSize);
            return COM_GET_SYNC_DIR;
        }
        else if(strcmp(comand, "exit") == 0){
            return COM_EXIT;
        }
        else{
            printf("DropboxClient - Comando não reconhecido\n");
        }
    }
}

/*Envia o comando ao servidor*/
bool DropboxClient::sendComand(char* comand, int length){

	char buffer[CP_MAX_MSG_SIZE];

	bzero(buffer, sizeof(buffer));
	strncpy(buffer, comand, length);

    if(write(_socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error sending comand \'%s\' to server\n", comand);
        return false;
    }
    else{
        fprintf(stderr, "DropboxClient - Comand sent to server on socket %d!\n", _socket);
        return true;
    }
}

/*Verifica se hove alterações dentro do diretório em path*/
void* DropboxClient::fileWatcher(void* path){

    int fileDesc, watchDesc, length, isRunning = 1;
    struct inotify_event *event;
    char *eventPtr, *dirPath, buffer[4098];

    dirPath = (char*) path;
    fprintf(stderr, "DropboxClient - dirPath = %s\n", dirPath);

    fileDesc = inotify_init();

    if(fileDesc < 0){
        fprintf(stderr, "DropboxClient - Error starting inotify\n");
        return NULL;
    }

    watchDesc = inotify_add_watch(fileDesc, ".", IN_MODIFY | IN_CREATE | IN_DELETE);

    if(watchDesc < 0){
        fprintf(stderr, "DropboxClient - Error adding watch to \'%s\'\n", dirPath);
        isRunning = 0;
    }

    while(isRunning){
        //Verifica se arquivos foram alterados
        length = read(fileDesc, buffer, sizeof(buffer));

        if(length < 0){
            //Erro na leitura
            fprintf(stderr, "DropboxClient - Error watching directory \'%s\'\n", dirPath);
            isRunning = 0;
        }
        else{
            //Detectou mudança nos arquivos
            for(eventPtr = buffer; eventPtr < buffer + length; eventPtr += sizeof(struct inotify_event) + event->len){
                //Itera sobre os eventos lidos
                event = (struct inotify_event*) eventPtr;

                if(event->mask & IN_MODIFY){
                    fprintf(stderr, "DropboxClient - File \'%s\' was modified\n", event->name);
                }
                else if(event->mask & IN_CREATE){
                    fprintf(stderr, "DropboxClient - File \'%s\' was created\n", event->name);
                }
                else if(event->mask & IN_DELETE){
                    fprintf(stderr, "DropboxClient - File \'%s\' was deleted\n", event->name);
                }
            }
        }
    }

    inotify_rm_watch(fileDesc, watchDesc);
    close(fileDesc);

    return NULL;
}

/*Executa as ações referentes ao comand get_sync_dir*/
void DropboxClient::getSyncDirComand(){

    char syncDirPath[200];
    int errorCode;

    snprintf(syncDirPath, sizeof(syncDirPath), "%ssync_dir_%s", CLIENT_SYNC_DIR_PATH, _userId);
    //Verifica se sync_dir já existe no cliente
    if(access(syncDirPath, F_OK) == -1){
        //Diretório não encontrado
        fprintf(stderr, "DropboxClient - Directory \'%s\' doesn't exist\n", syncDirPath);
        sendInteger(_socket, CP_SYNC_DIR_NOT_FOUND);
        if((errorCode = mkdir(syncDirPath, S_IRWXU)) != 0){
            fprintf(stderr, "DropboxClient - Error %d creating directory %s\n", errorCode, syncDirPath);
            //TODO: Terminar conexão ou algo parecido
            close_connection();
            return;
        }
    }
    else{
        //Diretório encontrado
        sendInteger(_socket, CP_SYNC_DIR_FOUND);
    }
    // sync_client();
}

/*Envia o userId para o servidor e aguarda confirmação de log in*/
bool DropboxClient::sendUserId(char* userId){

	char buffer[CP_MAX_MSG_SIZE];

	strncpy(buffer, userId, sizeof(buffer));
	strncpy(_userId, userId, sizeof(_userId));
	//Envia a identificação para o servidor
	if(write(_socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error sending userId\n");
        return false;
    }
    //Aguar mensagem de login
    if(read(_socket, buffer, sizeof(buffer)) < 0){
		fprintf(stderr, "DropboxClient - Error receiving sign in message\n");
		return false;
	}
    else{
		if(atoi(buffer) == CP_LOGIN_SUCCESSFUL){
			return true;
		}
		else if(atoi(buffer) == CP_LOGIN_FAILED){
			return false;
		}
		else{
			fprintf(stderr, "DropboxClient - Internal error during sign in\n");
			return false;
		}
	}
	return false;
}

int DropboxClient::getSocket(){ return _socket; }
void DropboxClient::setUserId(char* userId){ strncpy(_userId, userId, sizeof(_userId)); }
