#include <string.h>
#include <stdlib.h>

#include "dropboxClient.h"


int main(int argc, char** argv){

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

    printf("DropBox - Sistemas Operacionais 2 - Etapa I\n");
    printf("Integrantes do grupo: André D. Carneiro, Lucas Sievert e Felipe Fuhr\n\n");

    while(isRunning){
        switch (readComand(comand)){
            case COM_UPLOAD:
                fprintf(stderr, "COM_UPLOAD\n");
                break;
            case COM_DOWNLOAD:
                fprintf(stderr, "COM_DOWNLOAD\n");
                break;
            case COM_LIST_SERVER:
                fprintf(stderr, "COM_LIST_SERVER\n");
                break;
            case COM_LIST_CLIENT:
                fprintf(stderr, "COM_LIST_CLIENT\n");
                break;
            case COM_GET_SYNC_DIR:
                fprintf(stderr, "COM_GET_SYNC_DIR\n");
                break;
            case COM_EXIT:
                isRunning = 0;
                break;
        }
    }
}

/*
*   Faz o loop que lê comandos do usuário e retorna o comando lido por meio de comandBuffer
*/
int readComand(char* comandBuffer){

    char comand[MAXCOMANDSIZE];

    while(1){

        printf("> ");
        scanf("%s", comand);

        if(strncmp(comand, "upload", 6) == 0){
            strcpy(comandBuffer, comand);
            return COM_UPLOAD;
        }
        else if(strncmp(comand, "download", 8) == 0){
            strcpy(comandBuffer, comand);
            return COM_DOWNLOAD;
        }
        else if(strncmp(comand, "list_server", 11) == 0){
            strcpy(comandBuffer, comand);
            return COM_LIST_SERVER;
        }
        else if(strncmp(comand, "list_client", 11) == 0){
            strcpy(comandBuffer, comand);
            return COM_LIST_CLIENT;
        }
        else if(strncmp(comand, "get_sync_dir", 12) == 0){
            strcpy(comandBuffer, comand);
            return COM_GET_SYNC_DIR;
        }
        else if(strncmp(comand, "exit", 6) == 0){
            return COM_EXIT;
        }
        else{
            printf("Comando não reconhecido\n");
        }
    }
}
