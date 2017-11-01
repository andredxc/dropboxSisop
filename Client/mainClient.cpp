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


int main(int argc, char** argv){

    pthread_t fileWatcherThread;
    char comand[MAXCOMANDSIZE];
    int isRunning = 1;
    DropboxClient client;

    //Verifica os argumentos passados
    if(argc < 4){
        printf("Usage: ./dropboxClient <user> <address> <port>\n");
        return -1;
    }
    //Conecta ao servidor
    if(client.connect_server(argv[2], atoi(argv[3])) < 0){
        return -1;
    }
    //Envia o userId do cliente para o servidor
    if(!client.sendUserId(argv[1])){
		fprintf(stderr, "Error logging in\n");
		return -1;
	}
	//Envia um comando get_sync_dir "implicitamente"
	client.getSyncDirComand();

    //Conexão realizada com sucesso
    fprintf(stderr, "DropBox - Sistemas Operacionais 2 - Etapa I\n");
    fprintf(stderr, "André D. Carneiro, Lucas Sievert e Felipe Fuhr\n\n");
    //Define o nome de usuário
    //~ client.setUserId(argv[1]);

    //Cria a thread que verifica por alterações nos arquivos
    // pthread_create(&fileWatcherThread, NULL, fileWatcher, (void*) ".");

    //Lẽ comandos do usuário
    while(isRunning){
        switch (client.readComand(comand, sizeof(comand))){
            case COM_UPLOAD:
                strncpy(comand, &comand[7], sizeof(comand));
                client.send_file(comand);
                break;
            case COM_DOWNLOAD:
                fprintf(stderr, "COM_DOWNLOAD\n");
                fprintf(stderr, "Comand read: \'%s\'\n", comand);
                break;
            case COM_LIST_SERVER:
                fprintf(stderr, "COM_LIST_SERVER\n");
                fprintf(stderr, "Comand read: \'%s\'\n", comand);
                break;
            case COM_LIST_CLIENT:
                fprintf(stderr, "COM_LIST_CLIENT\n");
                fprintf(stderr, "Comand read: \'%s\'\n", comand);
                break;
            case COM_GET_SYNC_DIR:
                client.getSyncDirComand();
                break;
            case COM_EXIT:
                client.close_connection();
                isRunning = 0;
                break;
        }
    }
}
