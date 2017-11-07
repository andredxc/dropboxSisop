#ifndef DROPBOXUTIL_H
#define DROPBOXUTIL_H

#include <stdlib.h>
#include "../Server/dropboxServer.h"

#define MAXNAME 20

//Definição do protocolo de comunicação entre cliente e servidor
#define CP_SYNC_DIR_FOUND       		1
#define CP_SYNC_DIR_NOT_FOUND   		2

#define CP_LOGIN_SUCCESSFUL				3
#define CP_LOGIN_FAILED					4

#define CP_CLIENT_SEND_FILE				5
#define CP_CLIENT_SEND_FILE_ACK			6
#define CP_CLIENT_SEND_FILE_SIZE_ACK	7

#define CP_CLIENT_END_CONNECTION        8

#define CP_SEND_FILE_COMPLETE           9
#define CP_FILE_PART_RECEIVED           10
#define CP_SEND_FILE_COMPLETE_ACK       11

#define CP_SYNC_CLIENT                  12
#define CP_SYNC_FILE_COMPUTED           13
#define CP_SYNC_FILE_OK                 14
#define CP_SYNC_UPLOAD_FILE             15
#define CP_SYNC_DOWNLOAD_FILE           16
#define CP_SYNC_FILE_NOT_FOUND          17

#define CP_CLIENT_GET_FILE				18
#define CP_CLIENT_GET_FILE_ACK			19
#define CP_CLIENT_GET_FILE_SIZE_ACK	20

#define CP_MAX_MSG_SIZE  				256

//Outras constantes
#define CLIENT_SYNC_DIR_PATH    "./files/clientFiles/"
#define SERVER_SYNC_DIR_PATH    "./files/serverFiles/"

bool sendInteger(int socket, int message);
bool receiveExpectedInt(int socket, int message);
const char *getFileExtension(const char *filename);
const char *get_filename_ext(const char *filename);
void getMTime(const char* filePath, char* buffer, int bufferSize);
void printFileInfo(struct file_info file);
void printClient(struct client client, bool printAll);
const char *getFileName(const char *filename, char* buffer);
void getFileSize(const char* filePath, int* buffer);
void getFileInfo(const char* filePath, struct file_info *info);
time_t convertTimeString(const char* timeString);
time_t getMTimeValue(const char* filePath);

#endif
