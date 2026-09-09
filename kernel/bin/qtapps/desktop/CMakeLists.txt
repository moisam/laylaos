cmake_minimum_required(VERSION 3.16)
project(desktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_AUTOMOC OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# The Desktop Background Executable
add_executable(desktop-bg
    desktop_main.cpp
    desktop_window.cpp
    desktop_icon.cpp
)
target_link_libraries(desktop-bg PRIVATE Qt6::Widgets)

# The Bottom Panel Executable
add_executable(desktop-panel
    panel_main.cpp
    panel_window.cpp
    panel_alt_tab.cpp
)
target_link_libraries(desktop-panel PRIVATE Qt6::Widgets)

