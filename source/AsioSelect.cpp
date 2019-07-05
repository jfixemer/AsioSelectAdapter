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

#include "adapter/AsioSelect.hpp"
#include <sys/select.h>
#include <sys/time.h>

#ifdef ASIO_SELECT_TRACE
#include <iostream>
struct fd_set_nr
{
    int max;
    fd_set& fds;
};

std::ostream& operator<<(std::ostream& os, const fd_set_nr& sn)
{
    const char* sep = "";
    os << "max_fd=" << sn.max;
    os << ", fdset=[";
    for (int i = 0; i < sn.max + 1; ++i)
        if (FD_ISSET(i, &sn.fds))
        {
            os << sep << i;
            sep = ", ";
        }
    os << "]";
    return os;
}
#endif
namespace adapter
{
namespace asio = boost::asio;

// AsioSelect allows Asio to dispatch events designed to integrate with
// BSD sockets and select.

AsioSelect::device_t::device_t(asio::io_context& ctx)
    : parent(ctx)
{
}
AsioSelect::device_t::device_t(asio::io_context& ctx, const native_handle_type& fd)
    : parent(ctx, fd)
{
}
AsioSelect::device_t::device_t(device_t&& other)
    : parent(std::move(other))
{
}

void AsioSelect::setup(dispatch_cb_t disp, select_cb_t sel)
{
    on_dispatch_ = std::move(disp);
    on_select_ = std::move(sel);
    update();
}

void AsioSelect::update()
{
    if (on_select_)
        update(on_select_);
}

void AsioSelect::update(select_cb_t& get_select_info)
{

    int max_fds = 0;
    fd_set fds;
    FD_ZERO(&fds);
    timeval tv = { 0, 0 };
    int blocking = 1;
    get_select_info(&max_fds, &fds, &tv, &blocking);

    audit_read(fds, max_fds);
    add_read(fds, max_fds);
    if (!blocking)
        audit_timeout(std::chrono::seconds(tv.tv_sec) + std::chrono::milliseconds(tv.tv_usec));
}

AsioSelect::AsioSelect(asio::io_context& io)
    : asio_io_(io)
    , select_timer_(io)
{
    FD_ZERO(&m_read_waiting);
}

// removes all fds not set in the fdset.
void AsioSelect::audit_read(fd_set& fdset, native_fd_t max_fd)
{
#ifdef ASIO_SELECT_TRACE
    std::cerr << "TRACE audit_read:  " << fd_set_nr { max_fd, fdset } << std::endl;
#endif
    for (auto iter = tracker_.begin(), last = tracker_.end(); iter != last;)
    {
        if ((*iter).first > max_fd || !FD_ISSET((*iter).first, &fdset))
            iter = tracker_.erase(iter);
        else
            ++iter;
    }
}

void AsioSelect::audit_timeout(duration_t new_timer_duration)
{
#ifdef ASIO_SELECT_TRACE
    std::cerr << "TRACE audit_timeout:  " << new_timer_duration.count() << std::endl;
#endif
    timer_t new_timer(asio_io_, new_timer_duration); // handles now() + duration.
    auto current_tmo = select_timer_.expiry();
    auto new_tmo = new_timer.expiry();
    auto five_ms = std::chrono::milliseconds(5);
    // Most likely cases are
    //  - timer is not running (current_tmo == epoch time_point), don't try to subtract from
    //  that btw.
    //  - timeout is the same timer as last time (+/- some processing delay getting to this
    //  point)
    if ((current_tmo < (new_tmo - five_ms)) || (current_tmo > (new_tmo + five_ms)))
    {
        auto self(shared_from_this());
        select_timer_ = std::move(new_timer);
        select_timer_.async_wait([this, self](error_code_t ec) {
            if (!ec)
            {
                // set to epoch to indicate already expired.
                // set before dispatching so that reschedule works.
                select_timer_.expires_at(time_point_t());
                dispatch(ec, EVENT::timeout, 0);
            }
        });
    }
}

void AsioSelect::add_read(fd_set& fdset, int maxfd)
{
#ifdef ASIO_SELECT_TRACE
    std::cerr << "TRACE add_read:  " << fd_set_nr { maxfd, fdset } << std::endl;
#endif
    auto self(shared_from_this());
    ++maxfd;
    for (int fd = 0; fd < maxfd; ++fd)
    {
        if (FD_ISSET(fd, &fdset) && !FD_ISSET(fd, &m_read_waiting))
        {
#ifdef ASIO_SELECT_TRACE
            std::cerr << "TRACE add_read+create: fd=" << fd << std::endl;
#endif
            // Create if missing
            auto end = tracker_.end();
            auto iter = tracker_.lower_bound(fd);
            // Is iter the item or just an insert hint?
            if ((iter == end) || (fd != iter->first))
            {
                iter = tracker_.insert(
                    iter, tracker_t::value_type { fd, device_t { asio_io_, fd } });
            }

            // Add the wait function
            FD_SET(fd, &m_read_waiting);

            iter->second.async_wait(device_t::wait_read, [this, self, fd](error_code_t ec) {
                // Clear the wait in the lambda because dispatch also handles timeout.
                FD_CLR(fd, &m_read_waiting);
                dispatch(ec, EVENT::fd_read, fd);
            });
        }
    }
}

void AsioSelect::dispatch(error_code_t ec, EVENT event, native_fd_t fd)
{
    if (on_dispatch_)
    {
        fd_set dispatch;
        FD_ZERO(&dispatch);
        int num_fds = 0;
        if (event == EVENT::fd_read)
        {
            FD_SET(fd, &dispatch);
            num_fds = 1;
        }

        if (on_dispatch_(num_fds, &dispatch, ec) < 0)
            asio_io_.stop();
    }
    if (on_select_)
    {
        update(on_select_);
    }
}

}
