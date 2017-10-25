#include <stdlib.h>

#define MAXNAME 20

//Definição do protocolo de comunicação entre cliente e servidor
#define CP_SYNC_DIR_FOUND       1
#define CP_SYNC_DIR_NOT_FOUND   2

#define CP_LOGIN_SUCCESSFUL		3
#define CP_LOGIN_FAILED			4

#define CP_MAX_MSG_SIZE  		256

//Outras constantes
#define CLIENT_SYNC_DIR_PATH    "./clientFiles/"
#define SERVER_SYNC_DIR_PATH    "./serverFiles/"

bool sendInteger(int socket, int message);
