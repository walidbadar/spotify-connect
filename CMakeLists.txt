cmake_minimum_required(VERSION 3.10)
project(spotify-connect)

# Enable warnings and optimization
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")

# Add your source file (adjust path if needed)
add_executable(spotify-connect ../spotify-connect.c)

# Find and link libcurl
find_package(CURL REQUIRED)
target_link_libraries(spotify-connect PRIVATE CURL::libcurl)
