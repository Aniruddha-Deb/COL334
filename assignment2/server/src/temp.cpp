#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <memory>
#include <iostream>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <fstream>
#include <vector>
#include <list>
#include <unordered_map>

#include "server.hpp"

#define MAX_CLIENTS 1005
#define MAX_EVENTS 64
#define BUF_SIZE 1500

volatile bool running = true;

void kill(int signal) {
    running = false;
}

// TODO 

class TCPConnection {

    struct sockaddr *addr;

public:

    int sock_fd;

    TCPConnection(int fd): sock_fd{fd} {}

    void write_chunk(std::shared_ptr<FileChunk> c) {
        // TODO write chunk to fd
    }

    void read() {
        // TODO read packet from fd
    }

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
        fcntl(sock_fd, F_SETFL, O_NONBLOCK);
    }

    void bind() {
        ::bind(sock_fd, info->ai_addr, info->ai_addrlen);
    }

    void listen() {
        ::listen(sock_fd, 20);
    }

    void close() {
        freeaddrinfo(info);

    }

    TCPConnection accept() {
        struct sockaddr_storage addr;
        socklen_t socklen = sizeof(addr);

        int fd = ::accept(sock_fd, (struct sockaddr *) &addr, &socklen);



    }

};

class UDPSocket {


};

std::vector<std::shared_ptr<FileChunk>> read_and_chunk_file(std::string filename) {

    std::ifstream infile(filename);

    infile.seekg(0, std::ios::end);
    size_t length = infile.tellg();
    infile.seekg(0, std::ios::beg);

    uint16_t id_ctr = 1;
    std::vector<std::shared_ptr<FileChunk>> v;

    while (infile.eofbit != 1) {
        FileChunk f;
        f.id = id_ctr;
        infile.read(f.data, 1024);
        f.size = infile.gcount();

        v.push_back(std::shared_ptr<FileChunk>(&f));
    }

    return v;
}



class Server {

    uint16_t port;
    uint16_t udp_port;
    
    std::unordered_map<uintptr_t, TCPConnection> clients;
    TCPServerSocket sock;

    // register a write + read circular buffer for each socket
    std::unordered_map<uintptr_t, >


    // bytes to write

public:

    Server(std::string addr, uint16_t p): port{p}, sock{port, addr} {}

    void run() {
        // launches event loop, and runs until termination.
        // asynchronously poll the sockets 

        int kq = kqueue();
        struct kevent evSet;
        struct kevent evList[MAX_EVENTS];

        EV_SET(&evSet, sock.sock_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        kevent(kq, &evSet, 1, NULL, 0, NULL);

        struct sockaddr_storage addr;
        socklen_t socklen = sizeof(addr);

        while (running) { // This is ugly, but I couldn't figure out an alternative

            int num_events = kevent(kq, NULL, 0, evList, MAX_EVENTS, NULL);

            for (int i=0; i<num_events; i++) {

                if (evList[i].ident == sock.sock_fd) {
                    
                    // accept a new connection
                    // TODO exception handling: what if fd is negative?
                    TCPConnection conn = sock.accept();
                    // ? what if file descriptor clashes? Same user reconnecting?
                    clients[conn.sock_fd] = conn;

                    // async reads
                    EV_SET(&evSet, conn.sock_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                    kevent(kq, &evSet, 1, NULL, 0, NULL);
                    // async writes
                    EV_SET(&evSet, conn.sock_fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
                    kevent(kq, &evSet, 1, NULL, 0, NULL);
                        
                } 
                else if (evList[i].flags & EV_EOF) {

                    // disconnect 
                    int fd = evList[i].ident;
                    
                    EV_SET(&evSet, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                    kevent(kq, &evSet, 1, NULL, 0, NULL);
                    
                    clients.erase(fd);
                }
                else if (evList[i].filter == EVFILT_READ) {
                    // 
                    recv_msg(evList[i].ident);
                }                
            }
        }

        // terminate all connections
        std::cout << "Terminating connections" << std::endl;
    }

};

int main(int argc, char** argv) {

    std::signal(SIGINT, kill);
    std::signal(SIGTERM, kill);
    std::signal(SIGKILL, kill);

    Server srv("127.0.0.1", 15001);
    srv.run();

    return 0;
}