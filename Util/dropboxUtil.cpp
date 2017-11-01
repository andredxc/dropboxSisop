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

/*Envia um int*/
bool sendInteger(int socket, int message){

    char buffer[CP_MAX_MSG_SIZE];

    snprintf(buffer, sizeof(buffer), "%d", message);
    if(write(socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxUtil - Error sending integer %d\n", message);
        return false;
    }
    return true;
}

/*Recebe um int e confirma seu valor de acordo com o esperado*/
bool receiveExpectedInt(int socket, int message){

	char buffer[CP_MAX_MSG_SIZE];

	bzero(buffer, sizeof(buffer));
	if(read(socket, buffer, sizeof(buffer)) < 0){
		fprintf(stderr, "DropboxUtil - Didn't receive expected integer\n");
		return false;
	}
	if(atoi(buffer) != message){
		fprintf(stderr, "DropboxUtil - Int received wasn't expected (%d)\n", atoi(buffer));
		return false;
	}
	return true;
}
