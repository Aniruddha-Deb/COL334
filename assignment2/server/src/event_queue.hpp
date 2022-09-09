#include <sys/event.h>
#include <vector>

typedef struct kevent event_t;

class EventQueue {

public:
    EventQueue();

    void add_event(uintptr_t fd, int16_t filter);

    std::vector<event_t> get_events();
};