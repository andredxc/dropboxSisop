#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "clientProxy.h"

int main(){
    ClientProxy proxy;
    int isRunning=1;

    //Inicialização do socket de comunicação com o cliente
    if(proxy.initialize_clientConnection() < 0){
        return -1;
    }


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
