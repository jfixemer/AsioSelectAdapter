#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <adapter/AsioSelect.hpp>
#include "legacy.hpp"

namespace asio=boost::asio;

TEST(one_test, AdapterAsync)
{
    using boost::asio::ip::udp;
    using boost::system::error_code;

    asio::io_context io;
    auto myAdapter = adapter::AsioSelect::make(io);

    legacy_initialize();
    auto& legacy = LegacyWrapper::instance();
    // EXPECT_CALL(legacy, select_info...);
    myAdapter->setup(legacy_dispatch_wrapper, legacy_select_info);

    io.run();

}