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
#ifdef BOOST_ASIO
#include <boost/asio.hpp>
#else
#include <asio.hpp>
#endif

namespace wrapper
{
template <class ASIO_SOCKET_T>
class AsioSelectAdapter
{
#ifdef BOOST_ASIO
    using asio=boost::asio;
#endif
public:
    using socket_t = ASIO_SOCKET_T; // e.g. [boost::]asio::udp_socket or tcp_socket, etc.
    using add_wait_fdset_cb = std::function<void(std::system_error ec, int fd)>;
    using add_wait_cb = std::function<void(std::system_error ec)>;
    using socket_up = std::unique_ptr<socket_t>;
    using tracker_t = std::vector<socket_up>;

    AsioSelectAdapter()
    {
        FD_ZERO(&m_waiting);
    }

    // Creates a socket for the FD.
    // use prepare async_read
    // use get for async_write
    void add(int fd)
    {
        if (tracker_.size() < fd)
            tracker_.resize(fd);

        if (!tracker_[fd])
        {
            FD_CLR(fd, &m_waiting); // defensive.
            tracker_[fd] = socket_t(io_, socket_t::protocol(), fd);
        }
    }

    bool add_wait(int fd, add_wait_cb callback)
    {
        add(fd);

        if(FD_ISSET(fd, &m_waiting))
            return false;

        FD_SET(fd, &m_waiting);

        // Since Boost:1.66.0 - socket::async_wait replaces async_read(null_buffer)
        tracker_[fd]->async_wait(std::move(callback));
    }

    // WARNING: Expectation is that FD_ISSET(maxfd-1,fdset) is true.
    // WARNING: Does not audit (remove unset fds)
    // WARNING: Creates a copy of callback for every new fd
    // Order of audit and add_wait is not too crucial, but 
    // add_wait, then audit may avoid memory relocation in audit.
    void add_wait(fd_set& fdset, int maxfd, add_wait_fdset_cb callback)
    {
        if (tracker_.size() < maxfd)
            tracker_.resize(maxfd);

        for(int fd = 0; fd < maxfd; ++fd)
        {
            if(FD_ISSET(fd, fdset))
                add_wait(fd, [fd, callback](std::system_error ec){callback(ec, fd);});
        }
    }

    // removes all fds not set in the fdset.
    // WARNING: does not add any fds not already tracked.  use add_wait.
    void audit(fd_set& fdset, int maxfd) 
    { 
        size_t counter = 0;
        for(auto& uptr : tracker_)
        {
            if( (counter > maxfd || !FD_ISSET(counter)) && tracker_[counter])
                close_impl(counter);
            ++counter;
        }
        compact();
    }


    void close(int fd)
    {
        close_impl(fd);
        compact();
    }

    // WARNING: can return nullptr (for fds we don't manage)
    // WARNING: returns the raw socket pointer (you don't own it, don't delete it)
    socket_t* get(int fd)
    {
        if(fd >= tracker_.size())
            return nullptr;
        
        return tracker_[fd].get();
    }

    // WARNING: Can only be called once 
    // (then it becomes a waiting socket, cannot prepared again until dispatched)
    socket_t* prepare(int fd)
    {
        if(fd >= tracker_.size() || !tracker_[fd])
            return nullptr;

        if( FD_ISSET(fd, &m_waiting))
            return nullptr;

        FD_SET(fd, &m_waiting);
        return tracker_[fd].get();
    }

    void is_waiting(int fd) { return FD_ISSET(fd, &m_waiting); }

    int size() { return (int)tracker_.size(); }

private:
    void close_impl(int fd)
    {
        FD_CLR(fd, &m_waiting);
        if (tracker_[fd])
            tracker_[fd].reset(nullptr);
    }

    void compact(void)
    {
        auto is_uptr_null = [](const socket_up& item) -> bool { return item; };
        auto last_not_null
            = std::find_if(tracker_.crbegin(), tracker_.crend(), is_uptr_null).base();

        if (last_not_null != tracker_.end())
            tracker_.erase((++last_not_null), tracker_.end());

    }

    asio::io_context io_;
    fd_set &m_waiting;
    std::vector<std::unique_ptr<socket_t>> tracker_;
};

}