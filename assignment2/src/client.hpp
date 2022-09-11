#pragma once

#include <string>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <random>
#include <chrono>
#include <functional>

#include "event_queue.hpp"
#include "tcp_server_socket.hpp"
#include "udp_socket.hpp"
#include "tcp_connection.hpp"

using namespace std::placeholders;
using namespace std::literals;

using udpaddr_t = struct sockaddr_in;

class Client {

    TCPConnection _tcp_conn;
    UDPSocket _udp_sock;

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

    std::vector<std::shared_ptr<FileChunk>> _file_chunks;

    // chunk requests should be randomized across clients for better speed 
    // solution: have a random permutation of chunk id's that we'll use and then
    // sequentially go over them. If we have the chunk, then ignore. 
    std::vector<uint32_t> _chunk_req_sequence;
    std::unordered_set<uint32_t> _rcvd_chunks;
    size_t _next_chunk_idx;

public:
    Client(std::string server_addr, uint16_t server_port): 
        _tcp_conn{server_addr, server_port},
        _udp_sock{"127.0.0.1",15000,(uint16_t)ntohs(_tcp_conn.get_address().sin_port)},
        _next_chunk_idx{0} {

        std::cout << _tcp_conn.get_address_as_string() << std::endl;
        std::cout << _udp_sock.get_address_as_string() << std::endl;

        if (_tcp_conn.get_fd() == -1 or _udp_sock.get_fd() == -1) {
            std::cout << "Could not initialize client. Sockets occupied" << std::endl;
            exit(-1);
        } 

        _evt_queue.add_event(_tcp_conn.get_fd(), EVFILT_READ);
        _evt_queue.add_event(_tcp_conn.get_fd(), EVFILT_WRITE);
        _evt_queue.add_event(_udp_sock.get_fd(), EVFILT_READ);
        _evt_queue.add_event(_udp_sock.get_fd(), EVFILT_WRITE);

        _tcp_conn.on_rcv_chunk(std::bind(&Client::received_chunk,this,_1,_2));
        _tcp_conn.on_send_chunk(std::bind(&Client::sent_chunk,this,_1,_2,_3));
        _tcp_conn.on_disconnect(std::bind(&Client::disconnected,this,_1));
        _udp_sock.on_chunk_request(std::bind(&Client::received_chunk_request,this,_1,_2));

        std::cout << "Created client and connected to server" << std::endl;
    }

    void sent_chunk(uint32_t chunk_id, uint32_t client_id, int errcode) {
        if (errcode != -1)
            std::cout << "Sent chunk " << chunk_id << " to server" << std::endl;
    }

    void received_chunk(std::shared_ptr<FileChunk> chunk, int errcode) {
        if (errcode != 0) {
            std::cout << "Could not read chunk" << std::endl;
            return;
        }

        // slot the chunk into our list of chunks, and erase from chunk requests
        if (_rcvd_chunks.find(chunk->id) == _rcvd_chunks.end()) {
            _file_chunks[chunk->id] = chunk;
            if (_chunk_request_times.find(chunk->id) != _chunk_request_times.end()) 
                _chunk_request_times.erase(chunk->id);
            _rcvd_chunks.insert(chunk->id);
            std::cout << "Received chunk " << chunk->id << ", have " << _rcvd_chunks.size() << " chunks now." << std::endl;
        }

        if (_rcvd_chunks.size() == _num_chunks and !_saved_file) {
            save_file();
            _saved_file = true;
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
            f.write(_file_chunks[i]->data, _file_chunks[i]->size);
        }

        std::cout << "Received entire file, wrote to " << fname << std::endl;
    }

    void received_chunk_request(uint32_t client_id, uint32_t chunk_id) {

        if (client_id == 0xffffffff) {
            _can_request = true;
            return;
        }

        if (!_registered) {
            // register
            std::cout << "Reading registration data" << std::endl;
            _client_id = client_id;
            _num_chunks = chunk_id;
            _registered = true;

            // setup chunk receiving infrastructure
            init_chunk_request_sequence();

            std::cout << "registered with client_id " << _client_id << std::endl;
            return;
        }

        // send the chunk on tcp, if we have it
        if (_file_chunks[chunk_id] != nullptr) {
            _tcp_conn.send_chunk_async(_file_chunks[chunk_id]);
        }
    }

    void chunk_requests_allowed() {
        _can_request = true;
    }

    void disconnected(uint32_t client_id) {
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

    void init_chunk_request_sequence() {
        std::random_device rd;
        std::seed_seq seed{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
        std::mt19937 rng{seed};

        _chunk_req_sequence.resize(_num_chunks);
        _file_chunks.resize(_num_chunks);

        std::cout << "There are " << _num_chunks << " chunks making up the file" << std::endl;

        for (uint32_t i=0; i<_num_chunks; i++) {
            _chunk_req_sequence[i] = i;
            _file_chunks[i] = std::shared_ptr<FileChunk>(nullptr);
        }

        std::shuffle(_chunk_req_sequence.begin(), _chunk_req_sequence.end(), rng);
    }

    void run(volatile bool& running) {

        _evt_queue.add_timer_event(TIMER_FD, 1000);

        while (running) {

            // std::cout << "In running loop" << std::endl;
            // this blocks! demn
            std::vector<event_t> evts = _evt_queue.get_events();

            for (const auto& e : evts) {
                if (e.ident == _tcp_conn.get_fd()) {
                    if (e.filter == EVFILT_READ) {
                        _tcp_conn.can_read();
                    }
                    else if (e.filter == EVFILT_WRITE) {
                        _tcp_conn.can_write();
                    }
                }
                else if (e.ident == _udp_sock.get_fd()) {
                    if (e.filter == EVFILT_READ) {
                        //std::cout << "Read event on UDP socket" << std::endl;
                        _udp_sock.can_read();
                    }
                    else if (e.filter == EVFILT_WRITE) {
                        //std::cout << "Write event on UDP socket" << std::endl;
                        _udp_sock.can_write();
                    }
                }
                else if (e.ident == TIMER_FD and e.filter == EVFILT_TIMER) {
                    // int ccr = _chunk_req_sequence.size();
                    // don't need num-event granularity; just requeue stale 
                    // requests when this callback happens
                    auto curr_time = std::chrono::high_resolution_clock::now();
                    for (auto p : _chunk_request_times) {
                        std::cout << p.first << "," << (curr_time-p.second)/1ms << std::endl;
                    }

                    for (auto it = _chunk_request_times.cbegin(); it != _chunk_request_times.cend(); ) {
                        if ((curr_time-it->second)/1ms > 500) {
                            // re-request chunk
                            std::cout << "re-requesting chunk " << it->first << std::endl;
                            _chunk_req_sequence.push_back(it->first);
                            _chunk_request_times.erase(it++);
                            continue;
                        }     
                        else {   // this is voodoo magic to me, pulled off StackOverflow.
                                 // TODO learn how to delete from a map while iterating through it
                            ++it;
                        }
                    }
                    // std::cout << "After timer: " << _chunk_request_times.size() << std::endl;
                }
            }

            if (_can_request and _registered) {
                // now, request the chunks we don't have on UDP, once in every loop
                while (_next_chunk_idx < _chunk_req_sequence.size() and 
                       _rcvd_chunks.find(_chunk_req_sequence[_next_chunk_idx]) != _rcvd_chunks.end()) {
                    _next_chunk_idx++;
                }
                if (_next_chunk_idx < _chunk_req_sequence.size()) {
                    // get and store server udp socket id 
                    // would just be server tcp socket id + 1
                    std::cout << "Requesting chunk " << _chunk_req_sequence[_next_chunk_idx] << std::endl;
                    //std::cout << _file_chunks[_chunk_req_sequence[_next_chunk_idx]] <<
                    _udp_sock.request_chunk_async(
                        _tcp_conn.get_remote(),
                        _client_id,
                        _chunk_req_sequence[_next_chunk_idx]
                    );
                    _chunk_request_times[_chunk_req_sequence[_next_chunk_idx]] = std::chrono::high_resolution_clock::now();
                    _next_chunk_idx++;
                }
                // if (_next_chunk_idx >= _chunk_req_sequence.size()) {
                //     std::cout << "Requested all chunks in sequence" << std::endl;
                //     std::cout << "Exiting without receiving complete file" << std::endl;
                //     break;
                // }
            }

            // broadly need to do four things:
            // 1. request chunks on UDP
            // 2. receive chunk requests on UDP
            // 3. receive chunks on TCP
            // 4. send requested chunks on TCP (if we have them)

            // keep it simple, use an event-queue and non blocking sockets.
            
            // have a timeout if a UDP datagram drops! maybe register this timeout
            // with the event queue and have that poll..
        }

        _tcp_conn.close();
        _udp_sock.close();
        _evt_queue.close();
    }
};