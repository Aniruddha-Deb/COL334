#include "client.hpp"
#include <csignal>

volatile bool running = true;

void terminate(int signal) {
    running = false;
}

// args:
// 1. Server address
// 2. Server port

int main(int argc, char** argv) {

    std::signal(SIGINT, terminate);
    std::signal(SIGTERM, terminate);
    std::signal(SIGKILL, terminate);

    Client clt("127.0.0.1", 15000);
    // clt.connect();
    // pause here? take user input and then start running
    clt.run(running);

    return 0;
}