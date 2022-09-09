#include <string>

class Server {

    std::string addr;
    uint16_t port;

public:

    Server(std::string address, uint16_t p);

    void run(volatile bool& running);

    void register_socket()

};