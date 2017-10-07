
#define MAXNAME 20
#define MAXFILES 20

typedef struct file_info{
    char name[MAXNAME];
    char extensions[MAXNAME];
    char last_modified[MAXFILES];
    int size;
} FILE_INFO;

typedef struct client{
    int devices[2];
    char userid[MAXNAME];
    FILE_INFO file_info[MAXFILES];
    int logged_in;
} CLIENT;

/*
*   Funções definidas na especificação
*/
void sync_server();
void receive_file(char* file);
void send_file(char* file);

/*
*   Funções extras
*/
int initServer();
