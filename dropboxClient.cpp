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

//------------------------------------------------FUNÇÕES DEFINIDAS NA ESPECIFICAÇÃO

/*
*   Constructor
*/
DropboxClient::DropboxClient(){

}

/*
*   Envia arquivo
*/
void DropboxClient::send_file(char* file){

    if(access(file, F_OK) == -1){
        //Arquivo não existe
        fprintf(stderr, "File \'%s\' doesn't exist\n", file);
        return;
    }
}

/*
*   Estabelece conexão
*/
int DropboxClient::connect_server(char* host, int port){

    struct hostent *server;
    struct sockaddr_in serverAddress;
    int comSocket;

    server = gethostbyname(host);
    if(server == NULL){
        fprintf(stderr, "Server \'%s\' doesn't exist\n", host);
        return -1;
    }
    //Inicializando socket
    comSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(comSocket == -1){
        fprintf(stderr, "Error creating socket\n");
        return -1;
    }
    //Inicializando struct do socket do servidor
    serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(port);
	serverAddress.sin_addr = *((struct in_addr*) server->h_addr);
	bzero(&(serverAddress.sin_zero), 8);
    //Conectando ao servidor
    if(connect(comSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr, "Couldn't connect to server at %s on port %d\n", inet_ntoa(serverAddress.sin_addr), ntohs(serverAddress.sin_port));
        close(comSocket);
        return -1;
    }

    return comSocket;
}

//------------------------------------------------OUTRAS FUNÇÕES

/*
*   Faz o loop que lê comandos do usuário e retorna os argumentos por meio de comandBuffer
*/
int DropboxClient::readComand(char* comandBuffer){

    char comand[MAXCOMANDSIZE];

    while(1){

        printf("> ");
        fgets(comand, MAXCOMANDSIZE, stdin);
        if(comand[strlen(comand)-1] == '\n'){
            //Elimina o \n lido
            comand[strlen(comand)-1] = '\0';
        }

        if(strncmp(comand, "upload", 6) == 0){
            if(comand[6] == ' '){
                //Copia os argumentos sem o espaço do inicio
                strcpy(comandBuffer, &comand[7]);
                return COM_UPLOAD;
            }
            else{
                //Não tem espaço entre comando e argumento
                fprintf(stderr, "Usage: upload <path/filename>\n");
            }
        }
        else if(strncmp(comand, "download", 8) == 0){
            if(comand[8] == ' '){
                //Copia os argumentos sem o espaço do inicio
                strcpy(comandBuffer, &comand[9]);
                return COM_DOWNLOAD;
            }
            else{
                //Não tem espaço entre comando e argumento
                fprintf(stderr, "Usage: download <filename>\n");
            }
        }
        else if(strcmp(comand, "list_server") == 0){
            strcpy(comandBuffer, comand);
            return COM_LIST_SERVER;
        }
        else if(strcmp(comand, "list_client") == 0){
            strcpy(comandBuffer, comand);
            return COM_LIST_CLIENT;
        }
        else if(strcmp(comand, "get_sync_dir") == 0){
            strcpy(comandBuffer, comand);
            return COM_GET_SYNC_DIR;
        }
        else if(strcmp(comand, "exit") == 0){
            return COM_EXIT;
        }
        else{
            printf("Comando não reconhecido\n");
        }
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
    fprintf(stderr, "dirPath = %s\n", dirPath);

    fileDesc = inotify_init();

    if(fileDesc < 0){
        fprintf(stderr, "Error starting inotify\n");
        return NULL;
    }

    watchDesc = inotify_add_watch(fileDesc, ".", IN_MODIFY | IN_CREATE | IN_DELETE);

    if(watchDesc < 0){
        fprintf(stderr, "Error adding watch to \'%s\'\n", dirPath);
        isRunning = 0;
    }

    while(isRunning){
        //Verifica se arquivos foram alterados
        length = read(fileDesc, buffer, sizeof(buffer));

        if(length < 0){
            //Erro na leitura
            fprintf(stderr, "Error watching directory \'%s\'\n", dirPath);
            isRunning = 0;
        }
        else{
            //Detectou mudança nos arquivos
            for(eventPtr = buffer; eventPtr < buffer + length; eventPtr += sizeof(struct inotify_event) + event->len){
                //Itera sobre os eventos lidos
                event = (struct inotify_event*) eventPtr;

                if(event->mask & IN_MODIFY){
                    fprintf(stderr, "File \'%s\' was modified\n", event->name);
                }
                else if(event->mask & IN_CREATE){
                    fprintf(stderr, "File \'%s\' was created\n", event->name);
                }
                else if(event->mask & IN_DELETE){
                    fprintf(stderr, "File \'%s\' was deleted\n", event->name);
                }
            }
        }
    }

    inotify_rm_watch(fileDesc, watchDesc);
    close(fileDesc);

    return NULL;
}
