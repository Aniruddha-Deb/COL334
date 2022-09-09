#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "tcp_connection.hpp"
#include "chunk.hpp"




uintptr_t TCPConnection::get_fd() {
    return _fd;
}