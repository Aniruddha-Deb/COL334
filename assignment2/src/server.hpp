#pragma once

#include <string>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <functional>

#include "event_queue.hpp"
#include "tcp_server_socket.hpp"
#include "udp_socket.hpp"
#include "tcp_connection.hpp"
#include "lru_cache.hpp"

using namespace std::placeholders;

using udpaddr_t = struct sockaddr_in;

class Server {

    EventQueue _evt_queue;

    // we need a TCP server socket for client->server chunk transfer
    TCPServerSocket _tcp_ss;

    // and a set of TCP/UDP connections for writing chunks from server->client

    // collections can't store classes. They can store pointers (shared). 
    // Solution: Just have one TCPConnection map, and let others index into that 
    // we can use unique pointers in that case
    std::unordered_map<uintptr_t,std::unique_ptr<TCPConnection>> _fd_map;

    // ? what's the most optimal way to store client UDP map data
    // eh, let's just store it in a typedef'd sockaddr_in
    std::unordered_map<uint32_t,udpaddr_t> _client_udp_map; 
    std::unordered_map<uint32_t,uintptr_t> _client_tcp_map;

    // this is like a relation between client_id and chunk_id, allowing 
    // amortized O(1) relation access, update and deletion
    std::unordered_map<uint32_t,std::unordered_set<uint32_t>> _chunk_requests;
    std::unordered_map<uint32_t,std::unordered_set<uint32_t>> _client_requests;

    // and a UDP socket for receiving/transmitting requests
    UDPSocket _udp_ss;

    // when clients connect, each client needs to be sent a unique ID. This 
    // is a way of 'connecting' their TCP and UDP ports together, as we can't 
    // obtain the sending client's TCP port from their UDP request, without 
    // having very messy (address,port) maps. 
    // 
    // Before they're added to the server core, we store them in a pending queue
    std::unordered_map<uintptr_t,std::unique_ptr<TCPConnection>> _clients_pending_registration;
    uint32_t _next_client_id;

    uint32_t _tot_chunks;
    uint32_t _min_clients;
    bool _distributed_all_chunks = false;
    bool _informed_clients = false;

    // LRU Cache to cache some blocks
    LRUCache<uint32_t,std::shared_ptr<FileChunk>> _chunk_cache;

    // initial file chunk list
    std::queue<std::shared_ptr<FileChunk>> _file_chunks_to_distribute;
    std::unordered_set<uint32_t> _file_chunks_being_distributed;
    std::unordered_set<uint32_t> _file_chunks_distributed;

public:

    Server(uint16_t port, uint32_t min_clients, size_t chunk_cache_size): 
        _tcp_ss{port},
        _udp_ss{port},
        _next_client_id{0},
        _min_clients{min_clients},
        _chunk_cache{chunk_cache_size} {

        _evt_queue.add_event(_tcp_ss.get_fd(), EVFILT_READ);
        _evt_queue.add_event(_udp_ss.get_fd(), EVFILT_READ);
        _evt_queue.add_event(_udp_ss.get_fd(), EVFILT_WRITE);

        _udp_ss.on_chunk_request(std::bind(&Server::received_chunk_request,this,_1,_2));

        std::cout << "Created server" << std::endl;

    }

    void load_file(std::string filepath) {
        // the server will equally distribute the 1kb chunks among min_clients 
        // (The first n clients who join the server). once min_clients connect 
        // to the server, the server will delete the file and all transactions
        // taking place after that will be PSP
        
        _file_chunks_to_distribute = read_and_chunk_file(filepath);
        _tot_chunks = _file_chunks_to_distribute.size();

        std::cout << "Loaded file" << std::endl;
    }

    void sent_chunk(uint32_t chunk_id, uint32_t client_id, int errcode) {

        if (!_distributed_all_chunks) {
            if (_file_chunks_being_distributed.find(chunk_id) != _file_chunks_being_distributed.end()) {
                _file_chunks_being_distributed.erase(chunk_id);
                _file_chunks_distributed.insert(chunk_id);
                if (_file_chunks_distributed.size() == _tot_chunks) {
                    _distributed_all_chunks = true;
                    std::cout << "Distributed all chunks" << std::endl;
                }
            }
        }

        if (errcode != 0) {
            std::cout << "Could not write chunk" << std::endl;
            return;
        }
        _chunk_requests[chunk_id].erase(client_id);
        _client_requests[client_id].erase(chunk_id);
    }

    void client_disconnected(uint32_t client_id) {
        // disconnect client_id from everything
        std::cout << "Disconnecting client with id " << client_id << std::endl;
        
        for (auto chunk_id : _client_requests[client_id]) {
            _chunk_requests[chunk_id].erase(client_id);
        }

        _client_requests.erase(client_id);
        _client_udp_map.erase(client_id);
        _evt_queue.delete_event(_client_tcp_map[client_id], EVFILT_READ);
        _evt_queue.delete_event(_client_tcp_map[client_id], EVFILT_WRITE);
        // _fd_map[_client_tcp_map[client_id]]->close();
        _fd_map.erase(_client_tcp_map[client_id]);
        _client_tcp_map.erase(client_id);
    }

    void received_chunk(std::shared_ptr<FileChunk> chunk, int errcode) {
        if (errcode != 0) {
            std::cout << "Could not read chunk" << std::endl;
            return;
        }

        // serve the people who needed the chunk first
        if (_chunk_requests.find(chunk->id) != _chunk_requests.end()) {
            for (auto c : _chunk_requests[chunk->id]) {
                _fd_map[_client_tcp_map[c]]->send_chunk_async(chunk);
            }
        }

        // then store the chunk in the LRU cache (if it doesn't already exist)
        if (_chunk_cache.access(chunk->id) == nullptr) {
            _chunk_cache.insert(chunk->id, chunk);
        }
    }

    void received_chunk_request(uint32_t client_id, uint32_t chunk_id) {
        // check if chunk is in LRU cache first
        std::shared_ptr<FileChunk> chunk = _chunk_cache.access(chunk_id);
        if (chunk != nullptr) {
            _fd_map[_client_tcp_map[client_id]]->send_chunk_async(chunk);
        }
        else {
            // request the chunk
            if (_chunk_requests.find(chunk_id) != _chunk_requests.end()) {
                _chunk_requests[chunk_id].insert(client_id);
            }
            else {
                std::unordered_set<uint32_t> v;
                v.insert(client_id);
                _chunk_requests[chunk_id] = v;
            }

            _client_requests[client_id].insert(chunk_id); // guaranteed to exist

            for (auto p : _client_udp_map) {
                if (p.first != client_id) {
                    _udp_ss.request_chunk_async(p.second,client_id,chunk_id);
                }
            }
        }
    }

    // by convention, assume that udp_socket on client is tcp_socket+1
    static udpaddr_t tcp_to_udp_addr(struct sockaddr_in addr) {
        return addr;
    }

    void run(volatile bool& running) {

        _tcp_ss.listen();

        while (running) {

            // TODO this throws an exception when I terminate
            // libc++abi.dylib: terminating with uncaught exception of type std::length_error: vector
            // maybe return std::unique_ptr<event_t> instead of raw copies? 
            // I don't even know how (if) move semantics are working here
            std::vector<event_t> evts = _evt_queue.get_events();

            if (_distributed_all_chunks and !_informed_clients) {
                for (auto& p : _client_udp_map) {
                    _udp_ss.request_chunk_async(p.second, 0xffffffff, 0);
                }
                _informed_clients = true;
                std::cout << "Informed all clients that chunks were distributed" << std::endl;
            }

            for (const auto& e : evts) {
                if (e.ident == _tcp_ss.get_fd()) {
                    std::cout << "Received connection, assigning id " << _next_client_id << std::endl;
                    auto conn = _tcp_ss.accept(_next_client_id++);
                    _evt_queue.add_event(conn->get_fd(), EVFILT_READ);
                    _evt_queue.add_event(conn->get_fd(), EVFILT_WRITE);
                    _clients_pending_registration.emplace(conn->get_fd(), std::move(conn));

                }
                else if (_clients_pending_registration.find(e.ident) != _clients_pending_registration.end() and 
                         e.filter == EVFILT_WRITE) {
                    // send over registration data
                    std::cout << "Sending registration data to client" << std::endl;
                    auto conn = std::move(_clients_pending_registration[e.ident]);
                    int status = conn->send_regdata_sync(conn->get_client_id(), _tot_chunks);
                    std::cout << status << std::endl;
                    if (status == -1) {
                        // do errno check
                    }
                    else {
                        // check if it's less than 8 
                    }

                    std::cout << "Sent registration data" << std::endl;
                    _client_tcp_map[conn->get_client_id()] = conn->get_fd();
                    _client_udp_map[conn->get_client_id()] = conn->get_address();
                    _client_requests.emplace(conn->get_client_id(), std::unordered_set<uint32_t>());
                    conn->on_rcv_chunk(std::bind(&Server::received_chunk,this,_1,_2));
                    conn->on_send_chunk(std::bind(&Server::sent_chunk,this,_1,_2,_3));
                    conn->on_disconnect(std::bind(&Server::client_disconnected,this,_1));

                    // need to send initial chunks now
                    if (_distributed_all_chunks) {
                        _udp_ss.request_chunk_async(_client_udp_map[conn->get_client_id()], 0xffffffff, 0);
                    }
                    else {
                        int ctr = 0;
                        int chunk_lim = 1 + ((_tot_chunks-1)/_min_clients);
                        while (!_file_chunks_to_distribute.empty() and ctr < chunk_lim) {
                            conn->send_chunk_async(_file_chunks_to_distribute.front());
                            _file_chunks_being_distributed.insert(_file_chunks_to_distribute.front()->id);
                            _file_chunks_to_distribute.pop();
                            ctr++;
                        }
                    }

                    _fd_map.emplace(conn->get_fd(), std::move(conn));
                    _clients_pending_registration.erase(e.ident);

                    std::cout << "Registered client" << std::endl;
                }
                else if (_fd_map.find(e.ident) != _fd_map.end()) {
                    // auto conn = _fd_map[e.ident];
                    //std::cout << "event in thing" << std::endl;
                    if (e.filter == EVFILT_READ) {
                        //std::cout << "reading" << std::endl;
                        _fd_map[e.ident]->can_read();
                        //std::cout << "read" << std::endl;
                    }
                    else if (e.filter == EVFILT_WRITE) {
                        //std::cout << "writing" << std::endl;
                        _fd_map[e.ident]->can_write();
                        //std::cout << "wrote" << std::endl;
                    }
                }
                else if (e.ident == _udp_ss.get_fd()) {
                    if (e.filter == EVFILT_READ) {
                        // UDP request for packet
                        _udp_ss.can_read();
                    }
                    else if (e.filter == EVFILT_WRITE) {
                        _udp_ss.can_write();
                    }
                }
                // else if (e.flags & EV_EOF) {
                //     client_disconnected()
                // }
            }
        }

        std::cout << "Terminating" << std::endl;
        // for now, just shutdown all file descriptors and deregister them from 
        // the event queue

        _tcp_ss.close();
        _udp_ss.close();
        for (const auto& p : _fd_map) {
            p.second->close();
        }

        _evt_queue.close();

        // should we clear out the queues?
        // yeah, would be a good practice..
        // TODO clear out queues

    }
};