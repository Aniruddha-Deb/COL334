#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <queue>

#include "chunk.hpp"

class TCPConnection {

    uintptr_t _fd;
    std::queue<std::pair<std::shared_ptr<FileChunk>,std::function<void(int)>>> _send_chunk_buffer;
    std::queue<std::function<void(std::shared_ptr<FileChunk>,int)>> _rcv_callback_buffer;

public:

    TCPConnection(uintptr_t fd): _fd{fd} {
        // set socket to be non-blocking
        fcntl(fd, F_SETFL, O_NONBLOCK);

        // set socket low-water mark value for input to be the size of one file chunk
        const int sock_recv_lwm = sizeof(FileChunk);
        setsockopt(fd, SOL_SOCKET, SO_RCVLOWAT, &sock_recv_lwm, sizeof(sock_recv_lwm));
        // we don't have to do something similar for send, as that would slow down 
        // the speed at which we're sending. Just having this at one end is okay

        // socket buffer sizes are generally big enough (on my mac, it's 128 kB) for
        // buffer overflows to be a non-issue, so we don't tweak the default buffer
        // size

        // The sequentiality of TCP guarantees that we're always reading a contiguous
        // chunk sent by client/server. So we don't need a protocol indicating length 
        // or anything of that sort; just chunk into 1024 bytes while reading.

        // timeouts are also ok; let's not change those.
    }

    void send_chunk_async(std::shared_ptr<FileChunk> f, std::function<void(int)> callback) {
        _send_chunk_buffer.push({f,callback});
    }

    void rcv_chunk_async(std::function<void(std::shared_ptr<FileChunk>,int)> callback) {
        _rcv_callback_buffer.push(callback);
    }

    void can_write() {
        auto p = _send_chunk_buffer.front();
        _send_chunk_buffer.pop();
        ssize_t n_sent_bytes = send(_fd, p.first.get(), sizeof(FileChunk), 0);
        if (n_sent_bytes == -1) {
            // an error occured
            p.second(errno);
        }
        else {
            // what do we do if the entire thing wasn't written? 
            // Nothing. Chill out, for it shall be written eventually. 
            // Isn't calling the callback dubious in this case? 
            // Hmmm.... will we have to loop and send continuously?
            // TODO work this out.
            p.second(0);
        }
    }

    void can_read() {
        std::shared_ptr<FileChunk> c(new FileChunk);
        auto f = _rcv_callback_buffer.front();
        _rcv_callback_buffer.pop();
        ssize_t n_rcvd_bytes = recv(_fd, c.get(), sizeof(FileChunk), 0);

        if (n_rcvd_bytes == -1) {
            f(nullptr, errno);
        }
        else {
            // TODO handle the event where we don't read a complete filechunk in...
            f(c,0);
        }
    }

    uintptr_t get_fd() {
        return _fd;
    }

};