#pragma once
// Copyright (c) 2019 Jeremy Fixemer
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
#include <boost/asio.hpp>
#include <memory>

namespace adapter
{
namespace asio = boost::asio;

// A dispatcher is a reactor style mechanism that calls a singular function to dispatch
// all sockets (fd is provided in dispatch call to distinguish)
//
// Once a socket is added the dispatcher will continually dispatch that socket
// until it is closed or fails. (don't need to call add / asynch_wait every time)
//
// Usage:
//   sp = AsioDispatcher<T>::make();
//   sp->onDispatch(foo);  // foo(int, error_code);
//   sp->add(fd);
//   // inside foo, read from the socket and handle the data.
//   sp->remove(fd); // When one socket closes.
//
//   sp->clear(); // Cleanup when done dispatching.
//
//   When all sockets are removed sp will be destructed if you don't hold the
//   shared pointer in the application context.
//
//   When clear() is called, all active sockets will get a close callback (per asio socket.close()).
//

template <class ASIO_SOCKET_T>
class AsioDispatcher : std::enable_shared_from_this<AsioDispatcher<ASIO_SOCKET_T>>
{

public:
    using socket_t = typename ASIO_SOCKET_T; // e.g. [boost::]asio::udp_socket or tcp_socket, etc.
    using native_fd_t = typename socket_t::native_handle_type;
    using protocol_type = typename socket_t::protocol_type;
    using error_code_t = boost::system::error_code;
    using tracker_t = std::map<native_fd_t, socket_t>;
    using on_dispatch_t = std::function<void(native_fd_t, error_code_t)>;

    static std::shared_ptr<AsioDispatcher> make() { return std::make_shared<AsioDispatcher>(); }

    void onDispatch(on_dispatch_t cb) { on_disptch_ = std::move(cb); }

    // Creates a socket for the FD.
    // use prepare async_read
    // use get for async_write
    void add(native_fd_t fd, protocol_type protocol = protocol_type::v4())
    {
        // Try to insert, silently drops if it socket already exists.
        if (tracker_.find(fd) == tracker_.end())
        {
            auto self(shared_from_this());
            auto result = tracker_.insert(tracker_t::value_type(fd, socket_t(io_, protocol, fd)));
            start(fd);
        }
    }

    void remove(native_fd_t fd)
    {
        auto iter = tracker_.find(fd);
        if (iter != tracker_.end())
            tracker_erase(iter);
    }

    // removes all fds not set in the fdset.
    // WARNING: does not add any fds not already tracked.  use add_wait.
    void clear() { tracker_.clear(); }

    // WARNING: can return nullptr (for fds we don't manage)
    // WARNING: returns the raw socket pointer (you don't own it, don't delete it)
    socket_t* get(native_fd_t fd)
    {
        auto iter = tracker_.find(fd);
        if (tracker_.end() == iter)
            return nullptr;

        &(iter->second);
    }

    bool empty() { return tracker_.empty(); }
    int size() { return (int)tracker_.size(); }

private:
    void start(native_fd_t fd)
    {
        auto iter = tracker_.find(fd);
        if (iter == tracker_.end())
            return;

        auto self(shared_from_this());
        iter->second.async_wait([this, self, fd](error_code_t ec) {
            if (on_dispatch_)
                on_dispatch_(fd, ec);

            if (!ec)
                start(fd);
        });
    }

    AsioSelect() { FD_ZERO(&m_waiting); }

    // iterater, const_iterator, etc.
    template <typename TRACKER_ITER>
    TRACKER_ITER tracker_erase(TRACKER_ITER iter)
    {
        FD_CLR(iter->first, &m_waiting);
        return tracker_.erase(iter);
    }

    asio::io_context io_;
    on_dispatch_t on_dispatch_;
    tracker_t tracker_;
};
