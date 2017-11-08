#ifndef DROPBOXCLIENT_H
#define DROPBOXCLIENT_H

#include <stdio.h>
#include "../Util/dropboxUtil.h"

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

    public:
    //Funções definidas na especificação
        DropboxClient();
        int connect_server(char* host, int port);
        void sync_client();
        void send_file(char* filePath);
        void get_file(char* filePath, char *destination);
        void delete_file(char* file);
        void close_connection();
    //Funções extras
        int readComand(char* comandBuffer, int bufferSize);
        void getSyncDirComand();
        void listServerComand();
        bool sendUserId(char* userId);

        int getSocket();
        bool getIsConnected();
        char* getUserId();
        void setUserId(char* userId);

        void list_client();

        static void* fileWatcher(void* clientClass);
};

#endif
