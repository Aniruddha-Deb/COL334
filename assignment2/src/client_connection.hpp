class ClientConnection {

    uint32_t _client_id;
    uintptr_t _tcp_fd;
    uintptr_t _udp_fd;

    std::function<void(std::shared_ptr<FileChunk>)> _recv_chunk_callback;
    std::function<void(uint32_t, uint32_t)> _recv_chunk_req_callback;

    std::queue<ControlMessage> _control_msg_buffer;
    std::queue<std::shared_ptr<FileChunk>> _chunk_buffer;

public:

    ClientConnection(uint32_t client_id, uintptr_t tcp_fd, uintptr_t udp_fd, struct sockaddr_in client_addr);

    void on_recv_chunk(std::function<void(std::shared_ptr<FileChunk>)>);

    void on_recv_chunk_req(std::function<void(uint32_t, uint32_t)>);

    void send_control_msg(ControlMessage& msg);

    void req_chunk(uint32_t chunk_id);

    void send_chunk(std::shared_ptr<FileChunk> chunk);

    void close_client();

    void can_write_TCP();

    void can_read_TCP();

    void can_write_UDP();

    void can_read_UDP();

}