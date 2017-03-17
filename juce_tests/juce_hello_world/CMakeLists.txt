cmake_minimum_required(VERSION 3.6)
project(juce_hello_world)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(JUCE_SOURCE_DIR /Users/skull/Programming/Downloads/JUCE-master/modules)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
include_directories(${JUCE_SOURCE_DIR})

#find_library(RT rt)
#find_library(DL dl)
#find_library(X11 REQUIRED)
#find_library(Threads REQUIRED)
#find_library(Freetype REQUIRED)
#
#include_directories(AFTER ${FREETYPE_INCLUDE_DIRS})
#
#set(JUCE_LIBRARIES ${RT} ${DL} ${X11_LIBRARIES} ${FREETYPE_LIBRARIES})
set(JUCE_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/AppConfig.h
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/JuceHeader.h
        ${JUCE_SOURCE_DIR}/juce_core/juce_core.cpp
        ${JUCE_SOURCE_DIR}/juce_core/juce_core.mm
        ${JUCE_SOURCE_DIR}/juce_data_structures/juce_data_structures.cpp
        ${JUCE_SOURCE_DIR}/juce_data_structures/juce_data_structures.mm
        ${JUCE_SOURCE_DIR}/juce_events/juce_events.cpp
        ${JUCE_SOURCE_DIR}/juce_events/juce_events.mm
        ${JUCE_SOURCE_DIR}/juce_graphics/juce_graphics.cpp
        ${JUCE_SOURCE_DIR}/juce_graphics/juce_graphics.mm
        ${JUCE_SOURCE_DIR}/juce_gui_basics/juce_gui_basics.cpp
        ${JUCE_SOURCE_DIR}/juce_gui_basics/juce_gui_basics.mm
        ${JUCE_SOURCE_DIR}/juce_gui_extra/juce_gui_extra.cpp
        ${JUCE_SOURCE_DIR}/juce_gui_extra/juce_gui_extra.mm
        )

add_library(local_juce STATIC ${JUCE_SOURCE})
target_link_libraries(local_juce ${CMAKE_THREAD_LIBS_INIT})
#target_link_libraries(myproject_juce ${CMAKE_THREAD_LIBS_INIT} ${JUCE_LIBRARIES})

#FILE ( GLOB_RECURSE BLAH_SOURCE *.cpp )
#LIST ( SORT BLAH_SOURCE )
#INCLUDE_DIRECTORIES ( BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/JucerDialogs/ )

set(SOURCE_FILES main.cpp
        MainComponent.h
        MainComponent.cpp)
add_executable(juce_hello_world ${SOURCE_FILES})
target_link_libraries(juce_hello_world ${CMAKE_THREAD_LIBS_INIT} local_juce)