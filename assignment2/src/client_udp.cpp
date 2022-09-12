// this uses a 

#include <sys/socket.h>
#include <errno.h>
#include "client.hpp"

void Client::can_read_TCP() {
    std::shared_ptr<FileChunk> c(new FileChunk);
    ssize_t nb = recv(_tcp_sock, c.get(), sizeof(FileChunk), 0);

    if (nb == -1) {
        std::cerr << "Error while reading from TCP socket (errno " << errno << ")" << std::endl;
    }
    else if (nb == 0) {
        // this happens when the socket has disconnected
        // have a callback that detatches this socket from everything
        disconnected();
    }
    else {
        // handle the event where we don't read a complete filechunk in...
        // actually, such an event won't come up due to the low-water level
        // we've set.
        received_chunk(c);
    }
}

void Client::can_read_UDP() {

    ControlMessage req_data;
    int nb = recvfrom(_udp_sock, &req_data, sizeof(req_data), 0, nullptr, 0);

    if (nb == -1) {
        std::cerr << "Error while reading from UDP socket (errno " << errno << ")" << std::endl;
    }
    else {
        received_control(req_data);
    }
}

void Client::can_write_TCP() {
    if (!_chunk_buffer.empty()) {
        auto p = _chunk_buffer.front();
        ssize_t nb = send(_tcp_sock, p.get(), sizeof(FileChunk), 0);
        if (nb == -1) {
            std::cerr << "Error while writing to TCP socket (errno " << errno << ")" << std::endl;
        }
        else {
            _chunk_buffer.pop();
            sent_chunk(p->id);
        }
    }
}

void Client::can_write_UDP() {
    if (!_request_buffer.empty()) {
        uint32_t chunk_id = _request_buffer.front(); // not worrying about network byte order for now.
        ControlMessage c{REQ, _client_id, chunk_id};
        int nb = sendto(_udp_sock, &c, sizeof(c), 0, nullptr, 0);

        if (nb == -1) {
            std::cerr << "Error while writing to UDP socket (err " << errno << ")" << std::endl;
        }
        else {
            // datagram oriented protocol, so no worries here
            //std::cout << "Wrote " << data << " (" << nb << " bytes) to UDP socket " << ntohs(p.first.sin_port) << std::endl;
            _request_buffer.pop();
        }
    }
}
