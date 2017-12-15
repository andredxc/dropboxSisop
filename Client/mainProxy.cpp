#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "clientProxy.h"

int main(int argc, char** argv){

    ClientProxy proxy;
    int comunicationSocket, isRunning=1;

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

    //Espera por conexões do cliente e dispara threads
    while(isRunning){
        fprintf(stderr, "Server is listening.\n");
        comunicationSocket = proxy.listenAndAccept();

        // TODO: if checkServer == 1 else abaixo
        // Recebe informação do Cliente e manda ao Servidor
        comunicationSocket = proxy.handle_clientConnection(comunicationSocket);
        // Recebe informação do Servidor e manda ao cliente
        comunicationSocket = proxy.handle_serverConnection(comunicationSocket);
        // Manda informação ao Servidor
    }

    close(proxy.get_clientSocket());
    return 0;

}
