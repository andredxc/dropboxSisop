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
#include <sys/stat.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>
#include <list>
#include <fstream>
#include <string>

#include "dropboxUtil.h"

/* Envia um int */
bool sendInteger(SSL* ssl, int message){

    char buffer[CP_MAX_MSG_SIZE];

    snprintf(buffer, sizeof(buffer), "%d", message);
    if(SSL_write(ssl, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxUtil - Error sending integer %d\n", message);
        return false;
    }
    return true;
}

/* Recebe um int e confirma seu valor de acordo com o esperado */
bool receiveExpectedInt(SSL* ssl, int message){

  	char buffer[CP_MAX_MSG_SIZE];

  	bzero(buffer, sizeof(buffer));
  	if(SSL_read(ssl, buffer, sizeof(buffer)) <= 0){
  		fprintf(stderr, "DropboxUtil - Didn't receive expected integer %s (%d)\n", getCPMessage(message), message);
		return false;
  	}
    if(message == -1) return true;
  	else if(atoi(buffer) != message){
  		fprintf(stderr, "DropboxUtil - Int received wasn't expected (received: %s(%d), expected: %s(%d))\n", getCPMessage(atoi(buffer)), atoi(buffer), getCPMessage(message), message);
	    return false;
  	}
  	return true;
}

/* Retorna nome de um arquivo */
const char *getFileName(const char *filename){
    const char *name = strrchr(filename, '/');
    if(!name || name == filename) return "";
    return name;
}

/* Retorna a extensão de um arquivo */
const char *getFileExtension(const char *filename){
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

/* Descobre o tempo de modificação de um arquivo */
void getMTime(const char* filePath, char* buffer, int bufferSize){

    struct stat statBuff;

    if(stat(filePath, &statBuff)){
        bzero(buffer, bufferSize);
        return;
    }
    strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", localtime(&statBuff.st_mtime));
}

void getFileSize(const char* filePath, int* buffer){

    FILE *curFile;
    long curFileSize;

    if(!(curFile = fopen(filePath, "r"))){
        fprintf(stderr, "DropboxClient - Erro abrindo arquivo %s em\n", filePath);
        *buffer = -1;
        return;
    }

    // Descobrindo tamanho do arquivo
    fseek(curFile, 0, SEEK_END);
    curFileSize = ftell(curFile);
    fseek(curFile, 0, SEEK_SET);

    *buffer = curFileSize;
    fclose(curFile);
}

void getFileInfo(const char* filePath, struct file_info *info){

    // Monta estrutura file_info a partir do arquivo de endereço filePath
    snprintf(info->name, sizeof(info->name), "%s", basename(filePath));
    snprintf(info->extension, sizeof(info->extension), "%s", getFileExtension(filePath));
    getMTime(filePath, info->last_modified, sizeof(info->last_modified));
    getFileSize(filePath, &(info->size));

}

/* Imprime a struct file_info na tela */
void printFileInfo(FILE_INFO file){

    fprintf(stderr, "%30s     %5s     %20s     %5d\n",file.name, file.extension, file.last_modified, file.size);
    // fprintf(stderr, "    Printing file --------\n");
    // fprintf(stderr, "    Name: %s\n", file.name);
    // fprintf(stderr, "    Extension: %s\n", file.extension);
    // fprintf(stderr, "    M time: %s\n", file.last_modified);
    // fprintf(stderr, "    Size: %d\n", file.size);

}

/* Imprime a struct client na tela */
void printClient(CLIENT client, bool printAll){

    int i;

    fprintf(stderr, "Printing client --------\n");
    fprintf(stderr, "User ID: %s\n", client.userId);
    fprintf(stderr, "Devices: [0]: %d    [1]: %d\n", client.devices[0], client.devices[1]);
    fprintf(stderr, "Logged in: %d\n", client.logged_in);
    fprintf(stderr, "Files:\n");
    for (i = 0; i < MAXFILES; i++) {
        if(strlen(client.file_info[i].name) > 0){
            if(printAll){
                // Imprime todas as informações de todos os arquivos
                printFileInfo(client.file_info[i]);
            }
            else{
                // Imprime de forma reduzida
                fprintf(stderr, "    %s\n", client.file_info[i].name);
            }
        }
    }
    fprintf(stderr, "End of client ----------\n");
}

/* Cria uma variável tipo time_t a partir de uma string */
time_t convertTimeString(const char* timeString){

    struct tm tm;
    strptime(timeString, "%Y-%m-%d %H:%M:%S", &tm);
    return mktime(&tm);
}

/* Retorna uma variável time_t com o M time do arquivo */
time_t getMTimeValue(const char* filePath){

    struct stat statBuff;

    if(stat(filePath, &statBuff)){
        return 0;
    }
    return statBuff.st_mtime;
}

/* Retorna a string correspondente ao código do protocolo de comunicação */
const char* getCPMessage(int cpCode){

    switch(cpCode){

        case CP_SYNC_DIR_FOUND: return "CP_SYNC_DIR_FOUND";
        case CP_SYNC_DIR_NOT_FOUND: return "CP_SYNC_DIR_NOT_FOUND";
        case CP_LOGIN_SUCCESSFUL: return "CP_LOGIN_SUCCESSFUL";
        case CP_LOGIN_FAILED: return "CP_LOGIN_FAILED";
        case CP_CLIENT_SEND_FILE: return "CP_CLIENT_SEND_FILE";
        case CP_CLIENT_SEND_FILE_ACK: return "CP_CLIENT_SEND_FILE_ACK";
        case CP_CLIENT_SEND_FILE_SIZE_ACK: return "CP_CLIENT_SEND_FILE_SIZE_ACK";
        case CP_CLIENT_END_CONNECTION: return "CP_CLIENT_END_CONNECTION";
        case CP_SEND_FILE_COMPLETE: return "CP_SEND_FILE_COMPLETE";
        case CP_FILE_PART_RECEIVED: return "CP_FILE_PART_RECEIVED";
        case CP_SEND_FILE_COMPLETE_ACK: return "CP_SEND_FILE_COMPLETE_ACK";
        case CP_SYNC_CLIENT: return "CP_SYNC_CLIENT";
        case CP_SYNC_FILE_COMPUTED: return "CP_SYNC_FILE_COMPUTED";
        case CP_SYNC_FILE_OK: return "CP_SYNC_FILE_OK";
        case CP_SYNC_DOWNLOAD_FILE: return "CP_SYNC_DOWNLOAD_FILE";
        case CP_SYNC_FILE_NOT_FOUND: return "CP_SYNC_FILE_NOT_FOUND";
        case CP_CLIENT_GET_FILE: return "CP_CLIENT_GET_FILE";
        case CP_CLIENT_GET_FILE_ACK: return "CP_CLIENT_GET_FILE_ACK";
        case CP_CLIENT_GET_FILE_SIZE_ACK: return "CP_CLIENT_GET_FILE_SIZE_ACK";
        case CP_CLIENT_GET_FILE_EXISTS: return "CP_CLIENT_GET_FILE_EXISTS";
        case CP_CLIENT_GET_FILE_EXISTS_ACK: return "CP_CLIENT_GET_FILE_EXISTS_ACK";
        case CP_LIST_SERVER: return "CP_LIST_SERVER";
        case CP_LIST_SERVER_ACK: return "CP_LIST_SERVER_ACK";
        case CP_LIST_SERVER_FILE_OK: return "CP_LIST_SERVER_FILE_OK";
        case CP_CLIENT_DELETE_FILE: return "CP_CLIENT_DELETE_FILE";
        case CP_CLIENT_DELETE_FILE_ACK: return "CP_CLIENT_DELETE_FILE_ACK";
        case CP_CLIENT_GET_FILE_LOCK: return "CP_CLIENT_GET_FILE_LOCK";
        case CP_CLIENT_GET_FILE_LOCK_ACK: return "CP_CLIENT_GET_FILE_LOCK_ACK";
        case CP_CLIENT_GET_FILE_LOCK_SUCCESS: return "CP_CLIENT_GET_FILE_LOCK_SUCCESS";
        case CP_CLIENT_GET_FILE_UNLOCK: return "CP_CLIENT_GET_FILE_UNLOCK";
        case CP_CLIENT_GET_FILE_UNLOCK_ACK: return "CP_CLIENT_GET_FILE_UNLOCK_ACK";
        case CP_CLIENT_GET_FILE_UNLOCK_SUCCESS: return "CP_CLIENT_GET_FILE_UNLOCK_SUCCESS";
        case CP_CLIENT_GET_FILE_UNLOCK_FAIL: return "CP_CLIENT_GET_FILE_UNLOCK_FAIL";
        case CP_CLIENT_GET_FILE_NOEXISTS: return "CP_CLIENT_GET_FILE_NOEXISTS";
        case CP_CLIENT_NUMBER_OF_FILES_ACK: return "CP_CLIENT_NUMBER_OF_FILES_ACK";
        default: return "UNKNOWN";
    }
}

std::list<std::pair<std::string, int> > get_serverList()
{
    std::list<std::pair<std::string, int> > server_list; // lista a receber lista de servidores
    std::list<std::pair<std::string, int> >::iterator it; // iterador da lista
    std::string host;
    int port;

    std::ifstream infile(SERVER_LIST_TXT);

    if (infile) { }
    else {
        std::cerr << "Couldn't open\n\n";
    }

    while (infile >> host >> port)
    {
         server_list.push_back(std::make_pair(host, port));
         // std::cerr << host << " " << port << "\n";
    }
    //for(it = server_list.begin(); it != server_list.end(); it++)
    //{
    //     std::cerr << it->first << " " << it->second << "\n";
    //}
    //std::cerr << "\n";

    return server_list;
}
