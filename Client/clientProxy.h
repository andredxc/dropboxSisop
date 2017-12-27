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
        std::string _clientName;
        bool _isClientConnected;
        bool _isServerConnected;
        char _userId[MAXNAME];
        pthread_mutex_t _communicationMutex;
        pthread_mutex_t _clientMutex;
        pthread_mutex_t _serverMutex;
        int _communicationSocket;

        SSL *_sslClient;
        SSL *_sslServer;
        SSL_CTX *_ctx;

        // Socket de comunicação com o servidor
        int _serverSocket;
        std::vector<CLIENT> _clients;
        pthread_mutex_t _clientStructMutex;

        int _error;
        bool _first_login;

        // Gere se thread está disponível
        int _clientThreadState;
        int _serverThreadState;

        std::list<std::pair<std::string, int> > _server_list; // lista a receber lista de servidores
        std::list<std::pair<std::string, int> >::iterator _it; // iterador da lista, indica servidor conectado

    public:
        // Recebe informações do cliente
        ClientProxy(){
            _clientThreadState = 0;
            _serverThreadState = 0;
            _communicationSocket = -1;

            _isClientConnected = false;
            _isServerConnected = false;
            _error = 0;

            pthread_mutex_init(&_communicationMutex, NULL);
            pthread_mutex_init(&_clientMutex, NULL);
            pthread_mutex_init(&_serverMutex, NULL);


            _server_list = get_serverList();
            _it = _server_list.begin();

            _first_login = true;
        };
        int initialize_clientConnection(int port);
        int listenAndAccept();
        int get_clientSocket();
        int send_message(int message);

        // Manda informações ao servidor
        int connect_server(char* host, int port);

        void set_clientThreadState(int thread_state);
        int get_clientThreadState();
        void set_serverThreadState(int thread_state);
        int get_serverThreadState();

        SSL *getServerSocket();
        SSL *get_communicationSocket();

        pthread_t *clientWatcher();
        pthread_t *serverWatcher();
        static void* handle_clientConnection(void *arg);
        static void* handle_serverConnection(void *arg);
        void lock_socket();
        void unlock_socket();
        int check_socket(int socket);
        void closeConnection(int socket);
        void close_clientConnection();
        void close_serverConnection();
        std::pair<std::string, int> get_currentServerName();
        int getClientConnected();
        int getServerConnected();

        void lockClientSocket();
        void unlockClientSocket();
        void lockServerSocket();
        void unlockServerSocket();

        void reset_serverList();
        void increment_currentServer();
        bool compare_itEnd();

        int get_error();
        void set_error(int error);

    private:

};

/*Utilizada na criação da thread pois a função tem que ser static*/
struct proxyAndSocket{
    ClientProxy* instance;
    int* socket;
};

#endif
