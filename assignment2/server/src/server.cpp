#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <iostream>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <map>

#define MAX_CLIENTS 1005
#define MAX_EVENTS 64
#define BUF_SIZE 1500

volatile bool running = true;

void kill(int signal) {
    running = false;
}

// TODO 

class TCPConnection {

    int sock_fd;
};

class TCPServerSocket {

    uint16_t port;
    std::string addr;
    
    struct addrinfo *info;

public:

    int sock_fd;

    TCPServerSocket(uint16_t source_port, std::string source_addr): 
        addr(source_addr),
        port(source_port) {

        struct addrinfo hints;
        memset(&hints, 0, sizeof hints);
        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        ::getaddrinfo(source_addr.c_str(), std::to_string(source_port).c_str(), &hints, &info);
        sock_fd = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    }

    void bind() {
        ::bind(sock_fd, info->ai_addr, info->ai_addrlen);
    }

    void listen() {
        ::listen(sock_fd, 20);
    }

    void close() {

    }

    TCPConnection accept() {

    }

};

class UDPSocket {


};

class LRUCache {

};

class Server {

    uint16_t _port;
    std::map<uintptr_t, TCPConnection> _clients;
    TCPServerSocket sock;

public:

    Server(std::string addr, uint16_t port): _port{port}, sock{port, addr} {}

    void run() {
        // launches event loop, and runs until termination.
        // asynchronously poll the sockets 

        int kq = kqueue();
        struct kevent evSet;
        EV_SET(&evSet, sock.sock_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        kevent(kq, &evSet, 1, NULL, 0, NULL);

        struct kevent evSet;
        struct kevent evList[MAX_EVENTS];
        struct sockaddr_storage addr;
        socklen_t socklen = sizeof(addr);

        while (1) {
            int num_events = kevent(kq, NULL, 0, evList, MAX_EVENTS, NULL);
            for (int i = 0; i < num_events; i++) {
                // receive new connection

        while (running) { // This is ugly, but I couldn't figure out an alternative
            
        }

        // terminate all connections
        std::cout << "Terminating connections" << std::endl;
    }

};

int main(int argc, char** argv) {

    std::signal(SIGINT, kill);
    std::signal(SIGTERM, kill);
    std::signal(SIGKILL, kill);

    Server srv(15001);
    srv.run();

    return 0;
}