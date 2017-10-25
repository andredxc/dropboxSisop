#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/inotify.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "dropboxUtil.h"

/*Envia um int ao servidor*/
bool sendInteger(int socket, int message){

    char buffer[12];

    snprintf(buffer, sizeof(buffer), "%d", message);
    if(write(socket, buffer, strlen(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error sending integer %d\n", message);
        return false;
    }
    return true;
}
