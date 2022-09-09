#include "server.hpp"
#include <csignal>

volatile bool running = true;

void terminate(int signal) {
    running = false;
}

int main() {

    std::signal(SIGINT, terminate);
    std::signal(SIGTERM, terminate);
    std::signal(SIGKILL, terminate);

    Server srv("127.0.0.1", 15001);
    srv.run(running);

    return 0;
}