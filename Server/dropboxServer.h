#ifndef DROPBOXSERVER_H
#define DROPBOXSERVER_H

#include <stdio.h>
#include <vector>
#include "../Util/dropboxUtil.h"

#define MAXNAME 20
#define MAXFILES 20

#define SERVER_PORT 4000
#define SERVER_MAX_CLIENTES 20
#define SERVER_BACKLOG 20

typedef struct file_info{
    char name[MAXNAME];
    char extension[MAXNAME];
    char last_modified[MAXFILES];
    int size;
} FILE_INFO;

typedef struct client{
    int devices[2];
    char userId[MAXNAME];
    FILE_INFO file_info[MAXFILES];
    int logged_in;
} CLIENT;

class DropboxServer{

    private:
        int _serverSocket;
        std::vector<CLIENT> _clients;
        pthread_mutex_t _clientStructMutex;;

    public:
        DropboxServer();
    //Funções definidas na especificação
        void sync_server(int socket, char* userId);
        void receive_file(int socket, char* userId, char* file);
        void send_file(int socket, char* userId, char* file);
    //Funções extras
        int initialize();
		void handleConnection(int* socket);
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

};

#endif
