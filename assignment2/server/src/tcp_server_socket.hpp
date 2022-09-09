#include <string>
#include "tcp_connection.hpp"


class TCPServerSocket {


public:
    TCPServerSocket(std::string address, uint16_t port);

    void listen();

    TCPConnection accept();
};