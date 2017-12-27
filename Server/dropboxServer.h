#ifndef DROPBOXSERVER_H
#define DROPBOXSERVER_H

#include <stdio.h>
#include <vector>
#include <list>
#include <utility>
#include <iostream>
#include <string>
#include <list>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../Util/dropboxUtil.h"

#define MAXNAME 20
#define MAXFILES 20
#define MAXUSERS 5
#define MAXCONNECTIONS 5

#define SERVER_PORT 4000 // Após alterações, usaremos o abaixo.
#define SERVER_HOST_PORT_FILE "./server_host-port.txt" // ip route get 8.8.8.8 | awk '{print $NF; exit}' dá o ip no ubuntu

#define SERVER_MAX_CLIENTES 20
#define SERVER_BACKLOG 20

typedef struct file_info{
    char name[MAXNAME];
    char extension[MAXNAME];
    char last_modified[MAXFILES];
    int size;
    std::vector<int> lock;
} FILE_INFO;

typedef struct client{
    int devices[MAXUSERS];
    char userId[MAXNAME];
    FILE_INFO file_info[MAXFILES];
    int logged_in;
} CLIENT;

class DropboxServer{

    private:

        int _max_connections;
        int _serverSocket;
        std::vector<CLIENT> _clients;
        pthread_mutex_t _clientStructMutex;
        SSL_CTX *_ctx;
        //
        // Lista de Servidores vinda do arquivo txt
        std::list<std::pair<std::string, int> > _server_list; // lista a receber lista de servidores
        std::list<std::pair<std::string, int> >::iterator _it1; // iterador da lista, indica servidor conectado
        // Sockets dos Servidores
        std::list<std::pair<SSL *,int> > _sockets_list; // lista a receber lista de servidores
        std::list<std::pair<SSL *,int> >::iterator _it2; // lista a receber lista de servidores
        int _leaderSocket; // socket de comunicação com líder

        std::pair<std::string, int> _my_hostport; // host e port do atual servidor
        bool _imLeader;
        int _myPosition;

    public:
        DropboxServer();
    //Funções definidas na especificação
        void sync_server(int socket, SSL *ssl, char* userId);
        void receive_file(int socket, SSL *ssl, char* userId, char* file);
        void send_file(int socket, SSL *ssl, char* userId, char* file);

        void findLeader(int socket, char *userId);
    //Funções extras
        int initialize(int port);
        pthread_t *handleConnection(std::pair<int, SSL *> socket_ssl);
        std::pair<int, SSL *> listenAndAccept();
        void closeConnection(int socket);

        int getSocket();

        std::pair<std::string, int> getHostPort();
        int get_serverListPosition();
        SSL *connect_server(char* host, int port);
        void connectToServers();
        int get_myposition();
        int get_imleader();
        void elect_newLeader();

    private:
    //Funções extras
		    static void* handleConnectionThread(void* args);
        bool assignNewFile(char* fileName, char* fileMTime, int fileSize, char* userId);
        bool logInClient(int socket, char* userId);
        void logOutClient(int socket, char* userId);
        int countUserFiles(char* userId);
        int findUserIndex(const char* userId);
        void recoverData();
        int findUserFile(char* userId, char* fileName);
        void deleteFile(int socket, SSL *ssl, char* userId);
        void listServer(int socket, SSL *ssl, char* userId);
        void lockFile(int socket, SSL *ssl, char* userId);
        void unlockFile(int socket, SSL *ssl, char* userId);

        int initServerList(); // lista de servidores
        int initMyIndex(); // adquire próprio índice na lista de servidores

        int beginElection(); // começa eleição
        int sendElection(); // manda mensagem de eleição à todos os elementos posteriores à ele na lista de servidores
        int sendCoordinator(); // manda mensagem coordinator

        // Servers
        std::list<std::pair<SSL *,int> >::iterator getIt2();
        void incrementIt2();
        void resetIt2();
        std::list<std::pair<SSL *,int> >::iterator endIt2();
        void setIt22(int val);

        int justListen();

};

/*Utilizada na criação da thread pois a função tem que ser static*/
struct serverAndSocket{
    DropboxServer* instance;
    int socket;
    SSL *ssl;
};

#endif
