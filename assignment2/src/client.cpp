#include "client.hpp"
#include <csignal>

volatile bool running = true;

void terminate(int signal) {
    running = false;
}

// args:
// 1. Server address
// 2. Server port

void print_usage() {
    std::cout << "usage: client_(tcp|udp) [-a address] [-p port]" << std::endl;
}

int main(int argc, char** argv) {

    std::signal(SIGINT, terminate);
    std::signal(SIGTERM, terminate);
    std::signal(SIGKILL, terminate);

    std::string addr = "127.0.0.1";
    int port = 15000;

    char opt;
    while ((opt = getopt(argc, argv, "a:p:")) != -1) {
        std::cout << "Got opt " << opt << " with arg " << std::string(optarg) << std::endl;
        switch (opt) {
            case 'a': addr = std::string(optarg); break;
            case 'p': port = std::stoi(std::string(optarg)); break;
            default:
                print_usage();
                return 0;
        }
    }

    Client clt(addr, port);

    clt.run(running);

    return 0;
}