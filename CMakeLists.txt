cmake_minimum_required(VERSION 3.12)
project(siki)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads)
find_package(Boost COMPONENTS program_options system filesystem log REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
set(Boost_USE_STATIC_LIBS ON)

add_executable(netstore-client client.cpp helper.cpp)
target_link_libraries(netstore-client ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_executable(netstore-server server.cpp helper.cpp)
target_link_libraries(netstore-server ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

#[[
cmake_minimum_required(VERSION 3.11)

project(Siki)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "-lboost_program_options -Wall -Wextra -O2 -std=c++17")
find_package(Boost COMPONENTS program_options filesystem REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

#add_executable(err err.cpp err.h)
#add_executable(helper helper.cpp helper.h err.h)
add_executable(server server.cpp helper.cpp )
add_executable(client client.cpp helper.cpp)

target_link_libraries(server ${Boost_LIBRARIES} )
target_link_libraries(client ${Boost_LIBRARIES} )]]
