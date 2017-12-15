#ifndef CLIENTPROXY_H
#define CLIENTPROXY_H

#include <stdio.h>
#include <vector>
#include "../Util/dropboxUtil.h"

#define MAXNAME 20
#define MAXFILES 20

#define PROXY_PORT 4001 // Após alterações, usaremos o abaixo.
#define SERVER_HOST_PORT_FILE "./server_host-port.txt" // ip route get 8.8.8.8 | awk '{print $NF; exit}' dá o ip no ubuntu

#define SERVER_MAX_CLIENTES 20
#define SERVER_BACKLOG 20

class ClientProxy{

    private:
        // Socket de comunicação com o cliente
        int _clientSocket;
        bool _isConnected;
        char _userId[MAXNAME];
        pthread_mutex_t _comunicationMutex;

        // Socket de comunicação com o servidor
        int _serverSocket;
        std::vector<CLIENT> _clients;
        pthread_mutex_t _clientStructMutex;

    public:
        // Recebe informações do cliente
        ClientProxy(){ };
        int initialize_clientConnection();
        int listenAndAccept();
        int handle_clientConnection(int socket);
        int handle_serverConnection(int socket);
        int get_clientSocket();
        int send_message(int message);

        // Manda informações ao servidor
        int connect_server(char* host, int port);

    private:

};

#endif
