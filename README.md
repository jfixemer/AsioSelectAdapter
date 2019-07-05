# AsioSelectAdapter
Helps applications using fd_set integrate with ASIO.  

Status: Basically working.  Your mileage may vary.

Goal is to convert and old, select based event loop to a new event loop run by ASIO.

Building:
 - copy adapter/AsioSelect.hpp and source/AdpterSelect.cpp into your project. --or--
 - Use this project's CMake configuration: 
   - include: %ROOT%/include, 
   - link: %BUILD%/libselectadapter.a

Why not header only:  I don't fully understand the voodoo of this strategy and would probably
create something that wouldn't offer the inlining and optimization benefits.

Why: This is up to you but some of the items:
 - modernization, moving from select to a poll/epoll system
 - modernization, moving toward future C++ NetworkTS compilance
 - wanting to add Other network or HTTP services with Asio/Beast (REST, WebSockets)

What this does not do:
 - Provide portability to Windows (because asio::posix is not enabled on Windows)

The big problem you have hit is 
 - to many legacy interfaces to convert all at once.
 - some old or third party code cannot be converted (e.g. NetSNMP)

Given an Old event loop:
```c++
int main(...)
{
    legacy_initialize(); // create some sockets and stuff
    while(true)
    {
        fd_set read_fd_set;
        FD_ZERO(read_fd_set);
        int max_fd = 0;
        int blocking=1;
        struct timeval tv;

        // Read info about sockets and the next timeout.
        // e.g. snmp_select_info
        legacy_select_info(&max_fd, &read_fd_set, &tv, &blocking);

        struct timeval *tvp = (blocking ? null : &tv);

        // Ignoring write and exception fds for this UDP example...
        int num_ready = select(max_fd, &read_fd_set, NULL, NULL, tvp);

        if(num_ready < 0)
            if(check_fatal_error(...)) 
                break;

        // Alt: Some may prefer to dispatch sockets before handling timeouts.
        legacy_timeout();  // e.g. netsnmp::snmp_timeout

        if(num_ready > 0 )
            legacy_read(&read_fd_set); // e.g. netsnmp::snmp_read
    }
}
```

This would transform into  A helper function / functor / lambda and a modified 'main' loop based
on asio io/executor.run() 

Helper function:
```c++
#include <sys/select.h>
#include <boost/system/error_codes.hpp>

typedef std::shared_ptr<adapter::AsioSelect> Adapter_sptr;

int legacy_dispatch_wrapper(int num_ready, fd_set* read_fd_set, boost::system::error_code error);
{
    if(error && check_fatal_error(...))        
        return -1; 

    legacy_timeout();

    if(num_ready > 0 )
        legacy_read(read_fd_set);
}
```
The modified 'main' event loop...
```c++

#include "adapter/AsioSelect.hpp"
...

int main(...)
{
    asio::io_context asio_io;

    std::shared_ptr<adapter::AsioSelect> MyAdapter = adapter::AsioSelect::make(asio_io);

    initialize_legacy_service();
    create_asio_native_services(asio_io, ...);

    MyAdapter->setup(legacy_dispatch_wrapper, legacy_select_info);

    //If sockets were not ready at setup:  reactor_sp->update();
    asio_io.run();
}
```

