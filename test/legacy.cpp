#include "legacy.hpp"
#include <memory>
#include <gtest/gtest.h>

LegacyWrapper* legacy_wrapper_ptr = nullptr;

LegacyWrapper* LegacyWrapper::instance() { return legacy_wrapper_ptr; }
LegacyWrapper::LegacyWrapper(): last_global(legacy_wrapper_ptr)
{
    legacy_wrapper_ptr = this;  
}

LegacyWrapper::~LegacyWrapper()
{
    legacy_wrapper_ptr = last_global;  
}

int legacy_initialize() { return 0;}

int legacy_select_info(int* num_fds, fd_set* fds, struct timeval* tv, int* blocking)
{
    return LegacyWrapper::instance()->select_info(num_fds, fds, tv, blocking);
}

int legacy_select_info(int* num_fds, fd_set* fds)
{
    return LegacyWrapper::instance()->select_info(num_fds, fds, NULL, NULL);
}

void legacy_read(fd_set* read_fds) { LegacyWrapper::instance()->read(read_fds); }
void legacy_timeout() { LegacyWrapper::instance()->timeout(); }

// Example functions
int legacy_dispatch(int num_ready, fd_set* read_fds)
{
    return LegacyWrapper::instance()->dispatch(num_ready, read_fds);
}

int legacy_dispatch_wrapper(int num_ready, fd_set* read_fds, boost::system::error_code)
{
    return LegacyWrapper::instance()->dispatch(num_ready, read_fds);
}
