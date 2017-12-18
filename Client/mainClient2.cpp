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
#include <unistd.h>
#include "dropboxClient.h"
#include "clientProxy.h"

int client_process(int argc, char** argv, int *client_connected){
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
    *client_connected = 1;

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

int proxy_process(){
    ClientProxy proxy;
    int isRunning=1;

    //Inicialização do socket de comunicação com o cliente
    if(proxy.initialize_clientConnection() < 0){
        return -1;
    }

    /// TODO: Conectar a vários servidores, gerenciar mensagens dadas por eles (possivelmente tudo isso tem de ser transferido para o while abaixo)

    // Conecta ao servidor TODO: trocar argumentos para um txt, com lista de servidores
    if(proxy.connect_server("localhost", 4000) < 0){
        return -1;
    }
    ///////////////////////////////////////////////////////////

    fprintf(stderr, "Server is listening.\n");


    // TODO: if checkServer == 1 else abaixo
    // Recebe informação do Cliente e manda ao Servidor e vice-versa
    pthread_t *clientThread;
    pthread_t *serverThread;

    proxy.set_communicationSocket(proxy.listenAndAccept());

    clientThread = proxy.clientWatcher();
    serverThread = proxy.serverWatcher();

    pthread_join(*clientThread, NULL);
    pthread_join(*serverThread, NULL);

    close(proxy.get_clientSocket());
    return 0;
}

int main(int argc, char** argv){
    int *client_connected = (int*)malloc(sizeof(int));
    *client_connected = 0;
    int pid = fork();

    if(pid < 0) fprintf(stderr, "%s\n", "Error! Couldn't open Client.");
    else if(pid == 0){
        while(*client_connected == 0){;}
        system("konsole");
        proxy_process();
    }
    else{
        client_process(argc, argv, client_connected);
    }
}
