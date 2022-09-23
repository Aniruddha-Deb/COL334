#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>

#include <sys/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

void errcheck(const int errc, const char* errmsg) {
    if (errc == -1) {
        printf("%s (errno %d)\n", errmsg, errno);
        exit(12); // NZEC
    }
}

int main(int argc, char** argv) {

    const char* srvport = "16668";
    const char* srvaddr = "127.0.0.1";

    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int errc = getaddrinfo(srvaddr, srvport, &hints, &res);
    errcheck(errc, "Could not get server address information");

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    errcheck(fd, "Could not create socket");

    // set socket low water mark
    const int lwm = 8;
    errc = setsockopt(fd, SOL_SOCKET, SO_RCVLOWAT, &lwm, sizeof(lwm));
    errcheck(errc, "Could not set low water mark on socket");

    errc = connect(fd, res->ai_addr, res->ai_addrlen);
    errcheck(errc, "Could not connect to server");
    printf("Successfully connected to server\n");

    // set socket to be non blocking
    fcntl(fd, F_SETFL, O_NONBLOCK);

    freeaddrinfo(res);

    // create an event queue and add the socket to it
    int kq = kqueue();
    struct kevent evt;
    EV_SET(&evt, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &evt, 1, NULL, 0, NULL);

    struct kevent evts[64];
    while (true) {
        int n_evts = kevent(kq, NULL, 0, evts, 64, NULL);
        errcheck(n_evts, "Could not retrieve events from kqueue");

        for (int i=0; i<n_evts; i++) {
            if (evts[i].ident == fd && evts[i].filter == EVFILT_READ) {
                // read from the socket
                uint64_t data = 0;
                int nbytes = recv(fd, &data, sizeof(data), 0);
                errcheck(nbytes, "Could not read data from TCP socket");
                printf("Read %d bytes from TCP\n", nbytes);
            }
        }

        if (n_evts >= 1) break;
    }

    return 0;
}