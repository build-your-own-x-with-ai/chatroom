#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define PORT 8888
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define USERNAME_SIZE 32

// Message types
#define MSG_JOIN 1
#define MSG_LEAVE 2
#define MSG_TEXT 3
#define MSG_USER_LIST 4

// Message structure
typedef struct {
    int type;
    char username[USERNAME_SIZE];
    char content[BUFFER_SIZE];
} Message;

#endif
