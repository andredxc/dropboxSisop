#include <stdio.h>
#include "../Util/dropboxUtil.h"

class DropboxClient{

    #define COM_UPLOAD          1
    #define COM_DOWNLOAD        2
    #define COM_LIST_SERVER     3
    #define COM_LIST_CLIENT     4
    #define COM_GET_SYNC_DIR    5
    #define COM_EXIT            6

    #define MAXCOMANDSIZE       50

    private:
        int _socket;
        char _userId[MAXNAME];

    public:
    //Funções definidas na especificação
        DropboxClient();
        int connect_server(char* host, int port);
        void sync_client();
        void send_file(char* filePath);
        void get_file(char* file);
        void delete_file(char* file);
        void close_connection();
    //Funções extras
        int readComand(char* comandBuffer, int bufferSize);
        bool sendComand(char* comand, int length);
        void getSyncDirComand();
        bool sendUserId(char* userId);

        int getSocket();
        void setUserId(char* userId);

    private:
    //Funções extras
        void* fileWatcher(void* dirPath);
};
