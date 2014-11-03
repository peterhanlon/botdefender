cmake_minimum_required(VERSION 2.8.4)
project(c)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")

set(SOURCE_FILES common.c mod_collector.c mod_collector_comms.c mod_blocker.c)
add_executable(c ${SOURCE_FILES})

INCLUDE_DIRECTORIES(/usr/local/apache2/include)
LINK_DIRECTORIES(/usr/local/apache2/lib)