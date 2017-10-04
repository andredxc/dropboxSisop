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

    fprintf(stderr, "DropBox - Sistemas Operacionais 2 - Etapa I\n");
    fprintf(stderr, "Integrantes do grupo: André D. Carneiro, Lucas Sievert e Felipe Fuhr\n\n");

    while(isRunning){
        switch (readComand(comand)){
            case COM_UPLOAD:
                fprintf(stderr, "COM_UPLOAD\n");
                fprintf(stderr, "arguments: \'%s\'\n", comand);
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

/*
*   Faz o loop que lê comandos do usuário e retorna o comando lido por meio de comandBuffer
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
