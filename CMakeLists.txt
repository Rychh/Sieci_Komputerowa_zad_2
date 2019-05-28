cmake_minimum_required(VERSION 3.12)

project(Siki)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "-lboost_program_options -Wall -Wextra -O2 -std=c++17")
find_package(Boost COMPONENTS program_options filesystem REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(server server.cpp helper.h)
add_executable(client client.cpp helper.h)

target_link_libraries(server ${Boost_LIBRARIES} )
target_link_libraries(client ${Boost_LIBRARIES} )