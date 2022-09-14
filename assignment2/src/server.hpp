#pragma once

#include <string>
#include <cstring>
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

    std::unordered_map<uint32_t,std::unique_ptr<ClientConnection>> _clients;
    std::unordered_map<uintptr_t,uint32_t> _tcp_map;

    uintptr_t _tcp_ss;
    uintptr_t _udp_ss;

    uint32_t _min_clients;
    uint32_t _next_client_id = 0;

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
        _tcp_map.erase(_clients[client_id]->get_tcp_fd());
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
        conn->on_recv_chunk_request(std::bind(&Server::received_chunk_request,this,_1,_2));
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
        _tcp_map[new_fd] = _next_client_id;
        std::unique_ptr<ClientConnection> conn(new ClientConnection(_next_client_id++, new_fd, _udp_ss, addr));

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
                    else if (e.filter == EVFILT_WRITE) {
                        for (auto& p : _clients) {
                            p.second->can_write_UDP();
                        }
                    }
                }
                else if (_tcp_map.find(e.ident) != _tcp_map.end()) {
                    if (e.filter == EVFILT_READ) _clients[_tcp_map[e.ident]]->can_read_TCP();
                    else if (e.filter == EVFILT_WRITE) _clients[_tcp_map[e.ident]]->can_write_TCP();
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
