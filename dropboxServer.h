#include <stdio.h>

typedef struct file_info{
    char name[MAXNAME];
    char extensions[MAXNAME];
    char last_modified[MAXFILES];
    int size;
} FILE_INFO;

typedef struct client{
    int devices[2];
    char userid[MAXNAME];
    CLIENT file_info[MAXFILES];
    int logged_in;
}CLIENT;
