include_directories("${PROJECT_SOURCE_DIR}")

# First Simple Google Test
add_executable(first one_test.cpp legacy.cpp)
target_include_directories(first PRIVATE ../include ${Boost_INCLUDE_DIRS})
target_link_libraries(first gtest gmock_main ${Boost_LIBRARIES} selectadapter)
if(WIN32)
target_link_libraries(first Ws2_32)
endif()
add_test(NAME first_test COMMAND first)