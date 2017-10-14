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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dropboxClient.h"
#include "../Util/dropboxUtil.h"

/*
*   Constructor
*/
DropboxClient::DropboxClient(){}

//------------------------------------------------FUNÇÕES DEFINIDAS NA ESPECIFICAÇÃO

/*
*   Estabelece conexão
*/
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

/***/
void DropboxClient::sync_client(){}

/*
*   Envia arquivo
*/
void DropboxClient::send_file(char* file){

    if(access(file, F_OK) == -1){
        //Arquivo não existe
        fprintf(stderr, "DropboxClient - File \'%s\' doesn't exist\n", file);
        return;
    }
}

/***/
void DropboxClient::get_file(char* file){}

/***/
void DropboxClient::delete_file(char* file){}

/***/
void DropboxClient::close_connection(){}


//------------------------------------------------OUTRAS FUNÇÕES

/*
*   Faz o loop que lê comandos do usuário e retorna os argumentos por meio de comandBuffer
*/
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

/*
*   Envia o comando ao servidor
*/
bool DropboxClient::sendComand(char* comand, int length){

    if(write(_socket, comand, length) < 0){
        fprintf(stderr, "DropboxClient - Error sending comand \'%s\' to server\n", comand);
        return false;
    }
    else{
        fprintf(stderr, "DropboxClient - Comand sent to server on socket %d!\n", _socket);
        return true;
    }
}

/*
*   Verifica se hove alterações dentro do diretório em path
*/
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

/*
*   Executa as ações referentes ao comand get_sync_dir
*/
void DropboxClient::getSyncDirComand(){

    char syncDirPath[200];

    if(write(_socket, _userId, sizeof(_userId)) < 0){
        fprintf(stderr, "DropboxClient - Error sending userId\n");
        return;
    }

    snprintf(syncDirPath, sizeof(syncDirPath), "%s%s", CLIENT_SYNC_DIR_PATH, _userId);
    //Verifica se sync_dir já existe no cliente
    if(access(syncDirPath, F_OK) == -1){
        //Diretório não encontrado
        fprintf(stderr, "DropboxClient - Directory \'%s\' doesn't exist\n", syncDirPath);
        sendInteger(CP_SYNC_DIR_NOT_FOUND);
    }
    else{
        //Diretório encontrado
        sendInteger(CP_SYNC_DIR_FOUND);
        sync_client();
    }
}

/*
*   Envia um int ao servidor
*/
bool DropboxClient::sendInteger(int message){

    char buffer[12];

    snprintf(buffer, sizeof(buffer), "%d", message);
    if(write(_socket, buffer, strlen(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error sending integer %d\n", message);
        return false;
    }
    return true;
}

int DropboxClient::getSocket(){ return _socket; }
void DropboxClient::setUserId(char* userId){ strncpy(_userId, userId, sizeof(_userId)); }
