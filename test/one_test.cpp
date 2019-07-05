#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <adapter/AsioSelect.hpp>
#include "legacy.hpp"

namespace asio=boost::asio;

TEST(one_test, EmptySelectInfoExit)
{
    using boost::asio::ip::udp;
    using boost::system::error_code;
    using ::testing::_;
    using ::testing::Return;
    using ::testing::NotNull;

    asio::io_context io;
    auto myAdapter = adapter::AsioSelect::make(io);

    legacy_initialize();
    LegacyWrapper legacy;
    EXPECT_CALL(legacy, select_info(NotNull(), NotNull(), NotNull(), NotNull())).WillOnce(Return(0));
    myAdapter->setup(legacy_dispatch_wrapper, legacy_select_info);

    //No sockets or timers scheduled in select_info, should have no work and return.
    io.run();
}

TEST(one_test, SelectTimeval)
{
    using boost::asio::ip::udp;
    using boost::system::error_code;
    using ::testing::_;
    using ::testing::Return;
    using ::testing::NotNull;
    using ::testing::SetArgPointee;
    using ::testing::DoAll;

    asio::io_context io;
    auto myAdapter = adapter::AsioSelect::make(io);

    legacy_initialize();
    LegacyWrapper legacy;
    EXPECT_CALL(legacy, select_info(NotNull(), NotNull(), NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgPointee<2>(timeval{0,100}), SetArgPointee<3>(0), Return(0)))
        .RetiresOnSaturation();
    myAdapter->setup(legacy_dispatch_wrapper, legacy_select_info);


    EXPECT_CALL(legacy, dispatch(0, NotNull()))
        .WillOnce(Return(0));

    EXPECT_CALL(legacy, select_info(NotNull(), NotNull(), NotNull(), NotNull()))
        .WillOnce(Return(0));

    //No sockets or timers scheduled in select_info, should have no work and return.
    io.run();
}


TEST(one_test, NonGlobalLambda)
{
    using boost::asio::ip::udp;
    using boost::system::error_code;
    using ::testing::_;
    using ::testing::Return;
    using ::testing::NotNull;
    using ::testing::SetArgPointee;
    using ::testing::DoAll;

    asio::io_context io;
    auto myAdapter = adapter::AsioSelect::make(io);

    legacy_initialize();
    LegacyWrapper legacy;
    EXPECT_CALL(legacy, select_info(NotNull(), NotNull(), NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgPointee<2>(timeval{0,100}), SetArgPointee<3>(0), Return(0)))
        .RetiresOnSaturation();


    myAdapter->setup(
        [&legacy](int num_ready, fd_set* read_fds, boost::system::error_code ec){
            return legacy.dispatch(num_ready, read_fds);
        }, 
        [&legacy](int* num_fds, fd_set* fds, struct timeval* tv, int* blocking){
            return legacy.select_info(num_fds, fds, tv, blocking);}
        );

    EXPECT_CALL(legacy, dispatch(0, NotNull()))
        .WillOnce(Return(0));

    EXPECT_CALL(legacy, select_info(NotNull(), NotNull(), NotNull(), NotNull()))
        .WillOnce(Return(0));

    //No sockets or timers scheduled in select_info, should have no work and return.
    io.run();
}


