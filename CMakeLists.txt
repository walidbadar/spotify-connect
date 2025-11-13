cmake_minimum_required(VERSION 3.10)
project(spotify-connect)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(spotify-connect
    ${CMAKE_CURRENT_SOURCE_DIR}/src/spotify-connect.c
)

# Find and link libcurl
find_package(CURL REQUIRED)
target_link_libraries(spotify-connect PRIVATE CURL::libcurl)
