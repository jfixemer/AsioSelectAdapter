#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <adapter/AsioSelect.hpp>
#include "platform.hpp"


TEST(one_test, SimpleAdd)
{
    using ADAPTER= adapter::AsioSelect<boost::asio::ip::udp::socket>;
    ADAPTER::native_fd_t  native_fd = socket(AF_INET, SOCK_DGRAM, 0);

    ADAPTER adapter;

    adapter.add(native_fd);
}