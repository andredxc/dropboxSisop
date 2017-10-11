#include <stdio.h>

class DropboxClient{

    #define COM_UPLOAD          1
    #define COM_DOWNLOAD        2
    #define COM_LIST_SERVER     3
    #define COM_LIST_CLIENT     4
    #define COM_GET_SYNC_DIR    5
    #define COM_EXIT            6
    
    #define MAXNAME             20
    #define MAXCOMANDSIZE       50

    public:
    //Funções definidas na especificação
        DropboxClient();
        int connect_server(char* host, int port);
        void sync_client();
        void send_file(char* file);
        void get_file(char* file);
        void delete_file(char* file);
        void close_connection();
    //Funções extras
        int readComand(char* comandBuffer);

    private:
    //Funções extras
        void* fileWatcher(void* dirPath);
};