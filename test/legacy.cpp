#include "legacy.hpp"
#include <memory>
#include <gtest/gtest.h>

static std::unique_ptr<LegacyWrapper> legacy_wrapper_uptr;

void LegacyWrapper::global(LegacyWrapper* input) { return legacy_wrapper_uptr.reset(input); }

LegacyWrapper& LegacyWrapper::instance()
{
    if (!legacy_wrapper_uptr)
        legacy_wrapper_uptr.reset(new LegacyWrapper);

    return *legacy_wrapper_uptr;
}

int legacy_initialize() { LegacyWrapper::instance(); return 0;}

int legacy_select_info(int* num_fds, fd_set* fds, struct timeval* tv, int* blocking)
{
    return LegacyWrapper::instance().select_info(num_fds, fds, tv, blocking);
}

int legacy_select_info(int* num_fds, fd_set* fds)
{
    return LegacyWrapper::instance().select_info(num_fds, fds, NULL, NULL);
}

void legacy_read(fd_set* read_fds) { LegacyWrapper::instance().read(read_fds); }
void legacy_timeout() { LegacyWrapper::instance().timeout(); }

// Example functions
int legacy_dispatch(int num_ready, fd_set* read_fds)
{
    return LegacyWrapper::instance().dispatch(num_ready, read_fds);
}
int legacy_dispatch_wrapper(int num_ready, fd_set* read_fds, boost::system::error_code)
{
    return LegacyWrapper::instance().dispatch(num_ready, read_fds);
}
//// // i.e: netsnmp::snmp_select_info + 'non global context'
//// int tp_get_select_info(THIRD_PARTY*, int* num_fds, fd_set* fds)
//// {
////     FD_ZERO(fds);
////     *num_fds = 0;
//// #ifdef _WIN32
////     if (tp->sockets.empty())
////         return 0;
//// 
////     *num_fds = FD_SETSIZE;
////     for (auto& socket : tp->sockets)
////         FD_SET(socket, fds);
//// #else
////     *num_fds = 0;
////     for (auto& socket : tp->sockets)
////     {
////         FD_SET(socket, fds);
////         *num_fds = std::max(*num_fds, socket);
////     }
//// #endif
////     return 0;
//// }
//// 
//// // Creates a new socket and binds it to the given address.
//// // Normal third party would probably take a port to bind on,
//// // But this is just for test so we bind an ephemeral port loopback.
//// // (Use tp->addrs to get the info for test sending.)
//// int tp_open(THIRD_PARTY* tp)
//// {
////     ASSERT_EQ(tp->sockets.size() == tp->addrs.size);
//// 
////     // simulate third party by 'raw socket' creation.
////     // purpose of asio is to avoid crap like this ;-)
////     SOCKET fd = socket(AF_INET, SOCK_DGRAM, 0);
//// 
////     struct sockaddr_in addr = { 0 };
////     addr.sin_family = AF_INET;
////     addr.sin_addr.s_addr = 0x80000001; // loopback
////     addr.sin_port = 0; // ephemeral - assigned in bind.
//// 
////     auto bind_result = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
////     if (bind_result != 0)
////         close(fd);
////     ASSERT_EQ(bind_result, 0);
//// 
////     tp->sockets.push_back(fd);
////     tp->addrs.push_back(std::move(addr));
//// }
//// 
//// // Creates a new socket and binds it to the given address.
//// void tp_close(THIRD_PARTY* tp, size_t index)
//// {
////     close(tp->sockets[index]);
////     tp->sockets.erase(tp->sockets.begin() + index);
////     tp->socket_addr.erase(tp->socket_addr.begin() + index);
//// }
//// 
//// void tp_destroy(THIRD_PARTY* tp)
//// {
////     while (!tp->sockets.empty())
////     {
////         close(tp->sockets.back());
////         tp->sockets.pop_back();
////     }
////     tp->addrs.clear();
//// }