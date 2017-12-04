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
#include <unistd.h>

int main(int argc, char** argv){

    pthread_t fileWatcherThread;
    char comand[MAXCOMANDSIZE], syncDirPath[256];
    int isRunning = 1;
    DropboxClient client;
    char buffer[512];

    // Verifica os argumentos passados (retorna formato adequado e interrompe execução em caso de erro)
    if(argc < 4){
        printf("Usage: ./dropboxClient <user> <address> <port>\n");
        return -1;
    }

    // Conecta ao servidor
    if(client.connect_server(argv[2], atoi(argv[3])) < 0){
        return -1;
    }

    // Envia o userId do cliente para o servidor
    if(!client.sendUserId(argv[1])){
    		fprintf(stderr, "Error logging in\n");
    		return -1;
	  }

  	// Envia um comando get_sync_dir "implicitamente"
  	client.getSyncDirComand();

    // Conexão realizada com sucesso
    fprintf(stderr, "DropBox - Sistemas Operacionais 2 - Etapa I\n");
    fprintf(stderr, "André D. Carneiro, Lucas Sievert e Felipe Fuhr\n\n");
    // Define o nome de usuário
    // client.setUserId(argv[1]);

    //Cria a thread que verifica por alterações nos arquivos
    snprintf(syncDirPath, sizeof(syncDirPath), "%ssync_dir_%s", CLIENT_SYNC_DIR_PATH, client.getUserId());
    pthread_create(&fileWatcherThread, NULL, DropboxClient::fileWatcher, (void*) &client);

    //Lê comandos do usuário
    while(client.getIsConnected()){
        switch (client.readComand(comand, sizeof(comand))){
            case COM_UPLOAD:
                strncpy(comand, &comand[7], sizeof(comand));
                client.lockSocket();
                client.send_file(comand);
                client.unlockSocket();
                break;
            case COM_DOWNLOAD:
                strncpy(comand, &comand[9], sizeof(comand));
                client.lockSocket();
                client.get_file(comand, getcwd(buffer, sizeof(buffer)));
                client.unlockSocket();
                break;
            case COM_LIST_SERVER:
                client.lockSocket();
                client.listServerComand();
                client.unlockSocket();
                break;
            case COM_LIST_CLIENT:
                client.list_client();
                break;
            case COM_GET_SYNC_DIR:
                client.lockSocket();
                client.getSyncDirComand();
                client.unlockSocket();
                break;
            case COM_EXIT:
                client.lockSocket();
                client.close_connection();
                client.unlockSocket();
                break;
        }
    }
}
