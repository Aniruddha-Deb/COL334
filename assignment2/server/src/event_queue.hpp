#include <sys/event.h>
#include <unistd.h>
#include <vector>
#include <unordered_set>

typedef struct kevent event_t;

class EventQueue {

    int _kq;
    //std::unordered_set<std::pair<uintptr_t,int16_t>> _evtset;

public:
    EventQueue() {
        _kq = kqueue();
    }

    void add_event(uintptr_t fd, int16_t filter) {
        event_t evt;
        EV_SET(&evt, fd, filter, EV_ADD, 0, 0, NULL);
        kevent(_kq, &evt, 1, NULL, 0, NULL);
        //_evtset.insert({fd,filter});
    }

    void delete_event(uintptr_t fd, int16_t filter) {
        event_t evt;
        EV_SET(&evt, fd, filter, EV_DELETE, 0, 0, NULL);
        kevent(_kq, &evt, 1, NULL, 0, NULL);
        //_evtset.erase({fd,filter});
    }

    std::vector<event_t> get_events() {
        // Hmm, returning a vector of structs....
        // is this the right way to do this?
        // how do we check if move semantics are kicking in
        event_t evts[64];
        int n_evts = kevent(_kq, NULL, 0, evts, 64, NULL);

        std::vector<event_t> v;
        v.assign(evts, evts+n_evts);
        return v;
    }

    void close() {
        // is this step neccessary? I couldn't find any documentation on it
        // for (auto p : _evtset) {
        //     delete_event(p.first, p.second);
        // }
        ::close(_kq);
    }
};