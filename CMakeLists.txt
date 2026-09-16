cmake_minimum_required(VERSION 3.16)
project(cinema_pricing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(cinema_booking src/cinema.cpp)
target_include_directories(cinema_booking PUBLIC include)

add_executable(cinema_booking_tests tests/cinema_tests.cpp)
target_link_libraries(cinema_booking_tests PRIVATE cinema_booking)

add_executable(cinema_server src/server.cpp)
target_link_libraries(cinema_server PRIVATE cinema_booking)

enable_testing()
add_test(NAME cinema_booking_tests COMMAND cinema_booking_tests)