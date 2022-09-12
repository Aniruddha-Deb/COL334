#include <string>
#include <cstring>
#include <unistd.h>

#include "tcp_connection.hpp"

extern volatile bool running;

class TCPServerSocket {

    uintptr_t _fd;

public:
    TCPServerSocket(uint16_t port) {
        struct addrinfo hints;
        struct addrinfo *res;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE; // fill in IP for us
        getaddrinfo(nullptr, std::to_string(port).c_str(), &hints, &res);

        _fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (_fd == -1) {
            std::cout << "ERROR: failed to allocate server socket at port " << port << std::endl;
            std::cout << "Terminating" << std::endl;
            running = false;
        }

        // set socket to be non-blocking
        fcntl(_fd, F_SETFL, O_NONBLOCK);

        bind(_fd, res->ai_addr, res->ai_addrlen);

        freeaddrinfo(res);
    }

    void listen() {
        ::listen(_fd, 10);
    }

    // change this to accept a 
    std::unique_ptr<TCPConnection> accept(uint32_t client_id) {
        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(addr);
        uintptr_t new_fd = ::accept(_fd, (struct sockaddr*)&addr, &addr_size);
        std::unique_ptr<TCPConnection> conn(new TCPConnection(new_fd, client_id, addr));
        return conn;
    }

    void close() {
        ::close(_fd);
    }

    uintptr_t get_fd() {
        return _fd;
    }
};