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
#include "tcp_connection.hpp"

extern volatile bool running;

class UDPSocket {

    uintptr_t _fd;
    struct sockaddr_in _address;

    std::function<void(uint32_t,uint32_t)> _req_callback;
    std::function<void(void)> _allow_req_callback;
    std::queue<std::pair<struct sockaddr_in,std::pair<uint32_t,uint32_t>>> _chunk_req_buffer;

public:

    // need to have two constructors: one for a client socket and one for a server
    // socket
    // for a client one, we'll need to pass in both the destination IP and the
    // destination port, not the current port (although we can bind it to the 
    // current port onli)

    UDPSocket(std::string dest_ip, uint16_t dest_port, uint16_t src_port) {
        struct addrinfo hints;
        struct addrinfo *res;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        // hints.ai_flags = AI_PASSIVE; // fill in IP for us
        // TODO this can also return an error. Error handling here
        getaddrinfo(dest_ip.c_str(), std::to_string(dest_port).c_str(), &hints, &res);

        _fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (_fd == -1) {
            std::cout << "ERROR: failed to allocate UDP socket to " << dest_ip << ":" << dest_port << std::endl;
            return;
        }

        // set socket to be non-blocking
        fcntl(_fd, F_SETFL, O_NONBLOCK);

        // bind socket to src_port
        struct addrinfo* local;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;
        getaddrinfo(nullptr, std::to_string(src_port).c_str(), &hints, &local);
        bind(_fd, local->ai_addr, local->ai_addrlen);

        // not really required for a datagram socket
        // connect(_fd, res->ai_addr, res->ai_addrlen);

        socklen_t len = sizeof(_address);
        getsockname(_fd, (struct sockaddr*)&_address, &len);

        std::cout << "Created UDP socket, bound to " << get_address_as_string() << std::endl;

        // set socket low-water mark value for input to be 8 bytes
        // (ID of chunk we're requesting)
        // Does this matter in a datagram-oriented protocol?
        // const int sock_recv_lwm = sizeof(uint64_t);
        // setsockopt(_fd, SOL_SOCKET, SO_RCVLOWAT, &sock_recv_lwm, sizeof(sock_recv_lwm));

        freeaddrinfo(res);
        freeaddrinfo(local);
    }

    UDPSocket(uint16_t port) {
        std::cout << "Creating UDP socket at port " << port << std::endl;
        struct addrinfo hints;
        struct addrinfo *res;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE; // fill in IP for us
        // TODO this can also return an error. Error handling here
        getaddrinfo(nullptr, std::to_string(port).c_str(), &hints, &res);

        _fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (_fd == -1) {
            std::cout << "ERROR: failed to allocate UDP socket on port " << port << std::endl;
            return;
        }

        // set socket to be non-blocking
        fcntl(_fd, F_SETFL, O_NONBLOCK);

        bind(_fd, res->ai_addr, res->ai_addrlen);

        socklen_t len = sizeof(_address);
        getsockname(_fd, (struct sockaddr*)&_address, &len);

        std::cout << "Created UDP socket, bound to " << get_address_as_string() << std::endl;

        // set socket low-water mark value for input to be 8 bytes
        // (ID of chunk we're requesting)
        // Does this matter in a datagram-oriented protocol?
        // const int sock_recv_lwm = sizeof(uint64_t);
        // setsockopt(_fd, SOL_SOCKET, SO_RCVLOWAT, &sock_recv_lwm, sizeof(sock_recv_lwm));

        freeaddrinfo(res);
    }

    std::string get_address_as_string() {
        char s[INET6_ADDRSTRLEN];
        inet_ntop(_address.sin_family, &(_address.sin_addr), s, sizeof s);
        std::string retval(s);
        retval += ":" + std::to_string(ntohs(_address.sin_port));
        return retval;
    }


    void on_chunk_request(const std::function<void(uint32_t,uint32_t)> callback) {
        _req_callback = callback;
    }

    void request_chunk_async(const struct sockaddr_in addr, const uint32_t client_id, const uint32_t chunk_id) {
        _chunk_req_buffer.push({addr,{client_id,chunk_id}});
    }

    void on_allow_requests(const std::function<void(void)> callback) {
        _allow_req_callback = callback;
    }

    void can_read() {
        //std::cout << "Read is being called on UDP socket" << std::endl;
        uint64_t req_data;
        struct sockaddr_in sender_addr;
        socklen_t sender_addr_len = sizeof(sender_addr);
        int nb = recvfrom(_fd, &req_data, sizeof(req_data), 0, 
                          (struct sockaddr*)&sender_addr, &sender_addr_len);

        if (nb == -1) {
            std::cerr << "Error while reading from UDP socket" << std::endl;
        }
        else {
            //std::cout << "Read " << req_data << " (" << nb << " bytes) from UDP socket" << std::endl;
            uint32_t client_id = (uint32_t)(req_data>>32);
            uint32_t chunk_id = (uint32_t)req_data;
            if (client_id == 0xffffffff) {
                _allow_req_callback();
            }
            else {
                _req_callback(client_id, chunk_id);
            }
        }
    }

    void can_write() {
        if (!_chunk_req_buffer.empty()) {
            auto p = _chunk_req_buffer.front();
            uint64_t data = (((uint64_t)p.second.first)<<32)|(p.second.second);
            // struct sockaddr_in addr
            socklen_t len = sizeof(p.first);
            int nb = sendto(_fd, &data, sizeof(data), 0, 
                            (struct sockaddr*)&p.first, len);

            if (nb == -1) {
                std::cerr << "Error while writing to UDP socket (err " << errno << ")" << std::endl;
            }
            else {
                // datagram oriented protocol, so no worries here
                //std::cout << "Wrote " << data << " (" << nb << " bytes) to UDP socket " << ntohs(p.first.sin_port) << std::endl;
                _chunk_req_buffer.pop();
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