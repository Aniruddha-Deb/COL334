#pragma once

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <string>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <random>
#include <fstream>
#include <chrono>
#include <functional>

#include "event_queue.hpp"
#include "protocol.hpp"

using namespace std::literals;

using udpaddr_t = struct sockaddr_in;

class Client {

    uintptr_t _tcp_sock;
    uintptr_t _udp_sock;

    // TODO how do we claim this won't clash with any other file descriptor?
    static const uintptr_t TIMER_FD = 65535;

    std::unordered_map<uint32_t, std::chrono::time_point<std::chrono::high_resolution_clock>>
        _chunk_request_times;

    EventQueue _evt_queue;

    bool _registered = false;
    bool _saved_file = false;
    bool _can_request = false;

    uint32_t _client_id;
    uint32_t _num_chunks;
    uint32_t _num_rcvd_chunks;

    std::vector<std::shared_ptr<FileChunk>> _chunks;

    std::queue<uint32_t> _chunk_buffer;
    std::queue<uint32_t> _request_buffer;

    // chunk requests may be randomized across clients while testing
    // solution: have a random permutation of chunk id's that we'll use and then
    // sequentially go over them. If we have the chunk, then ignore. 
    std::vector<uint32_t> _req_sequence;

    size_t _next_chunk_idx;

public:

    Client(std::string server_addr, uint16_t server_port) {

        struct addrinfo* dest_addr = addr_info(server_addr, server_port, SOCK_STREAM);
        _tcp_sock = create_socket(dest_addr);

        // set socket low-water mark value for input to be the size of one file chunk
        const int sock_recv_lwm = sizeof(FileChunk);
        setsockopt(_tcp_sock, SOL_SOCKET, SO_RCVLOWAT, &sock_recv_lwm, sizeof(sock_recv_lwm));

        // connect TCP socket
        connect(_tcp_sock, dest_addr->ai_addr, dest_addr->ai_addrlen);

        // get TCP socket local address
        struct sockaddr_in src_addr;
        socklen_t len = sizeof(src_addr);
        getsockname(_tcp_sock, (struct sockaddr*)&src_addr, &len);

        // we use this to initialize UDP, as we want it to be bound at the same
        // port as TCP is
        dest_addr->ai_socktype = SOCK_DGRAM;
        _udp_sock = create_socket(dest_addr);
        bind(_udp_sock, (struct sockaddr*)&src_addr, len);

        // connect UDP socket now
        connect(_udp_sock, dest_addr->ai_addr, dest_addr->ai_addrlen);

        // can finally free dest_addr now
        freeaddrinfo(dest_addr);

        create_evt_queue();

        std::cout << "Created client and connected to server" << std::endl;
    }

    struct addrinfo* addr_info(std::string& addr, uint16_t port, int socktype) {
        struct addrinfo hints;
        struct addrinfo *res;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = socktype;
        getaddrinfo(addr.c_str(), std::to_string(port).c_str(), &hints, &res);

        return res;
    }

    uintptr_t create_socket(struct addrinfo* res) {
        uintptr_t fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd == -1) {
            std::cout << "ERROR: failed to allocate socket" << std::endl;
            return -1;
        }
        // set socket to be non-blocking
        fcntl(fd, F_SETFL, O_NONBLOCK);
        return fd;
    }

    void create_evt_queue() {
        _evt_queue.add_event(_tcp_sock, EVFILT_READ);
        _evt_queue.add_event(_tcp_sock, EVFILT_WRITE);
        _evt_queue.add_event(_udp_sock, EVFILT_READ);
        _evt_queue.add_event(_udp_sock, EVFILT_WRITE);
    }

    void init_chunk_request_sequence(bool randomize) {
        _req_sequence.resize(_num_chunks);
        _chunks.resize(_num_chunks);

        std::cout << "There are " << _num_chunks << " chunks making up the file" << std::endl;

        for (uint32_t i=0; i<_num_chunks; i++) {
            _req_sequence[i] = i;
            _chunks[i] = std::shared_ptr<FileChunk>(nullptr);
        }

        if (randomize) {
            std::random_device rd;
            std::seed_seq seed{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
            std::mt19937 rng{seed};

            std::shuffle(_req_sequence.begin(), _req_sequence.end(), rng);
        }
    }

    void save_file() {
        // TODO incorporate passing metadata in the zeroth chunk eg. name of file
        // for now, we save everything as text files
        // (definetly not a good thing)
        std::cout << "Saving file" << std::endl;
        std::string fname = "outfile_" + std::to_string(_client_id) + ".txt";
        std::ofstream f(fname);

        for (int i=0; i<_num_chunks; i++) {
            f.write(_chunks[i]->data, _chunks[i]->size);
        }

        std::cout << "Received entire file, wrote to " << fname << std::endl;        
    }

    // implementation defined
    void can_read_TCP();

    void can_read_UDP();

    void can_write_TCP();

    void can_write_UDP();

    // callbacks

    void sent_chunk(uint32_t chunk_id) {
        // do nothing for now :P
        std::cout << "Sent chunk " << chunk_id << std::endl;
    }

    void received_chunk(std::shared_ptr<FileChunk> chunk) {
        if (_chunks[chunk->id] == nullptr) {
            _chunks[chunk->id] = chunk;
            if (_chunk_request_times.find(chunk->id) != _chunk_request_times.end()) 
                _chunk_request_times.erase(chunk->id);
            _num_rcvd_chunks++;
            std::cout << "Received chunk " << chunk->id << ", have " << _num_rcvd_chunks << " chunks now." << std::endl;
        }

        if (_num_rcvd_chunks == _num_chunks and !_saved_file) {
            save_file();
            _saved_file = true;
        }
    }

    void disconnected() {
        // TODO find out where to save file, either on disconnect or somewhere else
        // is this for abnormal disconnects, or if the server bumps us off. 
        // For abnormal disconnects, we'd need to continue downloading the file
        // hmmm....
        //
        // Let's assume this is called only for abnormal disconnects, otherwise 
        // we're the ones who disconnect from the server (in general).
        // so, we'll have to reconnect to the server now and continue obtaining
        // chunks as before.

        std::cout << "Disconnected, TODO implement this" << std::endl;
    }

    void received_control(ControlMessage& m) {
        if (m.msgtype == OPEN) {
            _can_request = true;
        }
        else if (m.msgtype == REG and !_registered) {
            std::cout << "Reading registration data" << std::endl;
            _client_id = m.client_id;
            _num_chunks = m.chunk_id;
            _registered = true;

            init_chunk_request_sequence(false);
            std::cout << "registered with client_id " << _client_id << std::endl;
        }
        else if (_chunks[m.chunk_id] != nullptr) {
            send_chunk(m.chunk_id);
        }
    }

    // send requests/queue

    void request_chunk(uint32_t chunk_id) {
        _request_buffer.push(chunk_id);
    }

    void send_chunk(uint32_t chunk_id) {
        _chunk_buffer.push(chunk_id);
    }

    void periodic_resend_requests() {
        auto curr_time = std::chrono::high_resolution_clock::now();
        // for (auto p : _chunk_request_times) {
        //     std::cout << p.first << "," << (curr_time-p.second)/1ms << std::endl;
        // }

        for (auto it = _chunk_request_times.cbegin(); it != _chunk_request_times.cend(); ) {
            if ((curr_time-it->second)/1ms > 5000) {
                // re-request chunk
                _req_sequence.push_back(it->first);
                _chunk_request_times.erase(it++);
                continue;
            }     
            else {
                ++it;
            }
        }
    }

    void run(volatile bool& running) {
        _evt_queue.add_timer_event(TIMER_FD, 1000);

        while (running) {

            std::vector<event_t> evts = _evt_queue.get_events();

            for (const auto& e : evts) {
                if (e.ident == _tcp_sock) {
                    if (e.filter == EVFILT_READ) can_read_TCP();
                    if (e.filter == EVFILT_WRITE) can_write_TCP();
                }
                else if (e.ident == _udp_sock) {
                    if (e.filter == EVFILT_READ) can_read_UDP();
                    else if (e.filter == EVFILT_WRITE) can_write_UDP();
                }
                else if (e.ident == TIMER_FD and e.filter == EVFILT_TIMER) {
                    periodic_resend_requests();
                }
            }

            // have a cap of 100 so that we don't overburden the network with 
            // too many requests at once
            if (_can_request and _registered and _chunk_request_times.size() < 100) {

                // request the chunks we don't have on UDP, once in every loop
                while (_next_chunk_idx < _req_sequence.size() and 
                       _chunks[_req_sequence[_next_chunk_idx]] != nullptr) {
                    _next_chunk_idx++;
                }
                if (_next_chunk_idx < _req_sequence.size()) {
                    // get and store server udp socket id 
                    // would just be server tcp socket id + 1
                    //std::cout << "Requesting chunk " << _req_sequence[_next_chunk_idx] << std::endl;
                    //std::cout << _chunks[_req_sequence[_next_chunk_idx]] <<
                    request_chunk(_req_sequence[_next_chunk_idx]);
                    _chunk_request_times[_req_sequence[_next_chunk_idx]] = 
                        std::chrono::high_resolution_clock::now();
                    _next_chunk_idx++;
                }
                // The timer takes care of this
                // if (_next_chunk_idx >= _req_sequence.size()) {
                //     std::cout << "Requested all chunks in sequence" << std::endl;
                //     std::cout << "Exiting without receiving complete file" << std::endl;
                //     break;
                // }
            }

        }

    }

};


// class Client {

//     TCPConnection _tcp_sock;
//     UDPSocket _udp_sock;

//     // TODO how do we claim this won't clash with any other file descriptor?
//     static const uintptr_t TIMER_FD = 65535;

//     std::unordered_map<uint32_t, std::chrono::time_point<std::chrono::high_resolution_clock>>
//         _chunk_request_times;

//     EventQueue _evt_queue;

//     bool _registered = false;
//     bool _saved_file = false;
//     bool _can_request = false;
//     uint32_t _client_id;
//     uint32_t _num_chunks;

//     std::vector<std::shared_ptr<FileChunk>> _chunks;

//     // chunk requests should be randomized across clients for better speed 
//     // solution: have a random permutation of chunk id's that we'll use and then
//     // sequentially go over them. If we have the chunk, then ignore. 
//     std::vector<uint32_t> _req_sequence;
//     std::unordered_set<uint32_t> _rcvd_chunks;
//     size_t _next_chunk_idx;

// public:
//     Client(std::string server_addr, uint16_t server_port): 
//         _tcp_sock{server_addr, server_port},
//         _udp_sock{"127.0.0.1",15000,(uint16_t)ntohs(_tcp_sock.get_address().sin_port)},
//         _next_chunk_idx{0} {

//         std::cout << _tcp_sock.get_address_as_string() << std::endl;
//         std::cout << _udp_sock.get_address_as_string() << std::endl;

//         if (_tcp_sock.get_fd() == -1 or _udp_sock.get_fd() == -1) {
//             std::cout << "Could not initialize client. Sockets occupied" << std::endl;
//             exit(-1);
//         } 

//         _evt_queue.add_event(_tcp_sock.get_fd(), EVFILT_READ);
//         _evt_queue.add_event(_tcp_sock.get_fd(), EVFILT_WRITE);
//         _evt_queue.add_event(_udp_sock.get_fd(), EVFILT_READ);
//         _evt_queue.add_event(_udp_sock.get_fd(), EVFILT_WRITE);

//         _tcp_sock.on_rcv_chunk(std::bind(&Client::received_chunk,this,_1,_2));
//         _tcp_sock.on_send_chunk(std::bind(&Client::sent_chunk,this,_1,_2,_3));
//         _tcp_sock.on_disconnect(std::bind(&Client::disconnected,this,_1));
//         _udp_sock.on_chunk_request(std::bind(&Client::received_chunk_request,this,_1,_2));

//         std::cout << "Created client and connected to server" << std::endl;
//     }

//     void sent_chunk(uint32_t chunk_id, uint32_t client_id, int errcode) {
//         // if (errcode != -1)
//             // std::cout << "Sent chunk " << chunk_id << " to server" << std::endl;
//     }

//     void received_chunk(std::shared_ptr<FileChunk> chunk, int errcode) {
//         if (errcode != 0) {
//             std::cout << "Could not read chunk" << std::endl;
//             return;
//         }

//         // slot the chunk into our list of chunks, and erase from chunk requests

//     }

//     void save_file() {
//         // TODO incorporate passing metadata in the zeroth chunk eg. name of file
//         // for now, we save everything as text files
//         // (definetly not a good thing)
//         std::cout << "Saving file" << std::endl;
//         std::string fname = "outfile_" + std::to_string(_client_id) + ".txt";
//         std::ofstream f(fname);

//         for (int i=0; i<_num_chunks; i++) {
//             f.write(_chunks[i]->data, _chunks[i]->size);
//         }

//         std::cout << "Received entire file, wrote to " << fname << std::endl;
//     }

//     void received_chunk_request(uint32_t client_id, uint32_t chunk_id) {

//         if (client_id == 0xffffffff) {
//             _can_request = true;
//             return;
//         }

//         if (!_registered) {
//             // register
//             std::cout << "Reading registration data" << std::endl;
//             _client_id = client_id;
//             _num_chunks = chunk_id;
//             _registered = true;

//             // setup chunk receiving infrastructure
//             init_chunk_request_sequence();

//             std::cout << "registered with client_id " << _client_id << std::endl;
//             return;
//         }

//         // send the chunk on tcp, if we have it
//         if (_chunks[chunk_id] != nullptr) {
//             _tcp_sock.send_chunk_async(_chunks[chunk_id]);
//         }
//     }

//     void chunk_requests_allowed() {
//         _can_request = true;
//     }

//     void disconnected(uint32_t client_id) {
//         // TODO find out where to save file, either on disconnect or somewhere else
//         // is this for abnormal disconnects, or if the server bumps us off. 
//         // For abnormal disconnects, we'd need to continue downloading the file
//         // hmmm....
//         //
//         // Let's assume this is called only for abnormal disconnects, otherwise 
//         // we're the ones who disconnect from the server (in general).
//         // so, we'll have to reconnect to the server now and continue obtaining
//         // chunks as before.
//         std::cout << "Disconnected, TODO implement this" << std::endl;
//     }



//     void run(volatile bool& running) {

//         _evt_queue.add_timer_event(TIMER_FD, 1000);

//         while (running) {

//             // std::cout << "In running loop" << std::endl;
//             // this blocks! demn
//             std::vector<event_t> evts = _evt_queue.get_events();

//             for (const auto& e : evts) {
//                 if (e.ident == _tcp_sock.get_fd()) {
//                     if (e.filter == EVFILT_READ) {
//                         _tcp_sock.can_read();
//                     }
//                     else if (e.filter == EVFILT_WRITE) {
//                         _tcp_sock.can_write();
//                     }
//                 }
//                 else if (e.ident == _udp_sock.get_fd()) {
//                     if (e.filter == EVFILT_READ) {
//                         //std::cout << "Read event on UDP socket" << std::endl;
//                         _udp_sock.can_read();
//                     }
//                     else if (e.filter == EVFILT_WRITE) {
//                         //std::cout << "Write event on UDP socket" << std::endl;
//                         _udp_sock.can_write();
//                     }
//                 }
//                 else if (e.ident == TIMER_FD and e.filter == EVFILT_TIMER) {
//                     // int ccr = _req_sequence.size();
//                     // don't need num-event granularity; just requeue stale 
//                     // requests when this callback happens
//                     auto curr_time = std::chrono::high_resolution_clock::now();
//                     // for (auto p : _chunk_request_times) {
//                     //     std::cout << p.first << "," << (curr_time-p.second)/1ms << std::endl;
//                     // }

//                     for (auto it = _chunk_request_times.cbegin(); it != _chunk_request_times.cend(); ) {
//                         if ((curr_time-it->second)/1ms > 5000) {
//                             // re-request chunk
//                             //std::cout << "re-requesting chunk " << it->first << std::endl;
//                             _req_sequence.push_back(it->first);
//                             _chunk_request_times.erase(it++);
//                             continue;
//                         }     
//                         else {   // this is voodoo magic to me, pulled off StackOverflow.
//                                  // TODO learn how to delete from a map while iterating through it
//                             ++it;
//                         }
//                     }
//                     // std::cout << "After timer: " << _chunk_request_times.size() << std::endl;
//                 }
//             }

//             if (_can_request and _registered) {
//                 // now, request the chunks we don't have on UDP, once in every loop
//                 while (_next_chunk_idx < _req_sequence.size() and 
//                        _rcvd_chunks.find(_req_sequence[_next_chunk_idx]) != _rcvd_chunks.end()) {
//                     _next_chunk_idx++;
//                 }
//                 if (_next_chunk_idx < _req_sequence.size()) {
//                     // get and store server udp socket id 
//                     // would just be server tcp socket id + 1
//                     //std::cout << "Requesting chunk " << _req_sequence[_next_chunk_idx] << std::endl;
//                     //std::cout << _chunks[_req_sequence[_next_chunk_idx]] <<
//                     _udp_sock.request_chunk_async(
//                         _tcp_sock.get_remote(),
//                         _client_id,
//                         _req_sequence[_next_chunk_idx]
//                     );
//                     _chunk_request_times[_req_sequence[_next_chunk_idx]] = std::chrono::high_resolution_clock::now();
//                     _next_chunk_idx++;
//                 }
//                 // if (_next_chunk_idx >= _req_sequence.size()) {
//                 //     std::cout << "Requested all chunks in sequence" << std::endl;
//                 //     std::cout << "Exiting without receiving complete file" << std::endl;
//                 //     break;
//                 // }
//             }

//             // broadly need to do four things:
//             // 1. request chunks on UDP
//             // 2. receive chunk requests on UDP
//             // 3. receive chunks on TCP
//             // 4. send requested chunks on TCP (if we have them)

//             // keep it simple, use an event-queue and non blocking sockets.
            
//             // have a timeout if a UDP datagram drops! maybe register this timeout
//             // with the event queue and have that poll..
//         }

//         _tcp_sock.close();
//         _udp_sock.close();
//         _evt_queue.close();
//     }
// };