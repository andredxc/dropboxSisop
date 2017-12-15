#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "clientProxy.h"

int main(int argc, char** argv){





      /*DropboxServer server;
      int comunicationSocket, isRunning=1;

      //Inicialização do servidor
      if(server.initialize() < 0){
          return -1;
      }

      //Esperando por conexões e disparando threads
      while(isRunning){
          fprintf(stderr, "Server is listening.\n");
          comunicationSocket = server.listenAndAccept();
          server.handleConnection(&comunicationSocket);
      }

      close(server.getSocket());
      return 0;
      */



    ClientProxy proxy;
    int comunicationSocket, isRunning=1;

    //Inicialização do proxy
    if(proxy.initialize_clientConnection() < 0){
        return -1;
    }
    /*
    /// TODO: Conectar a vários servidores, gerenciar mensagens dadas por eles (possivelmente tudo isso tem de ser transferido para o while abaixo)

    // Conecta ao servidor TODO: trocar argumentos para um txt, com lista de servidores
    if(proxy.connect_server("localhost", 4000) < 0){
        return -1;
    }
    ///////////////////////////////////////////////////////////

    //Esperando por conexões e disparando threads
    while(isRunning){
        fprintf(stderr, "Server is listening.\n");
        //comunicationSocket = proxy.listenAndAccept();
        //message = proxy.handle_connection(&comunicationSocket);
        //proxy.send_message(message);
    }

    close(proxy.get_clientSocket());
    return 0;
    */
}
