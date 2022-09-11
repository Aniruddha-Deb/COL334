#include "server.hpp"
#include <csignal>
#include <iostream>
#include <string>

volatile bool running = true;

void terminate(int signal) {
    running = false;
}

// args:
// 1. Server port
// 2. Min no. clients
// 3. LRU cache size (in kB)

int main(int argc, char** argv) {

    std::signal(SIGINT, terminate);
    std::signal(SIGTERM, terminate);
    std::signal(SIGKILL, terminate);

    if (argc < 2) {
        std::cout << "missing argument: file to share" << std::endl;
    }

    Server srv(15000, 5, 1000);
    srv.load_file(std::string(argv[1])); 
    srv.run(running);

    return 0;
}