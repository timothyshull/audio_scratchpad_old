cmake_minimum_required(VERSION 3.3)
project(audio_programming)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(/Users/skull/Documents/Programming/C/Downloads/portsf/include)
link_directories(/Users/skull/Documents/Programming/C/Downloads/portsf/portsf)

add_executable(sf2float sf2float_orig.c)
target_link_libraries(sf2float libportsf.a)