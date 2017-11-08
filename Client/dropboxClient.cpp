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
#include <sys/stat.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <string>
#include <time.h>
#include "dropboxClient.h"
#include "../Util/dropboxUtil.h"

/*Constructor*/
DropboxClient::DropboxClient(){
    _isConnected = false;
}

//------------------------------------------------FUNÇÕES DEFINIDAS NA ESPECIFICAÇÃO

/*Estabelece a conexão entre host (endereço servidor) e port (porta da conexão)*/
int DropboxClient::connect_server(char* host, int port){

    struct hostent *server;
    struct sockaddr_in serverAddress;

    // tenta adquirir IP address (caso não consiga, interrompe execução e retorna erro)
    server = gethostbyname(host);
    if(server == NULL){
        fprintf(stderr, "DropboxClient - Server \'%s\' doesn't exist\n", host);
        return -1;
    }

    //Inicializa socket (caso não consiga, interrompe execução e retorna erro)
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if(_socket == -1){
        fprintf(stderr, "DropboxClient - Error creating socket\n");
        return -1;
    }
    //Inicializa struct do socket
    serverAddress.sin_family = AF_INET; // communication domain do socket criado
    serverAddress.sin_port = htons(port); // porta do socket
    serverAddress.sin_addr = *((struct in_addr*) server->h_addr); // endereço IP para o socket
    bzero(&(serverAddress.sin_zero), 8); // completa os 16 bits de serverAddress com 8 0's (trabalha-se com 16 bits, mas
                                         // sin_family tem 2 bits, sin_port tem 2 e sin_addr tem 4)
    // tente conectar ao servidor (caso não consiga, interrompe execução e retorna erro)
    if(connect(_socket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr, "DropboxClient - Couldn't connect to server at %s on port %d\n", inet_ntoa(serverAddress.sin_addr), ntohs(serverAddress.sin_port));
        close(_socket);
        return -1;
    }
    _isConnected = true;
    return _socket;
}

/* Sincroniza os arquivos entre cliente e servidor */
void DropboxClient::sync_client(){

    int numberOfFiles, i;
    char curFileName[CP_MAX_MSG_SIZE], curFilePath[512], syncDirPath[256], buffer[CP_MAX_MSG_SIZE];
    double diffTimeValue;
    bool syncedFile;
    struct dirent *entry;
    time_t serverFileMTime, localFileMTime;
    std::vector<std::string> serverFiles;
    DIR *localSyncDir;

    //Envia uma solicitação para o servidor e espera o número de arquivos como ack
    if(!sendInteger(_socket, CP_SYNC_CLIENT)){
        fprintf(stderr, "DropboxClient - Error sending CP_SYNC_CLIENT\n");
        return;
    }

    //Recebe o número de arquivos desse usuário no servidor
    bzero(buffer, sizeof(buffer));
    if(read(_socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error receiving ack for CP_SYNC_CLIENT\n");
        return;
    }
    numberOfFiles = atoi(buffer);

    //Laço que recebe o nome e o Mtime de cada arquivo do servidor
    for(i = 0; i < numberOfFiles; i++){

        //Recebe o nome do arquivo
        bzero(buffer, sizeof(buffer));
        if(read(_socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "DropboxClient - Error receiving file %d's name\n", i+1);
        }
        strncpy(curFileName, buffer, sizeof(curFileName));
        serverFiles.push_back(std::string(curFileName));

        //Recebe o M time do arquivo
        bzero(buffer, sizeof(buffer));
        if(read(_socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "DropboxClient - Error receiving file %d's M time\n", i+1);
        }
        serverFileMTime = convertTimeString(buffer);

        //Procura o arquivo no sync_dir local
        snprintf(curFilePath, sizeof(curFilePath), "%ssync_dir_%s/%s", CLIENT_SYNC_DIR_PATH, _userId, curFileName);
        if(access(curFilePath, F_OK) == -1){
            //Arquivo não encontrado, deve ser baixado do servidor
            fprintf(stderr, "%s - Download \'%s\' from server (reason: not found)\n", __FUNCTION__, curFileName);
            if(!sendInteger(_socket, CP_SYNC_FILE_NOT_FOUND)){
                fprintf(stderr, "DropboxClient - Erro sending CP_SYNC_FILE_NOT_FOUND\n");
            }
        }
        else{
            //Arquivo encontrado, compara os tempos de modificação
            localFileMTime = getMTimeValue(curFilePath);
            diffTimeValue = difftime(localFileMTime, serverFileMTime);

            if(diffTimeValue < 0){
                //Arquivo mais recente está no servidor
                fprintf(stderr, "%s - Download '%s' from server (reason: outdated)\n", __FUNCTION__, curFileName);
                if(!sendInteger(_socket, CP_SYNC_DOWNLOAD_FILE)){
                    fprintf(stderr, "DropboxClient - Erro sending CP_SYNC_DOWNLOAD_FILE\n");
                    //TODO
                }
            }
            else if(diffTimeValue > 0){
                //Arquivo mais recente está no cliente
                fprintf(stderr, "%s - Upload '%s' to server\n", __FUNCTION__, curFileName);
                send_file(curFilePath);
            }
            else{
                //O arquivo já está atualizado
                fprintf(stderr, "%s - File '%s' is up to date\n", __FUNCTION__, curFileName);
                if(!sendInteger(_socket, CP_SYNC_FILE_OK)){
                    fprintf(stderr, "DropboxClient - Erro sending CP_SYNC_FILE_OK\n");
                    //TODO
                }
            }
        }
    }
// =======
//
// 		while(currentAttempt!= maxAttempts && done == 0) {
//                 	if(!sendInteger(_socket, CP_SYNC_DOWNLOAD_FILE)){
//                     		fprintf(stderr, "DropboxClient - Erro sending CP_SYNC_DOWNLOAD_FILE\n");
// 				currentAttempt++;
//                 	}
// 			done = 1;
// 		}
// 		if(done == 0){
// 			close_conection();
// 			close(_socket);
// 			return;
// 		}
// 	    }
//
//
//             else{
// 	       if(diffTimeValue > 0){
//                     //Arquivo mais recente está no cliente
//                     fprintf(stderr, "%s - Upload '%s' to server\n", __FUNCTION__, curFileName);
//                     while(currentAttempt!= maxAttempts && done == 0) {
//                 	if(!sendInteger(_socket, CP_SYNC_DOWNLOAD_FILE)){
//                     		fprintf(stderr, "DropboxClient - Erro sending CP_SYNC_DOWNLOAD_FILE\n");
// 				currentAttempt++;
//                 	}
// 			done = 1;
// 		    }
// 		    if(done == 0){
// 			   close_conection();
// 			   close(_socket);
// 			   return;
// 		}
//                else{
//                   while(currentAttempt!= maxAttempts && done == 0) {
//                 	if(!sendInteger(_socket, CP_SYNC_DOWNLOAD_FILE)){
//                     		fprintf(stderr, "DropboxClient - Erro sending CP_SYNC_DOWNLOAD_FILE\n");
// 				currentAttempt++;
//                 	}
// 			done = 1;
// 		  }
// 		  if(done == 0){
// 			close_conection();
// 			close(_socket);
// 			return;
// 		  }
// 	      }
//          }
// >>>>>>> deaa3ab1c20dd0da7fb501cf5dadb5642c5e30d3
//     }
// }

    //Verifica se há novos arquivos no cliente utilizando o vector serverFiles
    snprintf(syncDirPath, sizeof(syncDirPath), "%s%s%s", CLIENT_SYNC_DIR_PATH, "sync_dir_", _userId);
    if((localSyncDir = opendir(syncDirPath)) != NULL){
        while((entry = readdir(localSyncDir)) != NULL){
            // Para cada arquivo no diretório
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
                continue;
            }

            //Para cada arquivo útil do diretório
            syncedFile = false;
            for(i = 0; i < serverFiles.size(); i++){
                if(strcmp(serverFiles.at(i).c_str(), entry->d_name) == 0){
                    //Encontrou arquivo com mesmo nome, já está sincrinizado
                    syncedFile = true;
                    break;
                }
            }
            if(!syncedFile){
                //Arquivo não sicronizado, deve ser enviado ao servidor
                snprintf(curFilePath, sizeof(curFilePath), "%s/%s", syncDirPath, entry->d_name);
                fprintf(stderr, "DropboxClient - Uploading unsynchronized file \'%s\'\n", basename(curFilePath));
                send_file(curFilePath);
            }
        }
        closedir(localSyncDir);
    }
}

/* Mostra a lista de arquivos e informações do cliente */
void DropboxClient::list_client(){

    DIR *localSyncDir;
    struct dirent *entry;
    struct stat statBuff;
    char syncDirPath[200], curFilePath[200];
    char curATime[50], curMTime[50], curCTime[50];
    int curFileSize;

    snprintf(syncDirPath, sizeof(syncDirPath), "%s%s%s/", CLIENT_SYNC_DIR_PATH, "sync_dir_", _userId);
    fprintf(stderr, "FILES STORED IN THE LOCAL SYNC_DIR (%s): \n\n", syncDirPath);
    fprintf(stderr, "%30s    %8s    %30s     %30s     %30s\n", "Name", "Size (B)", "Creation Time", "Modification Time", "Access Time");

    if((localSyncDir = opendir(syncDirPath)) != NULL){
        while((entry = readdir(localSyncDir)) != NULL){
            // Para cada arquivo no diretório
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
                continue;
            }
            // Para todos os arquivos úteis
            snprintf(curFilePath, sizeof(curFilePath), "%s%s", syncDirPath, entry->d_name);

            stat(curFilePath, &statBuff);
            strftime(curMTime, sizeof(curMTime), "%Y-%m-%d %H:%M:%S", localtime(&statBuff.st_mtime));
            strftime(curATime, sizeof(curATime), "%Y-%m-%d %H:%M:%S", localtime(&statBuff.st_atime));
            strftime(curCTime, sizeof(curCTime), "%Y-%m-%d %H:%M:%S", localtime(&statBuff.st_ctime));
            getFileSize(curFilePath, &curFileSize);

            fprintf(stderr, "%30s     %6d     %30s     %30s     %30s\n", entry->d_name, curFileSize, curCTime, curMTime, curATime);
        }
        fprintf(stderr, "FIM DOS ARQUIVOS\n");
    }
    closedir(localSyncDir);
}

/* Envia um arquivo */
void DropboxClient::send_file(char* filePath){

    FILE *file;
    int fileSize, i, iterations, sizeSent, sizeToSend;
    char buffer[CP_MAX_MSG_SIZE], mTime[CP_MAX_MSG_SIZE];

    // Verifica o tamanho do nome do arquivo
    if(strlen(basename(filePath)) > CP_MAX_MSG_SIZE-1){
        fprintf(stderr, "DropboxClient - Name size limit exceded\n");
        return;
    }
    bzero(mTime, sizeof(mTime));
    getMTime(filePath, mTime, sizeof(mTime));
    // Abre o arquivo para leitura
    if(!(file = fopen(filePath, "r"))){
        // Erro na abertura
        fprintf(stderr, "DropboxClient - Error opening file \'%s\'\n", filePath);
        return;
    }

    // Descobre o tamanho do arquivo
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Envia pedido de envio de arquivo
    sendInteger(_socket, CP_CLIENT_SEND_FILE);
    if(!receiveExpectedInt(_socket, CP_CLIENT_SEND_FILE_ACK)){
        fprintf(stderr, "DropboxClient - Error receiving confirmation from server\n");
        fclose(file);
        return;
    }

    // Envia o nome do arquivo
    bzero(buffer, sizeof(buffer));
    strncpy(buffer, basename(filePath), sizeof(buffer));
    if(write(_socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error sending filename \'%s\'\n", buffer);
        fclose(file);
        return;
    }

    // Envia o tamanho do arquivo
    if(!sendInteger(_socket, fileSize)){
        fprintf(stderr, "DropboxClient - Error sending file size\n");
        fclose(file);
        return;
    }

    if(!receiveExpectedInt(_socket, CP_CLIENT_SEND_FILE_SIZE_ACK)){
        fprintf(stderr, "DropboxClient - Error receiving size confirmation from server\n");
        fclose(file);
        return;
    }

    // Começa o envio do arquivo
    iterations = (fileSize%CP_MAX_MSG_SIZE) > 0 ? fileSize/CP_MAX_MSG_SIZE + 1 : fileSize/CP_MAX_MSG_SIZE;
    sizeSent = 0;
    for(i = 0; i < iterations; i++){

        fprintf(stderr, "DropboxClient - Sending file, iteration %d of %d\n", i+1, iterations);
        sizeToSend = (fileSize - sizeSent) > CP_MAX_MSG_SIZE ? CP_MAX_MSG_SIZE : (fileSize - sizeSent);
        bzero(buffer, sizeof(buffer));
        fread((void*) buffer, sizeToSend, 1, file);
        if(write(_socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "DropboxClient - Error sending part %d of file\n", i);
        }
        if(!receiveExpectedInt(_socket, CP_FILE_PART_RECEIVED)){
            fprintf(stderr, "DropboxClient - Error receving ack for part %d of %d\n", i+1, iterations);
        }
        sizeSent += sizeToSend;
    }
    fclose(file);

    // Envia mesagem informando fim do arquivo
    if(!sendInteger(_socket, CP_SEND_FILE_COMPLETE)){
        fprintf(stderr, "DropboxClient - Error sending file send complietion message\n");
        return;
    }

    // Recebe ack
    if(!receiveExpectedInt(_socket, CP_SEND_FILE_COMPLETE_ACK)){
        fprintf(stderr, "DropboxClient - Error receiving ack for file completion\n");
        return;
    }

    // Envia a data de ultima modificação do arquivo
    if(write(_socket, mTime, sizeof(mTime)) < 0){
        fprintf(stderr, "DropboxClient - Error sending file's M time \'%s\'\n", buffer);
        return;
    }

    // Exibe mensagem ao usuário
    if(sizeSent == fileSize){
        fprintf(stderr, "DropboxClient - File sent \'%s\' successfully\n", basename(filePath));
    }
}

/* Recebe um arquivo do servidor */
void DropboxClient::get_file(char* filePath, char *destination){

    FILE *file = NULL;
    int fileSize, sizeReceived, iterations, sizeToReceive;
    char buffer[CP_MAX_MSG_SIZE], newfilePath[CP_MAX_MSG_SIZE];
    FILE *newFile;

    snprintf(newfilePath, sizeof(newfilePath), "%s/%s", destination, filePath);

    // Verifica o tamanho do nome do arquivo
    if(strlen(basename(filePath)) > CP_MAX_MSG_SIZE-1){
        fprintf(stderr, "DropboxClient - Name size limit exceded\n");
        return;
    }

    // Envia pedido de download de arquivo
    sendInteger(_socket, CP_CLIENT_GET_FILE);
    if(!receiveExpectedInt(_socket, CP_CLIENT_GET_FILE_ACK)){
        fprintf(stderr, "DropboxClient - Error receiving confirmation from server\n");
        fclose(file);
        return;
    }

    // Envia o nome do arquivo
    bzero(buffer, sizeof(buffer));
    strncpy(buffer, basename(filePath), sizeof(buffer));
    if(write(_socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error sending filename \'%s\'\n", buffer);
        fclose(file);
        return;
    }
    // Recebe confirmação da existência do arquivo
    if(!receiveExpectedInt(_socket, CP_CLIENT_GET_FILE_EXISTS)){
        fprintf(stderr, "DropboxClient - Error confirming file existance\n");
        return;
    }
    // if(!sendInteger(_socket, CP_CLIENT_GET_FILE_EXISTS_ACK)){
    //     fprintf(stderr, "DropboxClient - Error sending file existance ack\n");
    //     return;
    // }

    // Cria o arquivo
    if(!(newFile = fopen(newfilePath, "w"))){
        fprintf(stderr, "Socket %d - Error creating file \'%s\'\n", socket, destination);
        return;
    }

    // Recebe o tamanho do arquivo
    bzero(buffer, sizeof(buffer));
    if(read(_socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error receiving file size\n");
        return;
    }
    if(!sendInteger(_socket, CP_CLIENT_GET_FILE_SIZE_ACK)){
        fprintf(stderr, "DropboxClient - Error sending file size ack\n");
        return;
    }
    fileSize = atoi(buffer);
    // Recebe o arquivo
    sizeReceived = 0;
    fprintf(stderr, "DropboxClient - Downloading \'%s\' to \'%s\'\n", filePath, destination);
    iterations = (fileSize%CP_MAX_MSG_SIZE) > 0 ? fileSize/CP_MAX_MSG_SIZE + 1 : fileSize/CP_MAX_MSG_SIZE;
    for(int i = 0; i < iterations; i++){

        fprintf(stderr, "Socket %d - Receiving file, iteration %d of %d\n", _socket, i+1, iterations);
        sizeToReceive = (fileSize - sizeReceived) > CP_MAX_MSG_SIZE ? CP_MAX_MSG_SIZE : (fileSize - sizeReceived);
        bzero(buffer, sizeof(buffer));
        if(read(_socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "Socket %d - Error receiving part of file %d\n", _socket, i+1);
        }
        fwrite((void*) buffer, sizeToReceive, 1, newFile);
        if(!sendInteger(_socket, CP_FILE_PART_RECEIVED)){
            fprintf(stderr, "Socket %d - Error sending ack for part %d of %d\n", _socket, i+1, iterations);
        }
        sizeReceived += sizeToReceive;
    }
    fclose(newFile);
    //Recebe confirmação de término do envio do arquivo
    if(!receiveExpectedInt(_socket, CP_SEND_FILE_COMPLETE)){
        fprintf(stderr, "Socket %d - Error receiving CP_SEND_FILE_COMPLETE\n", _socket);
        return;
    }
    fprintf(stderr, "RECEBEU CP_SEND_FILE_COMPLETE\n");

    if(!sendInteger(_socket, CP_SEND_FILE_COMPLETE_ACK)){
        fprintf(stderr, "Socket %d - Error sending CP_SEND_FILE_COMPLETE_ACK\n", _socket);
        return;
    }
    fprintf(stderr, "ENVIOU CP_SEND_FILE_COMPLETE_ACK\n");
    // Exibe mensagem ao usuário
    if(sizeReceived == fileSize){
        fprintf(stderr, "DropboxClient - File sent \'%s\' successfully\n", basename(filePath));
    }
    fprintf(stderr, "Tamanho do arquivo: %d\n", fileSize);
}

/* Delete algum arquivo do servidor */
void DropboxClient::delete_file(char* file){

    char buffer[CP_MAX_MSG_SIZE];

    //Envia o comando ao servidor
    if(!sendInteger(_socket, CP_CLIENT_DELETE_FILE)){
        fprintf(stderr, "DropboxClient - Error sending CP_CLIENT_DELETE_FILE\n");
        return;
    }
    //Espera o ack
    if(!receiveExpectedInt(_socket, CP_CLIENT_DELETE_FILE_ACK)){
        fprintf(stderr, "DropboxClient - Error receiving CP_CLIENT_DELETE_FILE_ACK\n");
        return;
    }

    //Envia o nome do arquivo
    bzero(buffer, sizeof(buffer));
    strncpy(buffer, basename(file), sizeof(buffer));
    if(write(_socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error sending file name\n");
        return;
    }
}

/* Termina a conexão */
void DropboxClient::close_connection(){

    if(!sendInteger(_socket, CP_CLIENT_END_CONNECTION)){
        fprintf(stderr, "DropboxClient - Error sending close connection message\n");
    }
    else{
        fprintf(stderr, "DropboxClient - Closing connection with server\n");
    }

    _isConnected = false;
}
//------------------------------------------------OUTRAS FUNÇÕES

/*Faz o loop que lê comandos do usuário e retorna os argumentos por meio de comandBuffer*/
int DropboxClient::readComand(char* comandBuffer, int bufferSize){

    char comand[MAXCOMANDSIZE];

    while(1){
        printf("> ");
        fgets(comand, MAXCOMANDSIZE, stdin);
        if(comand[strlen(comand)-1] == '\n'){
            // Elimina o \n lido ao fim
            comand[strlen(comand)-1] = '\0';
        }

        if(strncmp(comand, "upload", 6) == 0){
            if(comand[6] == ' '){
                // Copia os argumentos sem o espaço do inicio
                strncpy(comandBuffer, comand, bufferSize);
                return COM_UPLOAD;
            }
            else{
                // Não tem espaço entre comando e argumento
                fprintf(stderr, "DropboxClient - Usage: upload <path/filename>\n");
            }
        }
        else if(strncmp(comand, "download", 8) == 0){
            if(comand[8] == ' '){
                // Copia os argumentos sem o espaço do inicio
                strncpy(comandBuffer, comand, bufferSize);
                return COM_DOWNLOAD;
            }
            else{
                // Não tem espaço entre comando e argumento
                fprintf(stderr, "DropboxClient - Usage: download <filename>\n");
            }
        }
        else if(strcmp(comand, "list_server") == 0){
            strncpy(comandBuffer, comand, bufferSize);
            return COM_LIST_SERVER;
        }
        else if(strcmp(comand, "list_client") == 0){
            strncpy(comandBuffer, comand, bufferSize);
            return COM_LIST_CLIENT;
        }
        else if(strcmp(comand, "get_sync_dir") == 0){
            strncpy(comandBuffer, comand, bufferSize);
            return COM_GET_SYNC_DIR;
        }
        else if(strcmp(comand, "exit") == 0){
            return COM_EXIT;
        }
        else{
            printf("DropboxClient - Comando não reconhecido\n");
        }
    }
}

/*Verifica se hove alterações dentro do diretório em path*/
void* DropboxClient::fileWatcher(void* clientClass){

    int fileDesc, watchDesc, length, isRunning = 1;
    struct inotify_event *event;
    char *eventPtr, syncDirPath[256], buffer[4098], curFilePath[256];
    DropboxClient* client = (DropboxClient*) clientClass;

    snprintf(syncDirPath, sizeof(syncDirPath), "%ssync_dir_%s", CLIENT_SYNC_DIR_PATH, client->getUserId());
    fprintf(stderr, "DropboxClient - syncDirPath = %s\n", syncDirPath);

    fileDesc = inotify_init();

    if(fileDesc < 0){
        fprintf(stderr, "DropboxClient - Error starting inotify\n");
        return NULL;
    }

    watchDesc = inotify_add_watch(fileDesc, syncDirPath, IN_MODIFY | IN_CREATE | IN_DELETE);

    if(watchDesc < 0){
        fprintf(stderr, "DropboxClient - Error adding watch to \'%s\'\n", syncDirPath);
        isRunning = 0;
    }

    while(isRunning){
        //Verifica se arquivos foram alterados
        length = read(fileDesc, buffer, sizeof(buffer));

        if(length < 0){
            //Erro na leitura
            fprintf(stderr, "DropboxClient - Error watching directory \'%s\'\n", syncDirPath);
            isRunning = 0;
        }
        else{
            //Detectou mudança nos arquivos
            for(eventPtr = buffer; eventPtr < buffer + length; eventPtr += sizeof(struct inotify_event) + event->len){
                //Itera sobre os eventos lidos
                event = (struct inotify_event*) eventPtr;

                if(event->mask & IN_MODIFY){
                    fprintf(stderr, "DropboxClient - File \'%s\' was modified\n", event->name);
                    snprintf(curFilePath, sizeof(curFilePath), "%s/%s", syncDirPath, event->name);
                    client->send_file(curFilePath);
                }
                else if(event->mask & IN_CREATE){
                    fprintf(stderr, "DropboxClient - File \'%s\' was created\n", event->name);
                    snprintf(curFilePath, sizeof(curFilePath), "%s/%s", syncDirPath, event->name);
                    client->send_file(curFilePath);
                }
                else if(event->mask & IN_DELETE){
                    fprintf(stderr, "DropboxClient - File \'%s\' was deleted\n", event->name);
                    snprintf(curFilePath, sizeof(curFilePath), "%s/%s", syncDirPath, event->name);
                    client->delete_file(curFilePath);
                }
            }
        }
    }
    inotify_rm_watch(fileDesc, watchDesc);
    close(fileDesc);

    return NULL;
}

/*Executa as ações referentes ao comand get_sync_dir*/
void DropboxClient::getSyncDirComand(){

    char syncDirPath[200];
    int errorCode;

    snprintf(syncDirPath, sizeof(syncDirPath), "%ssync_dir_%s", CLIENT_SYNC_DIR_PATH, _userId);
    //Verifica se sync_dir já existe no cliente
    if(access(CLIENT_SYNC_DIR_PATH, F_OK) == -1){
        if((errorCode = mkdir(CLIENT_SYNC_DIR_PATH, S_IRWXU)) != 0){
            fprintf(stderr, "DropboxClient - Error %d creating sync dir %s\n", errorCode, CLIENT_SYNC_DIR_PATH);
            close_connection();
	    close(_socket);
	    fprintf(stderr, "Connection Close, Socket Close\n");
            return;
        }
    }
    if(access(syncDirPath, F_OK) == -1){
        //Diretório não encontrado
        fprintf(stderr, "DropboxClient - Directory \'%s\' is being created...\n", syncDirPath);
        if((errorCode = mkdir(syncDirPath, S_IRWXU)) != 0){
            fprintf(stderr, "DropboxClient - Error %d creating directory %s\n", errorCode, syncDirPath);
            close_connection();
	    close(_socket);
	    fprintf(stderr, "Connection Close, Socket Close\n");
            return;
        }
        sync_client();
        // sendInteger(_socket, CP_SYNC_DIR_NOT_FOUND);
        //dumpServerFiles();
    }
    else{
        // Diretório encontrado
        // sendInteger(_socket, CP_SYNC_DIR_FOUND);
        sync_client();
    }
}

/* Executa as ações referentes ao comand list_server */
void DropboxClient::listServerComand(){

    char buffer[CP_MAX_MSG_SIZE];
    char curFileName[CP_MAX_MSG_SIZE], curATime[50], curMTime[50], curCTime[50];
    int numberOfFiles, i, curFileSize;

    //Manda solicitação para o servidor
    if(!sendInteger(_socket, CP_LIST_SERVER)){
        fprintf(stderr, "DropboxClient - Error sending CP_LIST_SERVER\n");
        return;
    }
    //Recebe ack
    if(!receiveExpectedInt(_socket, CP_LIST_SERVER_ACK)){
        fprintf(stderr, "DropboxClient - Error receiving CP_LIST_SERVER_ACK\n");
        return;
    }

    //Recebe número de arquivos
    bzero(buffer, sizeof(buffer));
    if(read(_socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error receiving number of files\n");
        return;
    }
    numberOfFiles = atoi(buffer);

        fprintf(stderr, "FILES STORED IN THE SERVER'S SYNC_DIR: \n\n");
        fprintf(stderr, "%30s    %8s    %30s     %30s     %30s\n", "Name", "Size (B)", "Creation Time", "Modification Time", "Access Time");

    //Coleta os dados de FILE para todos os arquivos
    for(i = 0; i < numberOfFiles; i++){

        //Recebe nome do arquivo
        bzero(buffer, sizeof(buffer));
        if(read(_socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "DropboxClient - Error receiving file name\n");
        }
        strncpy(curFileName, buffer, sizeof(curFileName));

        //Recebe tamanho do arquivo
        bzero(buffer, sizeof(buffer));
        if(read(_socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "DropboxClient - Error receiving file size\n");
        }
        curFileSize = atoi(buffer);

        //Recebe A time do arquivo
        bzero(buffer, sizeof(buffer));
        if(read(_socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "DropboxClient - Error receiving A time\n");
        }
        strncpy(curATime, buffer, sizeof(curFileName));

        //Recebe M time do arquivo
        bzero(buffer, sizeof(buffer));
        if(read(_socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "DropboxClient - Error receiving M time\n");
        }
        strncpy(curMTime, buffer, sizeof(curFileName));

        //Recebe C time do arquivo
        bzero(buffer, sizeof(buffer));
        if(read(_socket, buffer, sizeof(buffer)) < 0){
            fprintf(stderr, "DropboxClient - Error receiving C time\n");
        }
        strncpy(curCTime, buffer, sizeof(curFileName));

        //Imprime a informação toda na tela
        fprintf(stderr, "%30s     %6d     %30s     %30s     %30s\n", curFileName, curFileSize, curCTime, curMTime, curATime);

        //Envia uma confirmação
        if(!sendInteger(_socket, CP_LIST_SERVER_FILE_OK)){
            fprintf(stderr, "DropboxClient - Error sending CP_LIST_SERVER_FILE_OK\n");
        }
    }
}

/* Envia o userId para o servidor e aguarda confirmação de log in */
bool DropboxClient::sendUserId(char* userId){

    char buffer[CP_MAX_MSG_SIZE];

    strncpy(buffer, userId, sizeof(buffer));
    strncpy(_userId, userId, sizeof(_userId));

    // Envia a identificação (ID do usuário) para o servidor
    if(write(_socket, buffer, sizeof(buffer)) < 0){
          fprintf(stderr, "DropboxClient - Error sending userId\n");
          return false;
    }

    // Aguarda mensagem de sucesso (ou fracasso) do login
    if(read(_socket, buffer, sizeof(buffer)) < 0){
        fprintf(stderr, "DropboxClient - Error receiving sign in message\n");
        return false;
  	}
    else{
        if(atoi(buffer) == CP_LOGIN_SUCCESSFUL){
            return true;
        }
        else if(atoi(buffer) == CP_LOGIN_FAILED){
            return false;
	      }
  	    else{
    		    fprintf(stderr, "DropboxClient - Internal error during sign in\n");
  		      return false;
        }
  	}

    // default (outro erro)
    return false;
}

int DropboxClient::getSocket(){ return _socket; }
bool DropboxClient::getIsConnected(){ return _isConnected; }
char* DropboxClient::getUserId(){ return _userId; }
void DropboxClient::setUserId(char* userId){ strncpy(_userId, userId, sizeof(_userId)); }
