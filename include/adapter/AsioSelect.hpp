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

// AsioSelect allows Asio to dispatch events designed to integrate with
// BSD sockets and select.

class AsioSelect : public std::enable_shared_from_this<AsioSelect>
{
public:
    // asio::posix::descriptor had a protected destructor for some reason...
    class device_t : public asio::posix::descriptor
    {
        using parent = asio::posix::descriptor;
        public:
            device_t(asio::io_context& ctx);
            device_t(asio::io_context& ctx, const native_handle_type& fd);
            device_t(device_t&& other);
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

    void setup(dispatch_cb_t disp, select_cb_t sel);


    inline bool is_waiting(native_fd_t fd) { return FD_ISSET(fd, &m_read_waiting); }
    inline bool empty() { return tracker_.empty(); }
    inline size_t size() { return tracker_.size(); }

    void update();

    void update(select_cb_t& get_select_info);

    AsioSelect(asio::io_context& io);
private:

    // removes all fds not set in the fdset.
    void audit_read(fd_set& fdset, native_fd_t max_fd);

    void audit_timeout(duration_t new_timer_duration);

    void add_read(fd_set& fdset, int maxfd);

    void dispatch(error_code_t ec, EVENT event, native_fd_t fd);
    
    asio::io_context& asio_io_;
    timer_t select_timer_;
    select_cb_t on_select_;
    dispatch_cb_t on_dispatch_;
    tracker_t tracker_;
    fd_set m_read_waiting;
};

}
