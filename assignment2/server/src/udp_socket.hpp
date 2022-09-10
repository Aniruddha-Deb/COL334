#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <queue>

#include "chunk.hpp"

extern volatile bool running;

class UDPSocket {

    uintptr_t _fd;

    std::function<void(uint32_t,uint32_t)> _req_callback;
    std::queue<std::pair<struct sockaddr_in,uint32_t>> _chunk_req_buffer;

public:

    UDPSocket(uint16_t port) {
        struct addrinfo hints;
        struct addrinfo *res;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE; // fill in IP for us
        // TODO this can also return an error. Error handling here
        getaddrinfo(nullptr, std::to_string(port).c_str(), &hints, &res);

        _fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (_fd == -1) {
            std::cout << "ERROR: failed to allocate UDP socket on port " << port << std::endl;
            std::cout << "Terminating" << std::endl;
            running = false;
        }

        // set socket to be non-blocking
        fcntl(_fd, F_SETFL, O_NONBLOCK);

        // set socket low-water mark value for input to be 8 bytes
        // (ID of chunk we're requesting)
        const int sock_recv_lwm = sizeof(uint64_t);
        setsockopt(_fd, SOL_SOCKET, SO_RCVLOWAT, &sock_recv_lwm, sizeof(sock_recv_lwm));

        freeaddrinfo(res);
    }

    void on_chunk_request(const std::function<void(uint32_t,uint32_t)> callback) {
        _req_callback = callback;
    }

    void request_chunk_async(const struct sockaddr_in addr, const uint32_t chunk_id) {
        _chunk_req_buffer.push({addr,chunk_id});
    }

    void can_read() {
        uint64_t req_data;
        struct sockaddr_in sender_addr;
        socklen_t sender_addr_len = sizeof(sender_addr);
        int nb = recvfrom(_fd, &req_data, sizeof(req_data), 0, 
                          (struct sockaddr*)&sender_addr, &sender_addr_len);

        if (nb == -1) {
            std::cerr << "Error while reading from UDP socket" << std::endl;
        }
        else {
            uint32_t client_id = (req_data>>32);
            uint32_t chunk_id = (uint32_t)req_data;
            _req_callback(client_id, chunk_id);
        }
    }

    void can_write() {
        if (!_chunk_req_buffer.empty()) {
            auto p = _chunk_req_buffer.front();
            // uint32_t chunk_id = p.second;
            // struct sockaddr_in addr
            socklen_t len = sizeof(p.first);
            int nb = sendto(_fd, &(p.second), sizeof(p.second), 0, 
                            (struct sockaddr*)&p.first, len);

            if (nb == -1) {
                std::cerr << "Error while writing to UDP socket" << std::endl;
            }
            else {
                // datagram oriented protocol, so no worries here
                // std::cout << "Wrote " << nb << " bytes to UDP socket" << std::endl;
            }
        }
    }

    void close() {
        ::close(_fd);
    }

    uintptr_t get_fd() {
        return _fd;
    }

};