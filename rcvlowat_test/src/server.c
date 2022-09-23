#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>

void errcheck(const int errc, const char* errmsg) {
    if (errc == -1) {
        printf("%s (errno %d)\n", errmsg, errno);
        exit(12); // NZEC
    }
}

int main(int argc, char** argv) {

    const char* port = "16668";

    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int errc = getaddrinfo(NULL, port, &hints, &res);
    errcheck(errc, "Could not get address information");

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    errcheck(fd, "Could not create socket");

    errc = bind(fd, res->ai_addr, res->ai_addrlen);
    errcheck(errc, "Could not bind socket to address");

    listen(fd, 10);

    freeaddrinfo(res);

    uint32_t data = 5328567;

    while (true) {
        printf("Listening for a client\n");
        // accept a connection and send 4 bytes
        struct sockaddr_in clientaddr;
        socklen_t len = sizeof(clientaddr);
        int clt = accept(fd, (struct sockaddr*)&clientaddr, &len);
        printf("Accepted a client\n");

        send(clt, &data, sizeof(data), 0);
        printf("Sent 4 bytes to the client\n");
    }

    return 0;
}