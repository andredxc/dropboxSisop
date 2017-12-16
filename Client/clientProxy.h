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

        // Gere se thread está disponível
        int _clientThreadState;
        int _serverThreadState;
        pthread_mutex_t _clientMutex;
        pthread_mutex_t _serverMutex;

    public:
        // Recebe informações do cliente
        ClientProxy(){
            _clientThreadState = 0;
            _serverThreadState = 0;
        };
        int initialize_serverConnection();
        int listenAndAccept();
        int get_clientSocket();
        int send_message(int message);

        // Manda informações ao servidor
        int connect_server(char* host, int port);

        void set_clientThreadState(int thread_state);
        int get_clientThreadState();
        void set_serverThreadState(int thread_state);
        int get_serverThreadState();

        int getServerSocket();
        int getClientSocket();
        pthread_t *clientWatcher();
        pthread_t *serverWatcher();
        static void* handle_clientConnection(void *arg);
        static void* handle_serverConnection(void *arg);
        void lock_serverSocket();
        void unlock_serverSocket();
        void lock_clientSocket();
        void unlock_clientSocket();

    private:

};

/*Utilizada na criação da thread pois a função tem que ser static*/
typedef struct proxyAndSocket{
    ClientProxy* instance;
    int* socket;
};

#endif
