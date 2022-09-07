#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 1005
#define MAX_EVENTS 64
#define BUF_SIZE 1500

int fd_clients[MAX_CLIENTS];

// TODO 

void recv_chunk(int s) {
    char buf[MAX_MSG_SIZE];
    int bytes_read = recv(s, buf, sizeof(buf) - 1, 0);
    buf[bytes_read] = 0;
    printf("client #%d: %s", get_conn(s), buf);
    fflush(stdout);
}

int main(int argc, char** argv) {



    return 0;
}