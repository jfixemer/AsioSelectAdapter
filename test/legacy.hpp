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
    // This would be incidental tracking in the third party
    // maybe even buried in a subsctruct or list of sessions, etc.
    // Some libraries may even track this as (!)global data(!).
    std::vector<int> sockets;
    std::vector<sockaddr_in> addrs;

    static void global(LegacyWrapper*);
    static LegacyWrapper& instance();

    MOCK_METHOD4(select_info, int(int* num_fds, fd_set* fds, struct timeval* tv, int* blocking));
    MOCK_METHOD2(dispatch, int(int num_ready, fd_set* read_fds));
    MOCK_METHOD1(read, void(fd_set* read_fds));
    MOCK_METHOD0(timeout, void());

    //int select_info(int* num_fds, fd_set* fds, struct timeval* tv, int* blocking);
    //void dispatch(int num_ready, fd_set* read_fds);
    //void read(fd_set* read_fds);
    //void timeout();
};


// Retrieve information about sockets.
int legacy_initialize();
int legacy_select_info(int* num_fds, fd_set* fds, struct timeval* tv, int* blocking); //netsnmp::snmp_select_info
void legacy_read(fd_set* read_fds); // netsnmp::snmp_read
void legacy_timeout();

// Example functions
int legacy_dispatch(int num_ready, fd_set* read_fds);
int legacy_dispatch_wrapper(int num_ready, fd_set* read_fds, boost::system::error_code);
