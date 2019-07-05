#pragma once
#include <gmock/gmock.h>

#include <vector>
#include <sys/select.h>
#include <netinet/in.h>
#include <boost/system/error_code.hpp>

// Helper to generate some raw sockets for testing.
// Most of these apps would be C APIs so using 'raw pointer' conventions.

struct LegacyWrapper
{
    static LegacyWrapper* instance();
    
    MOCK_METHOD4(select_info, int(int* num_fds, fd_set* fds, struct timeval* tv, int* blocking));
    MOCK_METHOD2(dispatch, int(int num_ready, fd_set* read_fds));
    MOCK_METHOD1(read, void(fd_set* read_fds));
    MOCK_METHOD0(timeout, void());

    //int select_info(int* num_fds, fd_set* fds, struct timeval* tv, int* blocking);
    //void dispatch(int num_ready, fd_set* read_fds);
    //void read(fd_set* read_fds);
    //void timeout();

    LegacyWrapper();
    ~LegacyWrapper();
    LegacyWrapper* last_global;
};


// Retrieve information about sockets.
int legacy_initialize();
int legacy_select_info(int* num_fds, fd_set* fds, struct timeval* tv, int* blocking); //netsnmp::snmp_select_info
void legacy_read(fd_set* read_fds); // netsnmp::snmp_read
void legacy_timeout();

// Example functions
int legacy_dispatch(int num_ready, fd_set* read_fds);
int legacy_dispatch_wrapper(int num_ready, fd_set* read_fds, boost::system::error_code);
