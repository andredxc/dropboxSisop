#ifndef DROPBOXCLIENT_H
#define DROPBOXCLIENT_H

#include <stdio.h>
#include "../Util/dropboxUtil.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

class DropboxClient{

    #define COM_UPLOAD          1
    #define COM_DOWNLOAD        2
    #define COM_LIST_SERVER     3
    #define COM_LIST_CLIENT     4
    #define COM_GET_SYNC_DIR    5
    #define COM_EXIT            6

    #define MAXCOMANDSIZE       256

    private:
        int _socket;
        bool _isConnected;
        char _userId[MAXNAME];
        pthread_mutex_t _comunicationMutex;

    public:
    //Funções definidas na especificação
        DropboxClient();
        int connect_server(char* host, int port);
        void sync_client(SSL *ssl);
        void send_file(char* filePath, SSL *ssl);
        void get_file(char* filePath, char *destination, SSL *ssl);
        void delete_file(char* file, SSL *ssl);
        void close_connection();
    //Funções extras
        int readComand(char* comandBuffer, int bufferSize);
        void getSyncDirComand();
        void listServerComand(SSL *ssl);
        bool sendUserId(char* userId, SSL *ssl);
        int getSocket();
        bool getIsConnected();
        char* getUserId();
        void setUserId(char* userId);
        int lockFile(char* fileName, SSL *ssl);
        void unlockFile(char* fileName, SSL *ssl);

        void lockSocket();
        void unlockSocket();

        void list_client();

        static void* fileWatcher(void* clientClass);
};

#endif
