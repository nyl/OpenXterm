cmake_minimum_required(VERSION 3.21)

project(OpenXterm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBVNCCLIENT REQUIRED libvncclient)
pkg_check_modules(QTERMWIDGET REQUIRED qtermwidget6)

add_executable(OpenXterm
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/SessionEditorDialog.cpp
    src/app/SessionEditorDialog.h
    src/domain/SessionProfile.h
    src/domain/SessionRepository.cpp
    src/domain/SessionRepository.h
    src/runtime/SshTerminalWidget.cpp
    src/runtime/SshTerminalWidget.h
    src/runtime/VncSessionWidget.cpp
    src/runtime/VncSessionWidget.h
    src/runtime/VncClientWorker.cpp
    src/runtime/VncClientWorker.h
)

target_include_directories(OpenXterm PRIVATE ${LIBVNCCLIENT_INCLUDE_DIRS} ${QTERMWIDGET_INCLUDE_DIRS})
target_link_directories(OpenXterm PRIVATE ${LIBVNCCLIENT_LIBRARY_DIRS} ${QTERMWIDGET_LIBRARY_DIRS})

target_link_libraries(OpenXterm
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        ${LIBVNCCLIENT_LIBRARIES}
        ${QTERMWIDGET_LIBRARIES}
)

if(MSVC)
    target_compile_options(OpenXterm PRIVATE /W4)
else()
    target_compile_options(OpenXterm PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(OpenXterm PRIVATE util)
endif()

install(TARGETS OpenXterm RUNTIME DESTINATION bin)
