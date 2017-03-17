cmake_minimum_required(VERSION 3.6)
project(juce_test)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ObjC++")

set(JUCE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/JuceLibraryCode)
file(GLOB_RECURSE JUCE_SOURCES
        RELATIVE
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${JUCE_SOURCE_DIR}/*.cpp"
        "${JUCE_SOURCE_DIR}/*.h"
        "${JUCE_SOURCE_DIR}/*.mm"
        )

include_directories(${JUCE_SOURCE_DIR})

include_directories(
        JuceLibraryCode
        JUCE/modules
        JUCE/modules/juce_audio_basics
        JUCE/modules/juce_audio_devices
        JUCE/modules/juce_audio_formats
        JUCE/modules/juce_audio_plugin_client
        JUCE/modules/juce_audio_processors
        JUCE/modules/juce_audio_utils
        JUCE/modules/juce_blocks_basics
        JUCE/modules/juce_box2d
        JUCE/modules/juce_core
        JUCE/modules/juce_cryptography
        JUCE/modules/juce_data_structures
        JUCE/modules/juce_events
        JUCE/modules/juce_graphics
        JUCE/modules/juce_gui_basics
        JUCE/modules/juce_gui_extra
        JUCE/modules/juce_opengl
        JUCE/modules/juce_osc
        JUCE/modules/juce_tracktion_marketplace
        JUCE/modules/juce_video
)

add_library(juce STATIC ${JUCE_SOURCES})

set(SOURCE_FILES main.cpp MainContentComponent.cpp)

add_executable(juce_test ${SOURCE_FILES})
target_link_libraries(juce_test juce)

target_link_libraries(juce_test
        "-framework Carbon"
        "-framework Cocoa"
        "-framework AppKit"
        "-framework CoreData"
        "-framework Foundation"
        "-framework OpenGL"
        "-framework IOKit"
        "-framework QuartzCore"
        "-framework WebKit"
        )
