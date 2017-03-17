cmake_minimum_required(VERSION 3.6)
project(audiooutput)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(QT_PATH "/usr/local/opt/qt5")

set(QT_LIB_DIR ${QT_PATH}/lib/cmake)

set(Qt5Core_DIR "${QT_LIB_DIR}/Qt5Core")
set(Qt5Multimedia_DIR "${QT_LIB_DIR}/Qt5Multimedia")
set(Qt5Widgets_DIR "${QT_LIB_DIR}/Qt5Widgets")

set(QT_LIBRARIES
        Qt5::Core
        Qt5::Multimedia
        Qt5::Widgets)

set(QT_LIBRARIES_REQUIRED
        Qt5Core
        Qt5Multimedia
        Qt5Widgets)

foreach (QT_LIBRARIES_REQUIRED ${QT_LIBRARIES_REQUIRED})
    find_package(${QT_LIBRARIES_REQUIRED} REQUIRED)
endforeach ()

# qt5_add_resources(RESOURCE_FILES qml.qrc)

set(SOURCE_FILES main.cpp audiooutput.h audiooutput.cpp)
add_executable(audiooutput ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} ${QT_LIBRARIES})