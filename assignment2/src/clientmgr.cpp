#include "client.hpp"
#include <csignal>

volatile bool running = true;

void terminate(int signal) {
    running = false;
}

int main(int argc, char const *argv[]) {

    std::signal(SIGINT, terminate);
    std::signal(SIGTERM, terminate);
    std::signal(SIGKILL, terminate);

    Client clt("127.0.0.1", 15000);
    // clt.connect();
    // pause here? take user input and then start running
    clt.run(running);

    return 0;
}