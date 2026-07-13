#include <coconext/event_loop.hpp>
#include <stdexcept>

namespace coconext::event_loop {

thread_local EventLoop::Handle* current_event_loop = nullptr;

EventLoop::Handle& get_current_event_loop() {
    if (current_event_loop) {
        return *current_event_loop;
    }
    throw std::runtime_error("No current event loop");
}

EventLoop::Handle coconext::event_loop::EventLoop::acquire() {
    mtx_.lock();
    auto handle = Handle(*this);
    current_event_loop = &handle;
    return std::move(handle);
}

EventLoop::Handle::~Handle() {
    current_event_loop = nullptr;
    loop_.mtx_.unlock();
}

}  // namespace coconext::event_loop
