#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <iostream>
#include <dirent.h>
#include <string>
#include <netdb.h>
#include <sys/stat.h>
#include <dirent.h>
#include "dropboxServer.h"
#include "../Util/dropboxUtil.h"

/*Constructor*/
DropboxServer::DropboxServer(){
    pthread_mutex_init(&_clientStructMutex, NULL);
    _server_list = get_serverList();
    _it1 = (_server_list).begin();
}

//--------------------------------------------------FUNÇÕES DEFINIDAS NA ESPECIFICAÇÃO

/*Sincroniza a parsta sync_dir com o cliente*/
void DropboxServer::sync_server(int socket, char* userId){

    char dirPath[512], buffer[CP_MAX_MSG_SIZE];
    int i, errorCode, numberOfFiles, userIndex;

    //Cria os diretórios referentes ao sync_dir
    if(access(SERVER_SYNC_DIR_PATH, F_OK) == -1){
        //Diretório dos sync_dir não existe
        if((errorCode = mkdir(SERVER_SYNC_DIR_PATH, S_IRWXU)) != 0){
            fprintf(stderr, "Socket %d - Error %d creating %s\n", socket, errorCode, SERVER_SYNC_DIR_PATH);
            return;
        }
    }
    snprintf(dirPath, sizeof(dirPath), "%ssync_dir_%s", SERVER_SYNC_DIR_PATH, userId);
    if(access(dirPath, F_OK) == -1){
        //Diretório do sync_dir desse usuário não existe
        fprintf(stderr, "Socket %d - Creating directory \'%s\'\n", socket, dirPath);
        if((errorCode = mkdir(dirPath, S_IRWXU)) != 0){
            fprintf(stderr, "Socket %d - Error %d creating %s\n", socket, errorCode, dirPath);
            return;
        }
    }

    //Envia o número de arquivos
    numberOfFiles = countUserFiles(userId);
    userIndex = findUserIndex(userId);
    if(!sendInteger(_ssl, numberOfFiles)){
        fprintf(stderr, "Socket %d - Error sending number of files\n", socket);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
    }

    // Envia um ack pelo número de arquivoss
    if(!receiveExpectedInt(_ssl, CP_CLIENT_NUMBER_OF_FILES_ACK)){
        fprintf(stderr, "Socket %d - Erro sending CP_CLIENT_NUMBER_OF_FILES_ACK\n", socket);
        return;
    }

    //Manda as informações de todos os arquivos
    for(i = 0; i < numberOfFiles; i++){

        //Envia o nome do arquivo
        bzero(buffer, sizeof(buffer));
        strncpy(buffer, _clients.at(userIndex).file_info[i].name, sizeof(buffer));
        if(SSL_write(_ssl, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "Socket %d - Error sending file %d name (%s)\n", socket, i+1, buffer);
            return;
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) if(SSL_read((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
        }

        // Ack pelo nome do arquivo
        if(!receiveExpectedInt(_ssl, CP_CLIENT_GET_FILE_ACK)){
            fprintf(stderr, "Socket %d - Error receiving CP_CLIENT_GET_FILE_ACK\n", socket);
            return;
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) sendInteger((*_it2).first, CP_CLIENT_GET_FILE_ACK);
        }

        //Envia o M time do arquivo
        bzero(buffer, sizeof(buffer));
        strncpy(buffer, _clients.at(userIndex).file_info[i].last_modified, sizeof(buffer));
        if(SSL_write(_ssl, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "Socket %d - Error sending file %d M time (%s)\n", socket, i+1, buffer);
            return;
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) if(SSL_read((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
        }

        //Espera resposta indicando o que fazer
        bzero(buffer, sizeof(buffer));
        if(SSL_read(_ssl, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "Socket %d - Error receiving answer from client\n", socket);
            return;
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) if(SSL_write((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
        }

        switch (atoi(buffer)) {
            case CP_SYNC_FILE_OK:
                //Arquivo está sincronizado
                break;
            case CP_CLIENT_SEND_FILE:
                //Cliente vai fazer upload do arquivo
                if(!sendInteger(_ssl, CP_CLIENT_SEND_FILE_ACK)){
                    fprintf(stderr, "Socket %d - Error sending CP_CLIENT_SEND_FILE_ACK\n", socket);
                    continue;
                }
                if(_imLeader == true){
                    for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                        if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
                }
                //Espera pelo nome do arquivo
                bzero(buffer, sizeof(buffer));
                int recv_file;
                if((recv_file = SSL_read(_ssl, buffer, sizeof(buffer))) < 0){
                    fprintf(stderr, "Socket %d - Error receiving file name from client\n", socket);
                }
                if(_imLeader == true){
                    for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                        if((*_it2).second == 1) if(SSL_write((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
                }
                if(recv_file >= 0){
                    receive_file(socket, userId, buffer);
                }
                break;
            case CP_CLIENT_GET_FILE:
                //Arquivo deve ser baixado do servidor
                if(!sendInteger(_ssl, CP_CLIENT_GET_FILE_ACK)){
                    fprintf(stderr, "Socket %d - Error sending CP_CLIENT_GET_FILE_ACK\n", socket);
                    continue;
                }
                if(_imLeader == true){
                    for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                        if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
                }

                //Recebe o nome do arquivo
                bzero(buffer, sizeof(buffer));
                int get_file;
                if((get_file = SSL_read(_ssl, buffer, sizeof(buffer))) < 0){
                    fprintf(stderr, "Socket %d - Error receiving file name\n", socket);
                }
                if(_imLeader == true){
                    for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                        if((*_it2).second == 1) if(SSL_write((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
                }

                if(get_file >= 0 && _imLeader == true){
                    send_file(socket, userId, buffer);
                }
                break;
            case CP_SYNC_FILE_NOT_FOUND:
                //Arquivo não se encontra no client
                break;
            default: fprintf(stderr, "Socket %d - Unrecognized answer at sync_server\n", socket); break;
        }
    }
}

/*Recebe um arquivo do cliente*/
void DropboxServer::receive_file(int socket, char* userId, char* file){

    char buffer[CP_MAX_MSG_SIZE], filePath[512];
    int fileSize, iterations, sizeReceived, sizeToReceive, i;
    FILE *newFile;

    // Define o path do arquivo
    snprintf(filePath, sizeof(filePath), "%ssync_dir_%s/%s", SERVER_SYNC_DIR_PATH, userId, basename(file));

    // Cria o arquivo
    if(!(newFile = fopen(filePath, "w"))){
        fprintf(stderr, "Socket %d - Error creating file \'%s\'\n", socket, filePath);
        return;
    }

    // Recebe o tamanho do arquivo
    bzero(buffer, sizeof(buffer));
    if(SSL_read(_ssl, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "Socket %d - Error receiving file size\n", socket);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) if(SSL_write((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
    }

    fileSize = atoi(buffer);
    if(!sendInteger(_ssl, CP_CLIENT_SEND_FILE_SIZE_ACK)){
        fprintf(stderr, "Socket %d - Error sending file size ack\n", socket);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
    }

    // Recebe o arquivo
    sizeReceived = 0;
    iterations = (fileSize%CP_MAX_MSG_SIZE) > 0 ? fileSize/CP_MAX_MSG_SIZE + 1 : fileSize/CP_MAX_MSG_SIZE;
    for(i = 0; i < iterations; i++){

        sizeToReceive = (fileSize - sizeReceived) > CP_MAX_MSG_SIZE ? CP_MAX_MSG_SIZE : (fileSize - sizeReceived);
        bzero(buffer, sizeof(buffer));
        if(SSL_read(_ssl, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "Socket %d - Error receiving part of file %d\n", socket, i+1);
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) if(SSL_write((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
        }

        fwrite((void*) buffer, sizeToReceive, 1, newFile);
        if(!sendInteger(_ssl, CP_FILE_PART_RECEIVED)){
            fprintf(stderr, "Socket %d - Error sending ack for part %d of %d\n", socket, i+1, iterations);
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
        }
        sizeReceived += sizeToReceive;
    }
    fclose(newFile);
    //Recebe confirmação de término do envio do arquivo
    if(!receiveExpectedInt(_ssl, CP_SEND_FILE_COMPLETE)){
        fprintf(stderr, "Socket %d - Error receiving CP_SEND_FILE_COMPLETE\n", socket);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) sendInteger((*_it2).first, CP_SEND_FILE_COMPLETE);
    }

    if(!sendInteger(_ssl, CP_SEND_FILE_COMPLETE_ACK)){
        fprintf(stderr, "Socket %d - Error sending CP_SEND_FILE_COMPLETE_ACK\n", socket);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
    }

    //Recebe o M time do arquivo no cliente
    bzero(buffer, sizeof(buffer));
    if(SSL_read(_ssl, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "Socket %d - Error receiving file's M time\n", socket);
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) if(SSL_write((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
    }

    //Preenche as estruturas internas do servidor
    if(!assignNewFile(file, buffer, fileSize, userId)){
        fprintf(stderr, "Socket %d - Error assigning new file \'%s\'\n", socket, file);
    }

    fprintf(stderr, "Socket %d - File \'%s\' was received\n", socket, basename(file));
}

/*Envia um arquivo para o cliente*/
void DropboxServer::send_file(int socket, char* userId, char* filePath){

    char fserverPath[512];
    int fileSize, iterations, sizeSent, sizeToSend;
    char buffer[CP_MAX_MSG_SIZE];
    FILE *file;

    snprintf(fserverPath, sizeof(fserverPath), "%ssync_dir_%s/%s", SERVER_SYNC_DIR_PATH, userId, basename(filePath));

    // Abre o arquivo para leitura
    if(!(file = fopen(fserverPath, "r"))){
        // Erro na abertura
        fprintf(stderr, "DropboxServer - Error opening file \'%s\'\n", filePath);
    }
    // Envia confirmação da existência do arquivo
    if(!sendInteger(_ssl, CP_CLIENT_GET_FILE_EXISTS)){
        fprintf(stderr, "DropboxServer - Error confirming file existance\n");
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
    }

    if(file == NULL) return;

    // Descobre o tamanho do arquivo
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    // Envia o tamanho do arquivo
    if(!sendInteger(_ssl, fileSize)){
        fprintf(stderr, "DropboxServer - Error sending file size\n");
        fclose(file);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
    }

    if(!receiveExpectedInt(_ssl, CP_CLIENT_GET_FILE_SIZE_ACK)){
        fprintf(stderr, "DropboxServer - Error receiving size confirmation from client\n");
        fclose(file);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) sendInteger((*_it2).first, CP_CLIENT_GET_FILE_SIZE_ACK);
    }

    // Começa o envio do arquivo
    iterations = (fileSize%CP_MAX_MSG_SIZE) > 0 ? fileSize/CP_MAX_MSG_SIZE + 1 : fileSize/CP_MAX_MSG_SIZE;
    sizeSent = 0;
    for(int i = 0; i < iterations; i++){

        sizeToSend = (fileSize - sizeSent) > CP_MAX_MSG_SIZE ? CP_MAX_MSG_SIZE : (fileSize - sizeSent);
        bzero(buffer, sizeof(buffer));
        fread((void*) buffer, sizeToSend, 1, file);
        if(SSL_write(_ssl, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "DropboxServer - Error sending part %d of file\n", i);
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) SSL_read((*_it2).first, buffer, sizeof(buffer));
        }

        if(!receiveExpectedInt(_ssl, CP_FILE_PART_RECEIVED)){
            fprintf(stderr, "DropboxServer - Error receving ack for part %d of %d\n", i+1, iterations);
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) sendInteger((*_it2).first, CP_FILE_PART_RECEIVED);
        }
        sizeSent += sizeToSend;
    }
    fclose(file);

    // Envia mesagem informando fim do arquivo
    if(!sendInteger(_ssl, CP_SEND_FILE_COMPLETE)){
        fprintf(stderr, "DropboxServer - Error sending file send completion message\n");
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
    }

    // Recebe ack
    if(!receiveExpectedInt(_ssl, CP_SEND_FILE_COMPLETE_ACK)){
        fprintf(stderr, "DropboxServer - Error receiving ack for file completion\n");
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) sendInteger((*_it2).first, CP_SEND_FILE_COMPLETE_ACK);
    }

    fprintf(stderr, "Socket %d - File \'%s\' was sent\n", socket, basename(filePath));
}

//--------------------------------------------------FUNÇÕES EXTRAS

/*Cria a thread para atender a comunicação com um cliente, encapsula a chamad a pthread_create*/
pthread_t *DropboxServer::handleConnection(int* socket){

	  pthread_t comunicationThread;
    struct serverAndSocket* arg = (struct serverAndSocket*) malloc(sizeof(struct serverAndSocket));

    arg->socket = socket;
    arg->instance = this;
	  pthread_create(&comunicationThread, NULL, handleConnectionThread, arg);

    return &comunicationThread;
}

/*Thread que realiza a comunicação entre o servidor e o cliente*/
void* DropboxServer::handleConnectionThread(void* args){

    struct serverAndSocket arg = *(struct serverAndSocket*)args;
    int socket, returnVal;
    bool isRunning = true;
    char receiveBuffer[CP_MAX_MSG_SIZE], userId[MAXNAME];
    int numberOfAtempts = 0, maxAtempts = 5;

    DropboxServer *server = arg.instance;
    socket = *arg.socket;

    fprintf(stderr, "DropboxServer - Starting thread with comunication socket = %d\n", socket);

    // Recebe o userId do cliente
    bzero(receiveBuffer, sizeof(receiveBuffer));
    if(SSL_read(server->_ssl, receiveBuffer, sizeof(receiveBuffer)) <= 0){
        fprintf(stderr, "Socket %d - Error receiving userId\n", socket);
        server->closeConnection(socket);
        free(args);
        return NULL;
    }
    if(server->get_imleader() == true){
        for(server->resetIt2(); server->getIt2() != server->endIt2(); server->incrementIt2())
            if((server->getIt2())->second == 1) if(SSL_write((server->getIt2())->first, receiveBuffer, sizeof(receiveBuffer)) < 0) server->setIt22(0);
    }

    // Valida o userId recebido
    if(server->logInClient(socket, receiveBuffer)){
  		// Logou com sucesso
  	    fprintf(stderr, "Socket %d - User \"%s\" just logged in\n", socket, receiveBuffer);
  	    if(!sendInteger(server->_ssl, CP_LOGIN_SUCCESSFUL))
        {
            fprintf(stderr, "Socket %d - User \"%s\" error\n", socket, receiveBuffer);
            return NULL;
        }
        if(server->get_imleader() == true){
            for(server->resetIt2(); server->getIt2() != server->endIt2(); server->incrementIt2())
                if((server->getIt2())->second == 1) receiveExpectedInt((server->getIt2())->first, -1);
        }
        strncpy(userId, receiveBuffer, sizeof(userId));
    }

    else{
		// Falha no login
		    fprintf(stderr, "Socket %d - Error logging user %s in\n", socket, receiveBuffer);
        sendInteger(server->_ssl, CP_LOGIN_FAILED);
        if(server->get_imleader() == true){
          for(server->resetIt2(); server->getIt2() != server->endIt2(); server->incrementIt2())
                if((server->getIt2())->second == 1) receiveExpectedInt((server->getIt2())->first, -1);
        }

        server->closeConnection(socket);
        free(args);
        return NULL;
    }
    // Fica no aguardo de mensagens do cliente
    while(isRunning){
        // Recebe comando do cliente
        bzero(receiveBuffer, sizeof(receiveBuffer));

        returnVal = SSL_read(server->_ssl, receiveBuffer, sizeof(receiveBuffer));
        if(server->get_imleader() == true){
            for(server->resetIt2(); server->getIt2() != server->endIt2(); server->incrementIt2())
                if((server->getIt2())->second == 1) if(SSL_write((server->getIt2())->first, receiveBuffer, sizeof(receiveBuffer)) < 0) server->setIt22(0);
        }

        if(returnVal < 0){
            //server->logOutClient(socket, userId); // TODO: Ver se isso aqui nesse lugar dá certo
            fprintf(stderr, "Socket %d - Error receiving comand from client\n", socket);
            numberOfAtempts++;
            if(numberOfAtempts >= maxAtempts){
                // Atingiu o timeout
                server->logOutClient(socket, userId);
                server->closeConnection(socket);
            }
        }
        else if(returnVal == 0){
            server->logOutClient(socket, userId);
            server->closeConnection(socket);
        }
        else{
            // Mensagem recebida com sucesso, reinicia timeout
            numberOfAtempts = 0;
            switch(atoi(receiveBuffer)){
                case CP_CLIENT_END_CONNECTION:
                    server->logOutClient(socket, userId);
                    server->closeConnection(socket);
                    break;
                case CP_CLIENT_SEND_FILE:
                    if(!sendInteger(server->_ssl, CP_CLIENT_SEND_FILE_ACK)){
                        fprintf(stderr, "Socket %d - Error sending CP_CLIENT_SEND_FILE_ACK\n", socket);
                        continue;
                    }
                    if(server->get_imleader() == true){
                        for(server->resetIt2(); server->getIt2() != server->endIt2(); server->incrementIt2())
                            if((server->getIt2())->second == 1) receiveExpectedInt((server->getIt2())->first, CP_CLIENT_SEND_FILE_ACK);
                    }

                    //Recebe o nome do arquivo
                    bzero(receiveBuffer, sizeof(receiveBuffer));
                    int recv_file;
                    if((recv_file = SSL_read(server->_ssl, receiveBuffer, sizeof(receiveBuffer))) < 0){
                        fprintf(stderr, "Socket %d - Error receiving file name\n", socket);
                    }
                    if(server->get_imleader() == true){
                        for(server->resetIt2(); server->getIt2() != server->endIt2(); server->incrementIt2())
                            if((server->getIt2())->second == 1) if(SSL_write((server->getIt2())->first, receiveBuffer, sizeof(receiveBuffer)) < 0) server->setIt22(0);
                    }
                    if(recv_file >= 0){
                        server->receive_file(socket, userId, receiveBuffer);
                    }
                    break;
                case CP_CLIENT_GET_FILE:
                    if(!sendInteger(server->_ssl, CP_CLIENT_GET_FILE_ACK)){
                        fprintf(stderr, "Socket %d - Error sending CP_CLIENT_GET_FILE_ACK\n", socket);
                        continue;
                    }
                    if(server->get_imleader() == true){
                        for(server->resetIt2(); server->getIt2() != server->endIt2(); server->incrementIt2())
                            if((server->getIt2())->second == 1) receiveExpectedInt((server->getIt2())->first, CP_CLIENT_GET_FILE_ACK);
                    }
                    //Recebe o nome do arquivo
                    bzero(receiveBuffer, sizeof(receiveBuffer));
                    int get_file;
                    if((get_file = SSL_read(server->_ssl, receiveBuffer, sizeof(receiveBuffer))) < 0){
                        fprintf(stderr, "Socket %d - Error receiving file name\n", socket);
                    }
                    if(server->get_imleader() == true){
                        for(server->resetIt2(); server->getIt2() != server->endIt2(); server->incrementIt2())
                            if((server->getIt2())->second == 1) if(SSL_write((server->getIt2())->first, receiveBuffer, sizeof(receiveBuffer)) < 0) server->setIt22(0);
                    }
                    if(get_file >= 0){
                        server->send_file(socket, userId, receiveBuffer);
                    }
                    break;
                case CP_SYNC_CLIENT:
                    server->sync_server(socket, userId);
                    break;
                case CP_CLIENT_DELETE_FILE:
                    server->deleteFile(socket, userId);
                    break;
                case CP_LIST_SERVER:
                    server->listServer(socket, userId);
                    break;
                case CP_CLIENT_GET_FILE_LOCK:
                    server->lockFile(socket, userId);
                    break;
                case CP_CLIENT_GET_FILE_UNLOCK:
                    server->unlockFile(socket, userId);
                    break;
                case CP_FIND_LEADER:
                    //server->findLeader(socket, userId);
                    break;
                case 0:
                    fprintf(stderr, "Connection with Client not available. Trying to connect %d\n", atoi(receiveBuffer));
                    sendInteger(server->_ssl, CP_LOGIN_FAILED);
                    if(server->get_imleader() == true){
                        for(server->resetIt2(); server->getIt2() != server->endIt2(); server->incrementIt2())
                            if((server->getIt2())->second == 1) receiveExpectedInt((server->getIt2())->first, -1);
                    }
                    server->closeConnection(socket);
                    free(args);
                    return NULL;
                default:
                    fprintf(stderr, "Socket %d - Unrecognized message %d\n", socket, atoi(receiveBuffer));
                    break;
            }
        }
    }
    free(args);
    return NULL;
}

/*Atribui um novo arquivo a um usuário*/
bool DropboxServer::assignNewFile(char* fileName, char* fileMTime, int fileSize, char* userId){

    uint j;
    bool done = false;
    int userIndex, fileIndex;

    pthread_mutex_lock(&_clientStructMutex);

    userIndex = findUserIndex(userId);
    fileIndex = findUserFile(userId, fileName);

    if(userIndex < 0){
        pthread_mutex_unlock(&_clientStructMutex);
        return false;
    }

    if(fileIndex < 0){
        //Arquivo novo
        j = 0;
        while(!done && j < MAXFILES){
            //Encontra o primeiro espaço vazio em file_info[]
            if(strlen(_clients.at(userIndex).file_info[j].name) == 0){
                //Encontrou um espaço vazio
                fileIndex = j;
                done = true;
            }
            j++;
        }
    }

    if(fileIndex >= 0){
        //Encontrou algum espaço vazio
        strncpy(_clients.at(userIndex).file_info[fileIndex].name, basename(fileName), sizeof(_clients.at(userIndex).file_info[fileIndex].name));
        strncpy(_clients.at(userIndex).file_info[fileIndex].extension, getFileExtension(basename(fileName)),
        sizeof(_clients.at(userIndex).file_info[fileIndex].extension));
        strncpy(_clients.at(userIndex).file_info[fileIndex].last_modified, fileMTime,
        sizeof(_clients.at(userIndex).file_info[fileIndex].last_modified));
        _clients.at(userIndex).file_info[fileIndex].size = fileSize;
        pthread_mutex_unlock(&_clientStructMutex);
        return true;
    }
    else{
        //Não encontrou nenhum espaço vazio
        fprintf(stderr, "DropboxServer - Couldn't find a space for file \'%s\'\n", fileName);
        pthread_mutex_unlock(&_clientStructMutex);
        return false;
    }
}

/*Adiciona um usuário a estrutura de clientes do servidor*/
bool DropboxServer::logInClient(int socket, char* userId){

    uint i;
    bool foundUser = false;
    CLIENT newClient;

    pthread_mutex_lock(&_clientStructMutex);
    //Verifica se usuário não atingiu o limite de conexões
    for (i = 0; i < _clients.size(); i++) {
        if(strcmp(userId, _clients.at(i).userId) == 0){
            foundUser = true;
            if(_clients.at(i).devices[0] > 0 && _clients.at(i).devices[1] > 0){
                printf("Socket %d - User already has 2 connections\n", socket);
                pthread_mutex_unlock(&_clientStructMutex);
                return false;
            }
            break;
        }
    }
    if(foundUser){
        //Usuário velho
        _clients.at(i).logged_in = 1;
        if(_clients.at(i).devices[0] < 0){
            _clients.at(i).devices[0] = socket;
        }
        else if(_clients.at(i).devices[1] < 0){
            _clients.at(i).devices[1] = socket;
        }
        else{
            printf("Socket %d - Internal error, one position should be empty\n", socket);
            pthread_mutex_unlock(&_clientStructMutex);
            return false;
        }
    }
    else{
        //Usuário novo
        newClient.devices[0] = socket;
        newClient.devices[1] = -1;
        newClient.logged_in = 1;
        strncpy(newClient.userId, userId, sizeof(newClient.userId));
        for(i = 0; i < MAXFILES; i++){
            bzero(newClient.file_info[i].name, sizeof(newClient.file_info[i].name));
        }
        _clients.push_back(newClient);
    }
    pthread_mutex_unlock(&_clientStructMutex);
    return true;
}

/*Remove um usuário da estrutura de clientes do servidor*/
void DropboxServer::logOutClient(int socket, char* userId){

    uint i;
    int userIndex;
    int j, k;
    char curFilePath[256];

    pthread_mutex_lock(&_clientStructMutex);
    //Remove os locks que o usuário tinha em arquivos
    userIndex = findUserIndex(userId);
    _clients.at(userIndex);
    for(j = 0; j < MAXFILES; j++){
        if(strlen(_clients.at(userIndex).file_info[j].name) > 0){
            // Arquivo existe
            for(k = 0; k < _clients.at(userIndex).file_info[j].lock.size(); k++){
                // Percorre a lista de locks do arquivo
                if(_clients.at(userIndex).file_info[j].lock.at(k) == socket){
                    // O cliente tinha lock no arquivo
                    _clients.at(userIndex).file_info[j].lock.erase(_clients.at(userIndex).file_info[j].lock.begin() + k);
                }
            }
        }
    }
    //Desloga o cliente
    for (i = 0; i < _clients.size(); i++) {
        if(strcmp(userId, _clients.at(i).userId) == 0){
            if(_clients.at(i).devices[0] == socket){
                _clients.at(i).devices[0] = -1;
            }
            else if(_clients.at(i).devices[1] == socket){
                _clients.at(i).devices[1] = -1;
            }
            else{
                printf("Internal error on logout\n");
            }
            pthread_mutex_unlock(&_clientStructMutex);
            return;
        }
    }
    pthread_mutex_unlock(&_clientStructMutex);
    printf("Internal error 2 on logout\n");
}

/* Conta o número de arquivos de um determinado usuário */
int DropboxServer::countUserFiles(char* userId){

    int i, userIndex, fileCount = 0;

    userIndex = findUserIndex(userId);
    if(userIndex >= 0){
        //Encontrou o usuário no vetor
        for(i = 0; i < MAXFILES; i++){
            //Conta o número de arquivos
            if(strlen(_clients.at(userIndex).file_info[i].name) > 0){
                fileCount++;
            }
        }
        return fileCount;
    }
    //Não encontrou o usuário no vetor
    return -1;
}

/* Encontra o índice correspondente a userId no vetor de clientes */
int DropboxServer::findUserIndex(const char* userId){

    uint i;

    for(i = 0; i < _clients.size(); i++){
        if(strcmp(_clients.at(i).userId, userId) == 0){
            //Encontrou o userId
            return i;
        }
    }
    //Não encontrou o userId
    return -1;
}

/* Recupera as estruturas internas do servidor com base nos diretórios locais */
void DropboxServer::recoverData(){

    DIR *serverSyncDir, *curSyncDir;
    struct dirent *entry, *curEntry;
    char curSyncDirPath[512], curFilePath[512];
    CLIENT clientBuffer;
    int i, fileCounter;

    if((serverSyncDir = opendir(SERVER_SYNC_DIR_PATH)) != NULL){
        //Abre o diretório em que estão todos os sync_dir_
            while((entry = readdir(serverSyncDir)) != NULL){
                // Para cada arquivo no diretório
                if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
                    //Ignora os arquivos . e ..
                    continue;
                }

                // Para todos os arquivos úteis
                snprintf(curSyncDirPath, sizeof(curSyncDirPath), "%s%s", SERVER_SYNC_DIR_PATH, entry->d_name);
                if((curSyncDir = opendir(curSyncDirPath)) != NULL){
                    //Entrou em algum sync_dir
                    if(sscanf(entry->d_name, "sync_dir_%s", clientBuffer.userId) > 0){
                        //O diretório encontrado é um sync_dir
                        //Inicializa as variáveis
                        clientBuffer.devices[0] = -1;
                        clientBuffer.devices[1] = -1;
                        clientBuffer.logged_in = 0;
                        for(i = 0; i < MAXFILES; i++){
                            bzero(clientBuffer.file_info[i].name, sizeof(clientBuffer.file_info[i].name));
                        }

                        //Recolhe dados sobre os arquivos
                        fileCounter = 0;
                        while((curEntry = readdir(curSyncDir)) != NULL){
                            if(strcmp(curEntry->d_name, ".") == 0 || strcmp(curEntry->d_name, "..") == 0){
                                //Ignora os arquivos . e ..
                                continue;
                            }

                            //Salva os dados dos arquivos úteis
                            snprintf(curFilePath, sizeof(curFilePath), "%s/%s", curSyncDirPath, curEntry->d_name);
                            getFileInfo(curFilePath, &clientBuffer.file_info[fileCounter]);
                            fileCounter++;
                        }
                        pthread_mutex_lock(&_clientStructMutex);
                        _clients.push_back(clientBuffer);
                        pthread_mutex_unlock(&_clientStructMutex);
                    }
                    closedir(curSyncDir);
                }
            }
            closedir(serverSyncDir);
        }
}

/* Retorna o índice para o arquivo em file_info se existente ou -1 caso contrário */
int DropboxServer::findUserFile(char* userId, char* fileName){

    int userIndex;
    uint i;

    userIndex = findUserIndex(userId);

    if(userIndex < 0){
        return -1;
    }

    for(i = 0; i < MAXFILES; i++){
        //Para cada arquivo do usuário
        if(strcmp(_clients.at(userIndex).file_info[i].name, fileName) == 0){
            return i;
        }
    }
    //Não encontrou o arquivo
    return -1;
}

/* Deleta um arquivo do servidor */
void DropboxServer::deleteFile(int socket, char* userId){

    char buffer[CP_MAX_MSG_SIZE], filePath[256];
    int userIndex, fileIndex, i;

    //Envia ack
    if(!sendInteger(_ssl, CP_CLIENT_DELETE_FILE_ACK)){
        fprintf(stderr, "Socket %d - Error sending CP_CLIENT_DELETE_FILE_ACK\n", socket);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
    }

    //Recebe o nome do arquivo a ser deletado
    bzero(buffer, sizeof(buffer));
    if(SSL_read(_ssl, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "Socket %d - Error receiving file name\n", socket);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) if(SSL_write((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
    }
    snprintf(filePath, sizeof(filePath), "%ssync_dir_%s/%s", SERVER_SYNC_DIR_PATH, userId, buffer);

    //Remove o arquivo
    if(remove(filePath) < 0){
        fprintf(stderr, "Socket %d - Error deleting file \'%s\'\n", socket, filePath);
        return;
    }

    //Remove o arquivo das estruturas internas do servidor
    //A consistência deve ser mantida, isto é, não podem haver registros vazios no meio do array
    pthread_mutex_lock(&_clientStructMutex);
    userIndex = findUserIndex(userId);
    fileIndex = findUserFile(userId, basename(filePath));

    if(fileIndex < 0){
        fprintf(stderr, "Socket %d - Internal error, file \'%s\' not found\n", socket, basename(filePath));
        pthread_mutex_unlock(&_clientStructMutex);
        return;
    }


    i = 0;
    while(i < MAXFILES){
        if(strlen(_clients.at(userIndex).file_info[i].name) == 0){
            //Encontrou o primeiro elemento vazio
            if(i == 0){
                //O array está vazio
                fprintf(stderr, "Socket %d - Internal error, array is empty\n", socket);
            }
            else if(i-1 == 0){
                //O array contém apenas um elemento
                bzero(_clients.at(userIndex).file_info[i-1].name, sizeof(_clients.at(userIndex).file_info[i-1].name));
            }
            else if(fileIndex < i-1){
                //FileIndex se encontra antes do último elemento
                memcpy((void*) &_clients.at(userIndex).file_info[fileIndex], (void*) &_clients.at(userIndex).file_info[i-1], sizeof(FILE_INFO));
                bzero(_clients.at(userIndex).file_info[i-1].name, sizeof(_clients.at(userIndex).file_info[i-1].name));
            }
            else if(fileIndex == i-1){
                //FileIndex é o último elemento do array
                bzero(_clients.at(userIndex).file_info[i-1].name, sizeof(_clients.at(userIndex).file_info[i-1].name));
            }
            break;
        }
        i++;
    }
    pthread_mutex_unlock(&_clientStructMutex);
    fprintf(stderr, "Socket %d - File \'%s\' was deleted\n", socket, basename(filePath));
}

/* Manda todos os dados usados no comand list_server para o cliente */
void DropboxServer::listServer(int socket, char* userId){

    int numberOfFiles, i, curFileSize;
    char buffer[CP_MAX_MSG_SIZE], syncDirPath[256];
    char curATime[50], curMTime[50], curCTime[50], curFilePath[512];
    DIR *localSyncDir;
    struct dirent *entry;
    struct stat statBuff;

    //Envia um ack
    if(!sendInteger(_ssl, CP_LIST_SERVER_ACK)){
        fprintf(stderr, "Socket %d - Error sending CP_LIST_SERVER_ACK\n", socket);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
    }

    //Envia o número de arquivos
    numberOfFiles = countUserFiles(userId);
    if(!sendInteger(_ssl, numberOfFiles)){
        fprintf(stderr, "Socket %d - Error sending number of files\n", socket);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
    }

    //Abre o diretório do sync dir
    snprintf(syncDirPath, sizeof(syncDirPath), "%ssync_dir_%s", SERVER_SYNC_DIR_PATH, userId);
    if((localSyncDir = opendir(syncDirPath)) == NULL){
        fprintf(stderr, "Socket %d - Error opening directory \'%s\'\n", socket ,syncDirPath);
        return;
    }

    for(i = 0; i < numberOfFiles; i++){

        if((entry = readdir(localSyncDir)) == NULL){
            fprintf(stderr, "Socket %d - Error getting file entry\n", socket);
        }
        else{
            // Para cada arquivo no diretório
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
                if((entry = readdir(localSyncDir)) == NULL){
                    fprintf(stderr, "Socket %d - Error getting file entry\n", socket);
                }
            }

            // Para todos os arquivos úteis
            snprintf(curFilePath, sizeof(curFilePath), "%s/%s", syncDirPath, entry->d_name);
            stat(curFilePath, &statBuff);
            strftime(curMTime, sizeof(curMTime), "%Y-%m-%d %H:%M:%S", localtime(&statBuff.st_mtime));
            strftime(curATime, sizeof(curATime), "%Y-%m-%d %H:%M:%S", localtime(&statBuff.st_atime));
            strftime(curCTime, sizeof(curCTime), "%Y-%m-%d %H:%M:%S", localtime(&statBuff.st_ctime));
            getFileSize(curFilePath, &curFileSize);

            //Envia o nome do arquivo
            bzero(buffer, sizeof(buffer));
            strncpy(buffer, entry->d_name, sizeof(buffer));
            if(SSL_write(_ssl, buffer, sizeof(buffer)) < 0){
                fprintf(stderr, "Socket %d - Error sending file name\n", socket);
            }
            if(_imLeader == true){
                for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                    if((*_it2).second == 1) if(SSL_read((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
            }

            // Recebe ack pelo nome do arquivo
            if(!receiveExpectedInt(_ssl, CP_LIST_SERVER_FILENAME_ACK)){
                fprintf(stderr, "Socket %d - Error getting ack for file name \'%s\'\n", socket, entry->d_name);
            }
            if(_imLeader == true){
                for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                    if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
            }

            //Envia tamanho, ATime, MTime e CTime na mesma string
            bzero(buffer, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "%d|%s|%s|%s", curFileSize, curATime, curMTime, curCTime);
            if(SSL_write(_ssl, buffer, sizeof(buffer)) < 0){
                fprintf(stderr, "Socket %d - Error file \'%s\' information\n", socket, entry->d_name);
            }
            if(_imLeader == true){
                for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                    if((*_it2).second == 1) if(SSL_read((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
            }

            //Espera pela confirmação do cliente
            if(!receiveExpectedInt(_ssl, CP_LIST_SERVER_FILE_OK)){
                fprintf(stderr, "Socket %d - Error getting ack for file \'%s\'\n", socket, entry->d_name);
            }
            if(_imLeader == true){
                for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                    if((*_it2).second == 1) sendInteger((*_it2).first, CP_LIST_SERVER_FILE_OK);
            }
        }
    }
    closedir(localSyncDir);
}

/* Faz a exclusão mútua distrinuída para um arquivo */
void DropboxServer::lockFile(int socket, char* userId){

    char buffer[CP_MAX_MSG_SIZE];
    int userIndex, fileIndex, socketIndex;
    uint i;

    //Envia o ack para o pedido
    if(!sendInteger(_ssl, CP_CLIENT_GET_FILE_LOCK_ACK)){
        fprintf(stderr, "DropboxClient - Error sending CP_CLIENT_GET_FILE_LOCK_ACK\n");
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
    }

    //Recebe o nome do arquivo
    bzero(buffer, sizeof(buffer));
    if(SSL_read(_ssl, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "Socket %d - Error receiving name of file to be locked\n", socket);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) if(SSL_write((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
    }

    //Executa o algoritmo do token
    pthread_mutex_lock(&_clientStructMutex);
    userIndex = findUserIndex(userId);
    fileIndex = findUserFile(userId, basename(buffer));
    //Verifica se os índices encontrados são válidos
    if(userIndex < 0 || fileIndex < 0){
        fprintf(stderr, "Socket %d - Error finding user \'%s\' or file \'%s\' at lockFile()\n", socket, userId, basename(buffer));
        pthread_mutex_unlock(&_clientStructMutex);
        //Envia mensagem de falha ao cliente
        if(!sendInteger(_ssl, 0)){
            fprintf(stderr, "DropboxClient - Error sending lock list size\n");
            return;
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
        }
        return;
    }
    //Verifica se o cliente está na lista de locks
    socketIndex = -1;
    for(i = 0; i < _clients.at(userIndex).file_info[fileIndex].lock.size(); i++){
        if(_clients.at(userIndex).file_info[fileIndex].lock.at(i) == socket){
            socketIndex = i;
        }
    }
    //Verifica se o cliente deve ser adicionado à lista
    if(socketIndex < 0){
        //O cliente não se encontra na lista, deve ser inserido
        _clients.at(userIndex).file_info[fileIndex].lock.push_back(socket);
    }
    //Imprime na tela a lista de espera para o arquivo
    fprintf(stderr, "\n");
    fprintf(stderr, "Lock for file %s:", _clients.at(userIndex).file_info[fileIndex].name);
    for(i = 0; i < _clients.at(userIndex).file_info[fileIndex].lock.size(); i++){
        fprintf(stderr, " %d ->", _clients.at(userIndex).file_info[fileIndex].lock.at(i));
    }
    fprintf(stderr, "\n");
    //Verifica a posição do cliente na lista
    if(_clients.at(userIndex).file_info[fileIndex].lock.at(0) == socket){
        //O cliente atual é o primeiro da lista, envia mensagem de sucesso
        if(!sendInteger(_ssl, CP_CLIENT_GET_FILE_LOCK_SUCCESS)){
            fprintf(stderr, "DropboxClient - Error sending CP_CLIENT_GET_FILE_LOCK_SUCCESS\n");
            pthread_mutex_unlock(&_clientStructMutex);
            return;
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
        }
    }
    else{
        //O cliente atual deve esperar o lock ser liberado, envia o tamanho da lista
        if(!sendInteger(_ssl, _clients.at(userIndex).file_info[fileIndex].lock.size())){
            fprintf(stderr, "DropboxClient - Error sending lock list size\n");
            pthread_mutex_unlock(&_clientStructMutex);
            return;
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
        }

    }
    pthread_mutex_unlock(&_clientStructMutex);
}

/* Libera um arquivo da exclusão mútua distribuída */
void DropboxServer::unlockFile(int socket, char* userId){

    uint i;
    int userIndex, fileIndex, socketIndex;
    char buffer[CP_MAX_MSG_SIZE];

    //Envia o ack para o pedido
    if(!sendInteger(_ssl, CP_CLIENT_GET_FILE_UNLOCK_ACK)){
        fprintf(stderr, "DropboxClient - Error sending CP_CLIENT_GET_FILE_UNLOCK_ACK\n");
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
    }

    //Recebe o nome do arquivo
    bzero(buffer, sizeof(buffer));
    if(SSL_read(_ssl, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "Socket %d - Error receiving name of file to be unlocked\n", socket);
        return;
    }
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            if((*_it2).second == 1) if(SSL_write((*_it2).first, buffer, sizeof(buffer)) < 0) (*_it2).second = 0;
    }

    //Procura pelo arquivo nas estruturas do servidor
    pthread_mutex_lock(&_clientStructMutex);
    userIndex = findUserIndex(userId);
    fileIndex = findUserFile(userId, basename(buffer));
    //Verifica se os índices encontrados são válidos
    if(userIndex < 0 || fileIndex < 0){
        fprintf(stderr, "Socket %d - Error finding user \'%s\' or file \'%s\' at unlockFile\n", socket, userId, basename(buffer));
        //Envia mensagem de falha ao cliente
        pthread_mutex_unlock(&_clientStructMutex);
        if(!sendInteger(_ssl, 0)){
            fprintf(stderr, "DropboxClient - Error sending lock list size\n");
            return;
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
        }
        return;
    }
    //Remove o lock do arquivo
    socketIndex = -1;
    for(i = 0; i < _clients.at(userIndex).file_info[fileIndex].lock.size(); i++){
        if(_clients.at(userIndex).file_info[fileIndex].lock.at(i) == socket){
            socketIndex = i;
        }
    }
    if(socketIndex >= 0){
        _clients.at(userIndex).file_info[fileIndex].lock.erase(_clients.at(userIndex).file_info[fileIndex].lock.begin() + socketIndex);
        fprintf(stderr, "Socket %d - Lock removed successfully from file \'%s\'\n", socket, basename(buffer));
        //Envia mensagem de confirmação da remoção do lock
        if(!sendInteger(_ssl, CP_CLIENT_GET_FILE_UNLOCK_SUCCESS)){
            fprintf(stderr, "DropboxClient - Error sending CP_CLIENT_GET_FILE_UNLOCK_SUCCESS\n");
            pthread_mutex_unlock(&_clientStructMutex);
            return;
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
        }
    }
    else{
        //Erro na busca do lock
        if(!sendInteger(_ssl, CP_CLIENT_GET_FILE_UNLOCK_FAIL)){
            fprintf(stderr, "DropboxClient - Error sending CP_CLIENT_GET_FILE_UNLOCK_FAIL\n");
            pthread_mutex_unlock(&_clientStructMutex);
            return;
        }
        if(_imLeader == true){
            for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
                if((*_it2).second == 1) receiveExpectedInt((*_it2).first, -1);
        }
    }
    //Imprime na tela a lista de espera para o arquivo
    fprintf(stderr, "\n");
    fprintf(stderr, "Lock for file %s:", _clients.at(userIndex).file_info[fileIndex].name);
    for(i = 0; i < _clients.at(userIndex).file_info[fileIndex].lock.size(); i++){
        fprintf(stderr, " %d ->", _clients.at(userIndex).file_info[fileIndex].lock.at(i));
    }
    fprintf(stderr, "\n");
    pthread_mutex_unlock(&_clientStructMutex);
}

/*Faz listen(), accept() e retorna o socket de comunicação*/
int DropboxServer::listenAndAccept(){

    struct sockaddr_in clientAddress;
    socklen_t clientLength;
    int newSocket;

    clientLength = sizeof(struct sockaddr_in);
    listen(_serverSocket, SERVER_BACKLOG);
    newSocket = accept(_serverSocket, (struct sockaddr*) &clientAddress, &clientLength);
    if(newSocket == -1){
        printf("DropboxServer - Error accepting connection\n");
    }

   //Amarra Socket com SSL
    _ssl = SSL_new(ctx);
    SSL_set_fd(_ssl,newSocket);
    int ssl_err = SSL_accept(_ssl);
    if(ssl_err < 0){
        //Erro aconteceu, fecha o SSL
        printf("[main] Erro com SSL\n");
        exit(1);
    }
    return newSocket;
}

/*Faz listen(), accept() e retorna o socket de comunicação*/
int DropboxServer::justListen(){

    struct sockaddr_in clientAddress;
    socklen_t clientLength;
    int value;

    clientLength = sizeof(struct sockaddr_in);
    if(_imLeader == true){
        for(_it2 = _sockets_list.begin(); _it2 != _sockets_list.end(); _it2++)
            fprintf(stderr, ""); //if(_it2->second == 1) fprintf(stderr, "Valor: %d\n", listen(_it2->first, SERVER_BACKLOG));
    }
}

/*Faz o socket() e bind() do servidor, retorna -1 ou o valor do socket*/
int DropboxServer::initialize(int port){

    struct sockaddr_in serverAddress;
    const SSL_METHOD *method;

    //Inicializa SSL
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();

    method = SSLv23_server_method();
    ctx = SSL_CTX_new(method);
    if (ctx == NULL){
      ERR_print_errors_fp(stderr);
      abort();
    }

    //Carrega certificados
    SSL_CTX_use_certificate_file(ctx, "CertFile.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "KeyFile.pem", SSL_FILETYPE_PEM);

    //Inicializa socket (caso não consiga, interrompe execução e retorna erro)
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(_serverSocket == -1){
        printf("DropboxServer - Error initializing socket\n");
        return -1;
    }

    //Inicializa struct do socket
    serverAddress.sin_family = AF_INET; // communication domain do socket criado
    serverAddress.sin_port = htons(port); // porta do socket
    serverAddress.sin_addr.s_addr = INADDR_ANY; // container genérico
    bzero(&(serverAddress.sin_zero), 8); // completa os 16 bits de serverAddress com 8 0's (trabalha-se com 16 bits, mas
                                         // sin_family tem 2 bits, sin_port tem 2 e sin_addr tem 4)
    //Faz o bind (atribui identidade ao socket)
    if(bind(_serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){
        printf("DropboxServer - Error binding server :: Port %d\n", port);
        return -1;
    }

    _my_hostport.second = port;
    // Adquire IP
    FILE * file_handler= popen("ifconfig", "r");
    std::vector<std::string> ips;
    char *host_name = '\0';
    char *aux;
    size_t nr;
    while(getline(&host_name, &nr, file_handler) > 0 && host_name) {
        if (host_name = strstr(host_name, "inet ")){
            host_name+=5;
            if (host_name = strchr(host_name, ':')) {
                ++host_name;
                if (aux = strchr(host_name, ' ')) {
                    *aux='\0';
                    ips.push_back(std::string(host_name));
                }
            }
        }
    }
    _my_hostport.first = ips[0];
    pclose(file_handler);

    getHostPort();
    get_serverListPosition();
    //Recupera as estruturas do servidor
    recoverData();

    return _serverSocket;
}

/*Fecha a conexão com um cliente*/
void DropboxServer::closeConnection(int socket){

    printf("DropboxServer - Closing connection with socket %d\n", socket);
    close(socket);
    pthread_exit(NULL);
}

int DropboxServer::getSocket(){ return _serverSocket; }

SSL *DropboxServer::connect_server(char* host, int port){

    struct hostent *server;
    struct sockaddr_in serverAddress;
    int sock;
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    // Inicializa o SSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method = SSLv23_client_method();

    ctx = SSL_CTX_new(method);
    if (ctx == NULL){
      ERR_print_errors_fp(stderr);
      abort();
    }

    // tenta adquirir IP address (caso não consiga, interrompe execução e retorna erro)
    server = gethostbyname(host);
    if(server == NULL){
        fprintf(stderr, "DropboxServer - Server \'%s\' doesn't exist\n", host);
        return NULL;
    }

    //Inicializa socket (caso não consiga, interrompe execução e retorna erro)
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1){
        fprintf(stderr, "DropboxServer - Error creating socket\n");
        return NULL;
    }
    //Inicializa struct do socket
    serverAddress.sin_family = AF_INET; // communication domain do socket criado
    serverAddress.sin_port = htons(port); // porta do socket
    serverAddress.sin_addr = *((struct in_addr*) server->h_addr); // endereço IP para o socket
    bzero(&(serverAddress.sin_zero), 8); // completa os 16 bits de serverAddress com 8 0's (trabalha-se com 16 bits, mas
                                         // sin_family tem 2 bits, sin_port tem 2 e sin_addr tem 4)
    // tente conectar ao servidor (caso não consiga, interrompe execução e retorna erro)
    if(connect(sock, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr, "DropboxServer - Couldn't connect to server at %s on port %d\n", inet_ntoa(serverAddress.sin_addr), ntohs(serverAddress.sin_port));
        close(sock);
        return NULL;
    }
    else
        fprintf(stderr, "DropboxServer - Connected to server at %s on port %d\n", inet_ntoa(serverAddress.sin_addr), ntohs(serverAddress.sin_port));

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl,sock);
    if(SSL_connect(ssl) == -1){
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    X509 *cert;
    char *line;
    cert = SSL_get_peer_certificate(ssl);
    if (cert != NULL){
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("\nSubject: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n\n", line);
    }

    return ssl;
}

void DropboxServer::connectToServers(){
    int i = 1;
    if(_imLeader == true){ // se é o líder
        for(_it1 = (_server_list).begin(); _it1 != (_server_list).end(); (_it1)++)
        {
            if(get_imleader() != i){
                char *host = (char *) ((*_it1).first).c_str();
                int port = ((*_it1).second);
                SSL *sock = connect_server(host, port);
                if(sock != NULL){
                    _sockets_list.push_back(std::make_pair(sock, 1)); // Conectou ao Socket
                }
                else{
                    _sockets_list.push_back(std::make_pair(sock, 0)); // Não conseguiu Conectar ao Socket
                }
            }
        i++;
        _leaderSocket = -1; // não precisa de socket de comunicação com sí mesmo
        }
    }
}

int DropboxServer::get_serverListPosition(){

    int i = 1;
    for(_it1 = (_server_list).begin(); _it1 != (_server_list).end(); (_it1)++)
    {
        if(_my_hostport.first == (*_it1).first && _my_hostport.second == (*_it1).second)
        {
            _myPosition = i;
            fprintf(stderr, "DropboxServer - Server Position: %d\n", _myPosition);
        }
        if(_myPosition == 1) _imLeader = true;
        if(_imLeader == true) fprintf(stderr, "DropboxServer - 'Hey!, I'm the leader!'\n");
        i++;
    }
    return _myPosition;
}

std::list<std::pair<SSL *,int> >::iterator DropboxServer::getIt2(){
    return _it2;
}

void DropboxServer::setIt22(int val){
    (*_it2).second = val;
}

void DropboxServer::incrementIt2(){
    _it2++;
}
void DropboxServer::resetIt2(){
    _it2 = _sockets_list.begin();
}
std::list<std::pair<SSL *,int> >::iterator DropboxServer::endIt2(){
    return _sockets_list.end();
}

int DropboxServer::get_myposition(){
    return _myPosition;
}

int DropboxServer::get_imleader(){
    return _imLeader;
}

// adquire lista de servidores
std::pair<std::string, int> DropboxServer::getHostPort(){
    //_my_hostport;
    fprintf(stderr, "Server Connected: Host = %s; Port = %d.\n", (_my_hostport.first).c_str(), _my_hostport.second);
    return _my_hostport;
}
