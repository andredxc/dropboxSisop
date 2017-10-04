#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <pthread.h>

#include "dropboxClient.h"


int main(int argc, char** argv){

    pthread_t fileWatcherThread;
    char user[MAXNAME];
    char address[10], port[4];
    char comand[MAXCOMANDSIZE];
    int isRunning = 1;

    if(argc < 4){
        printf("Usage: ./dropboxClient <user> <address> <port>\n");
        exit(0);
    }

    strcpy(user, argv[1]);
    strcpy(address, argv[2]);
    strcpy(port, argv[3]);

    fprintf(stderr, "DropBox - Sistemas Operacionais 2 - Etapa I\n");
    fprintf(stderr, "Integrantes do grupo: André D. Carneiro, Lucas Sievert e Felipe Fuhr\n\n");

    //Cria a thread que verifica por alterações nos arquivos
    pthread_create(&fileWatcherThread, NULL, fileWatcher, (void*) ".");

    while(isRunning){
        switch (readComand(comand)){
            case COM_UPLOAD:
                send_file(comand);
                break;
            case COM_DOWNLOAD:
                fprintf(stderr, "COM_DOWNLOAD\n");
                fprintf(stderr, "arguments: \'%s\'\n", comand);
                break;
            case COM_LIST_SERVER:
                fprintf(stderr, "COM_LIST_SERVER\n");
                fprintf(stderr, "arguments: \'%s\'\n", comand);
                break;
            case COM_LIST_CLIENT:
                fprintf(stderr, "COM_LIST_CLIENT\n");
                fprintf(stderr, "arguments: \'%s\'\n", comand);
                break;
            case COM_GET_SYNC_DIR:
                fprintf(stderr, "COM_GET_SYNC_DIR\n");
                fprintf(stderr, "arguments: \'%s\'\n", comand);
                break;
            case COM_EXIT:
                isRunning = 0;
                break;
        }
    }
}


void send_file(char* file){

    if(access(file, F_OK) == -1){
        //Arquivo não existe
        fprintf(stderr, "File \'%s\' doesn't exist\n", file);
        return;
    }
}

/*
*   Faz o loop que lê comandos do usuário e retorna os argumentos por meio de comandBuffer
*/
int readComand(char* comandBuffer){

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


void *fileWatcher(void* path){

    int fileDesc, watchDesc, length, isRunning = 1;
    struct inotify_event *event;
    char *eventPtr, *dirPath, buffer[4098];

    dirPath = path;
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
