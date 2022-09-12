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

#include "client_connection.hpp"
#include "protocol.hpp"

// this implementation uses a UDP control layer over a TCP data layer

ClientConnection::ClientConnection(
    uint32_t client_id, uintptr_t tcp_fd, uintptr_t udp_fd, struct sockaddr_in client_addr):
    _client_id{client_id},
    _tcp_fd{tcp_fd},
    _udp_fd{udp_fd} {

    _client_addr = client_addr;
}

void ClientConnection::can_write_TCP() {
    if (!_chunk_buffer.empty()) {
        auto p = _chunk_buffer.front();
        ssize_t nb = send(_tcp_fd, p.get(), sizeof(FileChunk), 0);
        if (nb == -1) {
            std::cerr << "Error while writing to TCP socket " << get_addr_str() << " (errno " << errno << ")" << std::endl;
        }
        else {
            _chunk_buffer.pop();
            _send_chunk_callback(_client_id, p->id);
        }
    }
}

void ClientConnection::can_read_TCP() {
    std::shared_ptr<FileChunk> c(new FileChunk);
    ssize_t nb = recv(_tcp_fd, c.get(), sizeof(FileChunk), 0);

    if (nb == -1) {
        std::cerr << "Error while reading from TCP socket " << get_addr_str() << " (errno " << errno << ")" << std::endl;
    }
    else if (nb == 0) {
        // this happens when the socket has disconnected
        // have a callback that detatches this socket from everything
        close_client();
    }
    else {
        // handle the event where we don't read a complete filechunk in...
        // actually, such an event won't come up due to the low-water level
        // we've set.
        _recv_chunk_callback(c);
    }
}

void ClientConnection::can_write_UDP() {
    if (!_control_msg_buffer.empty()) {
        auto p = _control_msg_buffer.front(); // not worrying about network byte order for now.
        socklen_t len = sizeof(_client_addr);
        int nb = sendto(_udp_fd, &p, sizeof(p), 0, 
                        (struct sockaddr*)&_client_addr, len);

        if (nb == -1) {
            std::cerr << "Error while writing to UDP socket (err " << errno << ")" << std::endl;
        }
        else {
            // datagram oriented protocol, so no worries here
            //std::cout << "Wrote " << data << " (" << nb << " bytes) to UDP socket " << ntohs(p.first.sin_port) << std::endl;
            _control_msg_buffer.pop();
        }
    }
}

    void ClientConnection::can_read_UDP() {
        ControlMessage req_data;
        // struct sockaddr_in sender_addr;
        // socklen_t sender_addr_len = sizeof(sender_addr);
        // let's see if this poses problems
        int nb = recvfrom(_udp_fd, &req_data, sizeof(req_data), 0, 
                          (struct sockaddr*)&sender_addr, &sender_addr_len);

        if (nb == -1) {
            std::cerr << "Error while reading from UDP socket" << std::endl;
        }
        else {
            //std::cout << "Read " << req_data << " (" << nb << " bytes) from UDP socket" << std::endl;
            uint32_t client_id = (uint32_t)(req_data>>32);
            uint32_t chunk_id = (uint32_t)req_data;
            _recv_chunk_req_callback(client_id, chunk_id);
        }
    }