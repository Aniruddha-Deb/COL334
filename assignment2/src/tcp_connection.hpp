#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <queue>
#include <iostream>

#include "chunk.hpp"

extern volatile bool running;

class TCPConnection {

    uintptr_t _fd;
    uintptr_t _client_id;
    struct sockaddr_in _address;
    struct sockaddr_in _remote;
    std::queue<std::shared_ptr<FileChunk>> _send_chunk_buffer;

    std::function<void(std::shared_ptr<FileChunk>,int)> _rcv_callback;
    std::function<void(uint32_t,uint32_t,int)> _send_callback;
    std::function<void(uint32_t)> _disconnect_callback;

    uint16_t _port;

public:

    TCPConnection(std::string dest_addr, uint16_t dest_port) {
        struct addrinfo hints;
        struct addrinfo *res;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        getaddrinfo(dest_addr.c_str(), std::to_string(dest_port).c_str(), &hints, &res);

        memcpy(&_remote, res->ai_addr, res->ai_addrlen);

        _fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (_fd == -1) {
            std::cout << "ERROR: failed to allocate socket for dest_addr " << dest_addr << ":" << dest_port << std::endl;
            return;
        }

        // set socket to be non-blocking
        fcntl(_fd, F_SETFL, O_NONBLOCK);

        // set socket low-water mark value for input to be the size of one file chunk
        const int sock_recv_lwm = sizeof(FileChunk);
        setsockopt(_fd, SOL_SOCKET, SO_RCVLOWAT, &sock_recv_lwm, sizeof(sock_recv_lwm));

        ::connect(_fd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);

        // This incorporates binding logic that only binds to port n if port n+1
        // is empty (can connect UDP socket to n+1 then)

        // hints.ai_flags = AI_PASSIVE;
        // struct addrinfo *local;
        // uint16_t src_port = 15002;
        // getaddrinfo(nullptr, std::to_string(src_port).c_str(), &hints, &local);
        // int err = 0;
        // do {
        //     getaddrinfo(nullptr, std::to_string(src_port+1).c_str(), &hints, &local);
        //     err = bind(_fd, local->ai_addr, local->ai_addrlen);
        //     if (err == -1) {
        //         src_port += 2;
        //         std::cout << "Could not use port pair " << src_port << "," << src_port+1 << ". Continuing search" << std::endl;
        //         continue;
        //     }

        //     err = bind(_fd, local->ai_addr, local->ai_addrlen);
            
        //     if (err == -1) {
        //         src_port += 2;
        //         std::cout << "Could not use port pair " << src_port << "," << src_port+1 << ". Continuing search" << std::endl;
        //         continue;
        //     }
        // } while (err != 0);

        socklen_t len = sizeof(_address);
        getsockname(_fd, (struct sockaddr*)&_address, &len);

        // _dest = res;
        // freeaddrinfo(local);
    }

    TCPConnection(uintptr_t fd, uint32_t client_id, struct sockaddr_in address): 
        _fd{fd}, _client_id{client_id}, _address{address} {
        // set socket to be non-blocking
        fcntl(fd, F_SETFL, O_NONBLOCK);

        // set socket low-water mark value for input to be the size of one file chunk
        const int sock_recv_lwm = sizeof(FileChunk);
        setsockopt(fd, SOL_SOCKET, SO_RCVLOWAT, &sock_recv_lwm, sizeof(sock_recv_lwm));
        // we don't have to do something similar for send, as that would slow down 
        // the speed at which we're sending. Just having this at one end is okay

        // socket buffer sizes are generally big enough (on my mac, it's 128 kB) for
        // buffer overflows to be a non-issue, so we don't tweak the default buffer
        // size

        // The sequentiality of TCP guarantees that we're always reading a contiguous
        // chunk sent by client/server. So we don't need a protocol indicating length 
        // or anything of that sort; just chunk into 1024 bytes while reading.

        // timeouts are also ok; let's not change those.
    }

    int send_regdata_sync(uint32_t client_id, uint32_t num_chunks) {
        uint64_t data = (((uint64_t)client_id)<<32) | num_chunks;
        return send(_fd, &data, sizeof(data), 0);
    }

    uint64_t recv_regdata_sync() {
        uint64_t data;
        recv(_fd, &data, sizeof(data), 0);
        return data;
    }


    void send_chunk_async(std::shared_ptr<FileChunk> f) {
        _send_chunk_buffer.push(f);
    }

    void on_rcv_chunk(std::function<void(std::shared_ptr<FileChunk>,int)> callback) {
        _rcv_callback = callback;
    }

    void on_send_chunk(std::function<void(uint32_t,uint32_t,int)> callback) {
        _send_callback = callback;
    }

    void on_disconnect(std::function<void(uint32_t)> callback) {
        _disconnect_callback = callback;
    }


    std::string get_address_as_string() {
        char s[INET6_ADDRSTRLEN];
        inet_ntop(_address.sin_family, &(_address.sin_addr), s, sizeof s);
        std::string retval(s);
        retval += ":" + std::to_string(ntohs(_address.sin_port));
        return retval;
    }

    void can_write() {
        // TODO clear out send buffer here?
        if (!_send_chunk_buffer.empty()) {
            auto p = _send_chunk_buffer.front();
            ssize_t nb = send(_fd, p.get(), sizeof(FileChunk), 0);
            if (nb == -1) {
                std::cerr << "Error while writing to TCP socket " << get_address_as_string() << std::endl;
                _send_callback(0, 0, errno);
            }
            else {
                // std::cout << "sent " << nb << " bytes to TCP socket " << get_address_as_string() << std::endl;
                _send_chunk_buffer.pop();
                _send_callback(p->id, _client_id, 0);
            }
        }
    }

    void can_read() {
        std::shared_ptr<FileChunk> c(new FileChunk);
        ssize_t nb = recv(_fd, c.get(), sizeof(FileChunk), 0);

        if (nb == -1) {
            std::cerr << "Error while reading from TCP socket " << get_address_as_string() << std::endl;
            _rcv_callback(nullptr, errno);
        }
        else if (nb == 0) {
            // this happens when the socket has disconnected
            // have a callback that detatches this socket from everything
            _disconnect_callback(_client_id);
            close();
        }
        else {
            // handle the event where we don't read a complete filechunk in...
            // actually, such an event won't come up due to the low-water level
            // we've set.
            // std::cout << "read " << nb << " bytes from TCP socket " << get_address_as_string() << std::endl;
            _rcv_callback(c,0);
        }
    }

    void close() {
        ::close(_fd);
    }

    uintptr_t get_fd() {
        return _fd;
    }

    uintptr_t get_client_id() {
        return _client_id;
    }

    struct sockaddr_in get_address() {
        return _address;
    }

    struct sockaddr_in get_remote() {
        return _remote;
    }

};