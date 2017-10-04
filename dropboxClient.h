#include <stdio.h>

#define COM_UPLOAD          1
#define COM_DOWNLOAD        2
#define COM_LIST_SERVER     3
#define COM_LIST_CLIENT     4
#define COM_GET_SYNC_DIR    5
#define COM_EXIT            6

#define MAXNAME 20
#define MAXCOMANDSIZE 50

int readComand(char* comandBuffer);
