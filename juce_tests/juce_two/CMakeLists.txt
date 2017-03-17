cmake_minimum_required(VERSION 3.5)
project(juce_two)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    JuceLibraryCode/modules/juce_audio_basics/juce_audio_basics.h
    JuceLibraryCode/modules/juce_audio_devices/juce_audio_devices.h
    JuceLibraryCode/modules/juce_audio_formats/juce_audio_formats.h
    JuceLibraryCode/modules/juce_audio_processors/juce_audio_processors.h
    JuceLibraryCode/modules/juce_audio_utils/juce_audio_utils.h
    JuceLibraryCode/modules/juce_core/juce_core.h
    JuceLibraryCode/modules/juce_cryptography/juce_cryptography.h
    JuceLibraryCode/modules/juce_data_structures/juce_data_structures.h
    JuceLibraryCode/modules/juce_events/juce_events.h
    JuceLibraryCode/modules/juce_graphics/juce_graphics.h
    JuceLibraryCode/modules/juce_gui_basics/juce_gui_basics.h
    JuceLibraryCode/modules/juce_gui_extra/juce_gui_extra.h
    JuceLibraryCode/modules/juce_opengl/juce_opengl.h
    JuceLibraryCode/modules/juce_video/juce_video.h
    JuceLibraryCode/AppConfig.h
    JuceLibraryCode/JuceHeader.h
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent_02.cpp
    Source/MainComponent_03.cpp)

add_executable(juce_two ${SOURCE_FILES})