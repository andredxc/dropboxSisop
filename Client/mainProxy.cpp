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

    /// TODO: Conectar a vários servidores, gerenciar mensagens dadas por eles (possivelmente tudo isso tem de ser transferido para o while abaixo)

    // Conecta ao servidor TODO: trocar argumentos para um txt, com lista de servidores
    if(proxy.connect_server("localhost", 4000) < 0){
        return -1;
    }
    ///////////////////////////////////////////////////////////

    fprintf(stderr, "Server is listening.\n");


    // TODO: if checkServer == 1 else abaixo
    // Recebe informação do Cliente e manda ao Servidor e vice-versa
    pthread_t *communicationThread;
    communicationThread = proxy.communicationWatcher();

    pthread_join(*communicationThread, NULL);

    close(proxy.get_clientSocket());
    return 0;

}
