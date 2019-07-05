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

#ifdef INTERACTIVE_TEST
// Obviously, waiting for the user to input something is 
// not workable for a real automated testcase.
// But it was a good first test, just to show the mechanism is working...
TEST(one_test, WaitForStdIn)
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
    fd_set test_read_fds;
    FD_ZERO(&test_read_fds);
    FD_SET(STDIN_FILENO, &test_read_fds);

    EXPECT_CALL(legacy, select_info(NotNull(), NotNull(), NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgPointee<0>(STDIN_FILENO), SetArgPointee<1>(test_read_fds), Return(0)))
        .RetiresOnSaturation();

    myAdapter->setup(legacy_dispatch_wrapper, legacy_select_info);

    EXPECT_CALL(legacy, dispatch(1, NotNull()))
        .WillOnce(Return(0));

    EXPECT_CALL(legacy, select_info(NotNull(), NotNull(), NotNull(), NotNull()))
        .WillOnce(Return(0));

    //
    io.run();

}
#endif

bool operator==(const fd_set& lhs, const fd_set& rhs)
{
    return (0 == memcmp(&lhs, &rhs, sizeof(fd_set)));
}

TEST(one_test, WaitUdpSocket)
{
    using timer = boost::asio::steady_timer;

    using boost::asio::ip::udp;
    using boost::system::error_code;
    using ::testing::_;
    using ::testing::Return;
    using ::testing::NotNull;
    using ::testing::SetArgPointee;
    using ::testing::DoAll;
    using ::testing::Pointee;
    using ::testing::InvokeWithoutArgs;

    asio::io_context io;
    auto myAdapter = adapter::AsioSelect::make(io);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GT(sockfd, 0);
    sockaddr_in bind_addr;
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = 0; // ephemeral port - OS will pick actual portno.
    bind_addr.sin_addr.s_addr = 0; 

    int bind_result = bind(sockfd, (sockaddr*)&bind_addr, sizeof(sockaddr_in));
    if(bind_result < 0)
        perror("Server socket binding");
    ASSERT_GE(bind_result, 0);

    socklen_t server_socklen = (socklen_t)sizeof(sockaddr_in);
    ASSERT_GE(getsockname(sockfd, (sockaddr*)&bind_addr, &server_socklen), 0);

    auto loopback = boost::asio::ip::make_address_v4("127.0.0.1");
    udp::endpoint server_endpoint(loopback, ntohs(bind_addr.sin_port));

    // open a client ephemeral socket.
    udp::socket client(io, udp::endpoint(loopback, 0));

    // use a timer to trigger a messag while we are socket waiting.
    timer trigger(io, std::chrono::milliseconds(100));

    LegacyWrapper legacy;
    fd_set test_read_fds;
    FD_ZERO(&test_read_fds);
    FD_SET(sockfd, &test_read_fds);

    EXPECT_CALL(legacy, select_info(NotNull(), NotNull(), NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgPointee<0>(sockfd), SetArgPointee<1>(test_read_fds), Return(0)))
        .RetiresOnSaturation();

    myAdapter->setup(legacy_dispatch_wrapper, legacy_select_info);

    char buffer[128]={0};
    const char* cbptr=buffer;

    sockaddr source_addr;
    socklen_t socklen = (socklen_t)sizeof(sockaddr);
    memset(&source_addr, 0, socklen);

    auto foo=[sockfd, &buffer, &source_addr, &socklen](){
        recvfrom(sockfd, buffer, 128, 0, &source_addr, &socklen);
    };

    EXPECT_CALL(legacy, dispatch(1, Pointee(test_read_fds)))
        .WillOnce(DoAll(InvokeWithoutArgs(foo), Return(0)));

    EXPECT_CALL(legacy, select_info(NotNull(), NotNull(), NotNull(), NotNull()))
        .WillOnce(Return(0));

    std::string data("sample buffer");

    trigger.async_wait([&client, &server_endpoint, &data](boost::system::error_code){
        client.send_to(asio::buffer(data), server_endpoint);
        });
    //
    io.run();

    ASSERT_EQ(data, cbptr);
}

