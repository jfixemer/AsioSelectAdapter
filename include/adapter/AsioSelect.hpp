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
#include <boost/asio/posix/descriptor.hpp>
#include <memory>
#include <sys/select.h>
#include <sys/time.h>

namespace adapter
{
namespace asio = boost::asio;

// AsioSelect allows Asio to dispatch events designed to integrate with
// BSD sockets and select.

class AsioSelect : public std::enable_shared_from_this<AsioSelect>
{
public:
    class device_t : public asio::posix::descriptor
    {
        using parent = asio::posix::descriptor;
        public:
            device_t(asio::io_context& ctx) : parent(ctx) {}
            device_t(asio::io_context& ctx, const native_handle_type& fd) : parent(ctx, fd) {}
            device_t(device_t&& other) : parent(std::move(other)) {}
            ~device_t() = default;
    };

    using native_fd_t = typename asio::posix::descriptor::native_handle_type;
    using timer_t = asio::steady_timer;
    using clock_t = timer_t::clock_type;
    using duration_t = timer_t::duration;
    using time_point_t = timer_t::time_point;
    using error_code_t = boost::system::error_code;
    using tracker_t = std::map<native_fd_t, device_t>;
    using max_fd=native_fd_t;
    using blocking=int;
    using select_cb_t = std::function<int(max_fd*, fd_set*, struct timeval*, blocking*)>;
    using dispatch_cb_t = std::function<int(int num_ready, fd_set*, error_code_t)>;

    enum class EVENT
    {
        fd_read,
        timeout
    };

    inline static std::shared_ptr<AsioSelect> make(asio::io_context& io)
    {
        return std::make_shared<AsioSelect>(io);
    }

    void setup(dispatch_cb_t disp, select_cb_t sel)
    {
        on_dispatch_ = std::move(disp);
        on_select_ = std::move(sel);
        update();
    }

    bool is_waiting(native_fd_t fd) { return FD_ISSET(fd, &m_read_waiting); }
    bool empty() { return tracker_.empty(); }
    size_t size() { return tracker_.size(); }

    void update()
    {
        if (on_select_)
            update(on_select_);
    }

    void update(select_cb_t& get_select_info)
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

    AsioSelect(asio::io_context& io)
        : asio_io_(io)
        , select_timer_(io)
    {
        FD_ZERO(&m_read_waiting);
    }
private:

    // removes all fds not set in the fdset.
    void audit_read(fd_set& fdset, native_fd_t max_fd)
    {
        for (auto iter = tracker_.begin(), last = tracker_.end(); iter != last;)
        {
            if ((*iter).first > max_fd || !FD_ISSET((*iter).first, &fdset))
                iter = tracker_.erase(iter);
            else
                ++iter;
        }
    }

    void audit_timeout(duration_t new_timer_duration)
    {
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

    void add_read(fd_set& fdset, int maxfd)
    {
        auto self(shared_from_this());
        for (int fd = 0; fd < maxfd; ++fd)
        {
            if (FD_ISSET(fd, &fdset) && !FD_ISSET(fd, &m_read_waiting))
            {
                // Create if missing
                auto end = tracker_.end();
                auto iter = tracker_.lower_bound(fd);
                // Is iter the item or just an insert hint?
                if ((iter == end) || (fd != iter->first))
                {
                    iter = tracker_.insert(iter, tracker_t::value_type{fd, device_t{asio_io_, fd}});
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

    void dispatch(error_code_t ec, EVENT event, native_fd_t fd)
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

            if(on_dispatch_(num_fds, &dispatch, ec) < 0)
                asio_io_.stop();
        }
        if (on_select_)
        {
            update(on_select_);
        }
    }
    

    asio::io_context& asio_io_;
    timer_t select_timer_;
    select_cb_t on_select_;
    dispatch_cb_t on_dispatch_;
    tracker_t tracker_;
    fd_set m_read_waiting;
};

}
