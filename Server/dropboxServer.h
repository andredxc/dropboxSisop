#ifndef DROPBOXSERVER_H
#define DROPBOXSERVER_H

#include <stdio.h>
#include <vector>
#include <list>
#include <utility>
#include <iostream>
#include <string>
#include <list>

#include "../Util/dropboxUtil.h"

#define MAXNAME 20
#define MAXFILES 20
#define MAXUSERS 2

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
        int _serverSocket;
        std::vector<CLIENT> _clients;
        pthread_mutex_t _clientStructMutex;

        //
        std::list<std::pair<char*, char*> > _server_list; // lista de servidores por host e porta
        std::list<std::pair<char*, char*> >::iterator _server_it; // iterador da lista
        int _myIndex; // index do servidor no server_list
        int _myID; // ID no algoritmo de seleção

    public:
        DropboxServer();
    //Funções definidas na especificação
        void sync_server(int socket, char* userId);
        void receive_file(int socket, char* userId, char* file);
        void send_file(int socket, char* userId, char* file);
    //Funções extras
        int initialize(int port);
        pthread_t *handleConnection(int* socket);
        int listenAndAccept();
        void closeConnection(int socket);

        int getSocket();

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
        void deleteFile(int socket, char* userId);
        void listServer(int socket, char* userId);
        void lockFile(int socket, char* userId);
        void unlockFile(int socket, char* userId);

        // TODO 08/12 (Felipe Führ)
        int initServerList(); // lista de servidores
        int initMyIndex(); // adquire próprio índice na lista de servidores

        int beginElection(); // começa eleição
        int sendElection(); // manda mensagem de eleição à todos os elementos posteriores à ele na lista de servidores
        int sendCoordinator(); // manda mensagem coordinator

        // TODO data posterior
        // Função que transfere alterações aos backups
        // Função que transfere todo o servidor para o host e porta que conversam com o cliente

};

/*Utilizada na criação da thread pois a função tem que ser static*/
struct serverAndSocket{
    DropboxServer* instance;
    int* socket;
};

#endif
