#include "server.hpp"
#include <iostream>
#include <string>

Server::Server(std::string address, uint16_t p): 
    addr{address},
    port{p} 
{
    // TODO create a server socket here
}

void Server::run(volatile bool& running) {

    // create the EventQueue

    while (running) {
        // get the file descriptors that are open from the Event Queue
        // check conditions on the file descriptors
        // call the callbacks 
    }

    std::cout << "Terminating" << std::endl;
}