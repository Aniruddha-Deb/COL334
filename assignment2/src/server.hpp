#pragma once

#include <string>
#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <fcntl.h>
#include <fstream>
#include <functional>

#include "event_queue.hpp"
#include "client_connection.hpp"
#include "protocol.hpp"
#include "lru_cache.hpp"

using namespace std::placeholders;

class Server {

    // client ID = file descriptor
    std::unordered_map<uint32_t,std::unique_ptr<ClientConnection>> _clients;

    uintptr_t _tcp_ss;
    uintptr_t _udp_ss;

    uint32_t _min_clients;

    LRUCache<uint32_t,std::shared_ptr<FileChunk>> _chunk_cache;
    EventQueue _evt_queue;

    uint32_t _tot_chunks;
    bool _distributed_all_chunks = false;
    bool _requests_open = false;

    std::unordered_map<uint32_t,std::unordered_set<uint32_t>> _chunk_requests;
    std::unordered_map<uint32_t,std::unordered_set<uint32_t>> _client_requests;

    // initial file chunk list
    std::queue<std::shared_ptr<FileChunk>> _chunks_to_distribute;
    std::unordered_set<uint32_t> _chunks_being_distributed;
    std::unordered_set<uint32_t> _chunks_distributed;


public:

    Server(uint16_t port, uint32_t min_clients, size_t chunk_cache_size):
        _min_clients{min_clients},
        _chunk_cache{chunk_cache_size} {

        _tcp_ss = create_server_socket(port, SOCK_STREAM);
        _udp_ss = create_server_socket(port, SOCK_DGRAM);

        _evt_queue.add_event(_tcp_ss, EVFILT_READ);
        _evt_queue.add_event(_udp_ss, EVFILT_READ);
        _evt_queue.add_event(_udp_ss, EVFILT_WRITE);

    }

    uintptr_t create_server_socket(uint16_t port, int type) {
        struct addrinfo hints;
        struct addrinfo *res;

        memset(&hints, 0, sizeof hints);

        hints.ai_family = AF_INET;
        hints.ai_socktype = type;
        hints.ai_flags = AI_PASSIVE; // fill in IP for us
        // TODO this can also return an error. Error handling here
        getaddrinfo(nullptr, std::to_string(port).c_str(), &hints, &res);

        uintptr_t fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd == -1) {
            std::cout << "ERROR: failed to allocate UDP socket on port " << port << std::endl;
            return -1;
        }
        // set socket to be non-blocking
        fcntl(fd, F_SETFL, O_NONBLOCK);
        bind(fd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);

        return fd;
    }

    void load_file(std::string filepath) {
        // the server will equally distribute the 1kb chunks among min_clients 
        // (The first n clients who join the server). once min_clients connect 
        // to the server, the server will delete the file and all transactions
        // taking place after that will be PSP

        std::ifstream infile(filepath);

        uint32_t id_ctr = 0;

        while (!infile.eof()) {
            std::shared_ptr<FileChunk> fptr(new FileChunk);
            fptr->id = id_ctr;
            infile.read(fptr->data, 1024);
            fptr->size = infile.gcount();
            id_ctr++;

            _chunks_to_distribute.push(fptr);
            // std::cout << "Read chunk of " << fptr->size << " bytes from file" << std::endl;
        }
        
        _tot_chunks = _chunks_to_distribute.size();

        std::cout << "Loaded " << _tot_chunks << " from file " << filepath << std::endl;
    }

    void open_requests() {
        for (auto& p : _clients) {
            p.second->send_control_msg({OPEN,0,0});
        }
        _requests_open = true;
        std::cout << "Informed all clients that chunks were distributed" << std::endl;
    }

    void sent_chunk(uint32_t client_id, uint32_t chunk_id) {

        if (!_distributed_all_chunks) {
            if (_chunks_being_distributed.find(chunk_id) != _chunks_being_distributed.end()) {
                _chunks_being_distributed.erase(chunk_id);
                _chunks_distributed.insert(chunk_id);
                if (_chunks_distributed.size() == _tot_chunks) {
                    _distributed_all_chunks = true;
                    std::cout << "Distributed all chunks" << std::endl;
                }
            }
        }

        _chunk_requests[chunk_id].erase(client_id);
        _client_requests[client_id].erase(chunk_id);
    }

    void deregister_from_queue(uint32_t tcp_fd_handle) {
        _evt_queue.delete_event(tcp_fd_handle, EVFILT_READ);
        _evt_queue.delete_event(tcp_fd_handle, EVFILT_WRITE);
    }

    void register_to_queue(uint32_t tcp_fd_handle) {
        _evt_queue.add_event(tcp_fd_handle, EVFILT_READ);
        _evt_queue.add_event(tcp_fd_handle, EVFILT_WRITE);
    }


    void client_disconnected(uint32_t client_id) {
        // disconnect client_id from everything
        std::cout << "Disconnecting client with id " << client_id << std::endl;
        for (auto chunk_id : _client_requests[client_id]) {
            _chunk_requests[chunk_id].erase(client_id);
        }
        _client_requests.erase(client_id);
        deregister_from_queue(_clients[client_id]->get_tcp_fd());
        _clients.erase(client_id);
    }

    void received_chunk(std::shared_ptr<FileChunk> chunk) {

        // serve the people who needed the chunk first
        if (_chunk_requests.find(chunk->id) != _chunk_requests.end()) {
            for (auto c : _chunk_requests[chunk->id]) {
                _clients[c]->send_chunk(chunk);
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
            _clients[client_id]->send_chunk(chunk);
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

            for (auto& p : _clients) {
                if (p.first != client_id) {
                    p.second->req_chunk(chunk_id);
                }
            }
        }
    }

    void bind_callbacks_to_client(std::unique_ptr<ClientConnection>& conn) {
        conn->on_recv_chunk(std::bind(&Server::received_chunk,this,_1));
        conn->on_send_chunk(std::bind(&Server::sent_chunk,this,_1,_2));
        conn->on_disconnect(std::bind(&Server::client_disconnected,this,_1));

    }

    void distribute_chunks_to_client(std::unique_ptr<ClientConnection>& conn) {
        int ctr = 0;
        int chunk_lim = 1 + ((_tot_chunks-1)/_min_clients);
        while (!_chunks_to_distribute.empty() and ctr < chunk_lim) {
            conn->send_chunk(_chunks_to_distribute.front());
            _chunks_being_distributed.insert(_chunks_to_distribute.front()->id);
            _chunks_to_distribute.pop();
            ctr++;
        }
    }

    void accept_new_client() {

        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(addr);
        uintptr_t new_fd = accept(_tcp_ss, (struct sockaddr*)&addr, &addr_size);

        // for now, let fd and client id be the same
        std::unique_ptr<ClientConnection> conn(new ClientConnection(new_fd, new_fd, _udp_ss, addr));

        register_to_queue(conn->get_tcp_fd());
        _client_requests.emplace(conn->get_client_id(), std::unordered_set<uint32_t>());
        bind_callbacks_to_client(conn);

        std::cout << "Sending registration data to client" << std::endl;
        conn->send_control_msg({ REG, (uint32_t)conn->get_client_id(), _tot_chunks });

        if (_distributed_all_chunks) {
            std::cout << "All chunks distributed already, letting client know" << std::endl;
            conn->send_control_msg({ OPEN, 0, 0 });
        }
        else {
            distribute_chunks_to_client(conn);
        }

        _clients.emplace(conn->get_client_id(), std::move(conn));
    }

    // impl specific
    void can_read_UDP();

    void run(volatile bool& running) {

        listen(_tcp_ss, 10);

        while (running) {
            std::vector<event_t> evts = _evt_queue.get_events();

            if (_distributed_all_chunks and !_requests_open) {
                open_requests();
            }

            for (const auto& e : evts) {
                if (e.ident == _tcp_ss) {
                    accept_new_client();
                }
                else if (e.ident == _udp_ss) {
                    if (e.filter == EVFILT_READ) can_read_UDP();
                    if (e.filter == EVFILT_WRITE) {
                        for (auto& p : _clients) {
                            p.second->can_write_UDP();
                        }
                    }
                }
                else if (_clients.find(e.ident) != _clients.end()) {
                    if (e.filter == EVFILT_READ) _clients[e.ident]->can_read_TCP();
                    if (e.filter == EVFILT_WRITE) _clients[e.ident]->can_write_TCP();
                }
            }
        }

        shutdown_server();
    }

    void shutdown_server() {
        for (const auto& p : _clients) {
            p.second->close_client();
        }

        close(_tcp_ss);
        close(_udp_ss);

        _evt_queue.close();
    }
};

// class Server {

//     EventQueue _evt_queue;

//     // we need a TCP server socket for client->server chunk transfer
//     TCPServerSocket _tcp_ss;

//     // and a set of TCP/UDP connections for writing chunks from server->client

//     // collections can't store classes. They can store pointers (shared). 
//     // Solution: Just have one TCPConnection map, and let others index into that 
//     // we can use unique pointers in that case
//     std::unordered_map<uintptr_t,std::unique_ptr<TCPConnection>> _fd_map;

//     // ? what's the most optimal way to store client UDP map data
//     // eh, let's just store it in a typedef'd sockaddr_in
//     std::unordered_map<uint32_t,udpaddr_t> _client_udp_map; 
//     std::unordered_map<uint32_t,uintptr_t> _client_tcp_map;

//     // this is like a relation between client_id and chunk_id, allowing 
//     // amortized O(1) relation access, update and deletion
//     std::unordered_map<uint32_t,std::unordered_set<uint32_t>> _chunk_requests;
//     std::unordered_map<uint32_t,std::unordered_set<uint32_t>> _client_requests;

//     // and a UDP socket for receiving/transmitting requests
//     UDPSocket _udp_ss;

//     // when clients connect, each client needs to be sent a unique ID. This 
//     // is a way of 'connecting' their TCP and UDP ports together, as we can't 
//     // obtain the sending client's TCP port from their UDP request, without 
//     // having very messy (address,port) maps. 
//     // 
//     // Before they're added to the server core, we store them in a pending queue
//     std::unordered_map<uintptr_t,std::unique_ptr<TCPConnection>> _clients_pending_registration;
//     uint32_t _next_client_id;


//     // LRU Cache to cache some blocks
//     LRUCache<uint32_t,std::shared_ptr<FileChunk>> _chunk_cache;

//     // initial file chunk list
//     std::queue<std::shared_ptr<FileChunk>> _chunks_to_distribute;
//     std::unordered_set<uint32_t> _chunks_being_distributed;
//     std::unordered_set<uint32_t> _chunks_distributed;

// public:

//     Server(uint16_t port, uint32_t min_clients, size_t chunk_cache_size): 
//         _tcp_ss{port},
//         _udp_ss{port},
//         _next_client_id{0},
//         _min_clients{min_clients},
//         _chunk_cache{chunk_cache_size} {

//         _evt_queue.add_event(_tcp_ss.get_fd(), EVFILT_READ);
//         _evt_queue.add_event(_udp_ss.get_fd(), EVFILT_READ);
//         _evt_queue.add_event(_udp_ss.get_fd(), EVFILT_WRITE);

//         _udp_ss.on_chunk_request(std::bind(&Server::received_chunk_request,this,_1,_2));

//         std::cout << "Created server" << std::endl;

//     }

//     void load_file(std::string filepath) {
//         // the server will equally distribute the 1kb chunks among min_clients 
//         // (The first n clients who join the server). once min_clients connect 
//         // to the server, the server will delete the file and all transactions
//         // taking place after that will be PSP
        
//         _chunks_to_distribute = read_and_chunk_file(filepath);
//         _tot_chunks = _chunks_to_distribute.size();

//         std::cout << "Loaded file" << std::endl;
//     }

//     void sent_chunk(uint32_t chunk_id, uint32_t client_id, int errcode) {

//         if (!_distributed_all_chunks) {
//             if (_chunks_being_distributed.find(chunk_id) != _chunks_being_distributed.end()) {
//                 _chunks_being_distributed.erase(chunk_id);
//                 _chunks_distributed.insert(chunk_id);
//                 if (_chunks_distributed.size() == _tot_chunks) {
//                     _distributed_all_chunks = true;
//                     std::cout << "Distributed all chunks" << std::endl;
//                 }
//             }
//         }

//         if (errcode != 0) {
//             std::cout << "Could not write chunk (errno " << errno << ")" << std::endl;
//             return;
//         }
//         _chunk_requests[chunk_id].erase(client_id);
//         _client_requests[client_id].erase(chunk_id);
//     }

//     void client_disconnected(uint32_t client_id) {
//         // disconnect client_id from everything
//         std::cout << "Disconnecting client with id " << client_id << std::endl;
        
//         for (auto chunk_id : _client_requests[client_id]) {
//             _chunk_requests[chunk_id].erase(client_id);
//         }

//         _client_requests.erase(client_id);
//         _client_udp_map.erase(client_id);
//         _evt_queue.delete_event(_client_tcp_map[client_id], EVFILT_READ);
//         _evt_queue.delete_event(_client_tcp_map[client_id], EVFILT_WRITE);
//         // _fd_map[_client_tcp_map[client_id]]->close();
//         _fd_map.erase(_client_tcp_map[client_id]);
//         _client_tcp_map.erase(client_id);
//     }

//     void received_chunk(std::shared_ptr<FileChunk> chunk, int errcode) {
//         if (errcode != 0) {
//             std::cout << "Could not read chunk (errno " << errno << ")" << std::endl;
//             return;
//         }

//         // serve the people who needed the chunk first
//         if (_chunk_requests.find(chunk->id) != _chunk_requests.end()) {
//             for (auto c : _chunk_requests[chunk->id]) {
//                 _fd_map[_client_tcp_map[c]]->send_chunk_async(chunk);
//             }
//         }

//         // then store the chunk in the LRU cache (if it doesn't already exist)
//         if (_chunk_cache.access(chunk->id) == nullptr) {
//             _chunk_cache.insert(chunk->id, chunk);
//         }
//     }

//     void received_chunk_request(uint32_t client_id, uint32_t chunk_id) {
//         // check if chunk is in LRU cache first
//         std::shared_ptr<FileChunk> chunk = _chunk_cache.access(chunk_id);
//         if (chunk != nullptr) {
//             _fd_map[_client_tcp_map[client_id]]->send_chunk_async(chunk);
//         }
//         else {
//             // request the chunk
//             if (_chunk_requests.find(chunk_id) != _chunk_requests.end()) {
//                 _chunk_requests[chunk_id].insert(client_id);
//             }
//             else {
//                 std::unordered_set<uint32_t> v;
//                 v.insert(client_id);
//                 _chunk_requests[chunk_id] = v;
//             }

//             _client_requests[client_id].insert(chunk_id); // guaranteed to exist

//             for (auto p : _client_udp_map) {
//                 if (p.first != client_id) {
//                     _udp_ss.request_chunk_async(p.second,client_id,chunk_id);
//                 }
//             }
//         }
//     }

//     // by convention, assume that udp_socket on client is tcp_socket+1
//     static udpaddr_t tcp_to_udp_addr(struct sockaddr_in addr) {
//         return addr;
//     }

//     void run(volatile bool& running) {

//         _tcp_ss.listen();

//         while (running) {

//             // TODO this throws an exception when I terminate
//             // libc++abi.dylib: terminating with uncaught exception of type std::length_error: vector
//             // maybe return std::unique_ptr<event_t> instead of raw copies? 
//             // I don't even know how (if) move semantics are working here
//             std::vector<event_t> evts = _evt_queue.get_events();

//             if (_distributed_all_chunks and !_informed_clients) {
//                 for (auto& p : _client_udp_map) {
//                     _udp_ss.request_chunk_async(p.second, 0xffffffff, 0);
//                 }
//                 _informed_clients = true;
//                 std::cout << "Informed all clients that chunks were distributed" << std::endl;
//             }

//             for (const auto& e : evts) {
//                 if (e.ident == _tcp_ss.get_fd()) {
//                     std::cout << "Received connection, assigning id " << _next_client_id << std::endl;

//                     std::cout << "Registered client" << std::endl;
//                 }
//                 else if (_fd_map.find(e.ident) != _fd_map.end()) {
//                     // auto conn = _fd_map[e.ident];
//                     //std::cout << "event in thing" << std::endl;
//                     if (e.filter == EVFILT_READ) {
//                         //std::cout << "reading" << std::endl;
//                         _fd_map[e.ident]->can_read();
//                         //std::cout << "read" << std::endl;
//                     }
//                     else if (e.filter == EVFILT_WRITE) {
//                         //std::cout << "writing" << std::endl;
//                         _fd_map[e.ident]->can_write();
//                         //std::cout << "wrote" << std::endl;
//                     }
//                 }
//                 else if (e.ident == _udp_ss.get_fd()) {
//                     if (e.filter == EVFILT_READ) {
//                         // UDP request for packet
//                         _udp_ss.can_read();
//                     }
//                     else if (e.filter == EVFILT_WRITE) {
//                         _udp_ss.can_write();
//                     }
//                 }
//                 // else if (e.flags & EV_EOF) {
//                 //     client_disconnected()
//                 // }
//             }
//         }

//         std::cout << "Terminating" << std::endl;
//         // for now, just shutdown all file descriptors and deregister them from 
//         // the event queue

//         _tcp_ss.close();
//         _udp_ss.close();
//         for (const auto& p : _fd_map) {
//             p.second->close();
//         }

//         _evt_queue.close();

//         // should we clear out the queues?
//         // yeah, would be a good practice..
//         // TODO clear out queues

//     }
// };