cmake_minimum_required(VERSION 3.16)
project(settings-apps LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(desktop-settings-bg settings_background.cpp)
target_link_libraries(desktop-settings-bg PRIVATE Qt6::Widgets)

add_executable(desktop-settings-theme settings_color_theme.cpp)
target_link_libraries(desktop-settings-theme PRIVATE Qt6::Widgets)

add_executable(desktop-settings-sysinfo settings_sysinfo.cpp)
target_link_libraries(desktop-settings-sysinfo PRIVATE Qt6::Widgets)

add_executable(desktop-settings-display settings_display.cpp)
target_link_libraries(desktop-settings-display PRIVATE Qt6::Widgets)

add_executable(desktop-settings-main settings_main.cpp)
target_link_libraries(desktop-settings-main PRIVATE Qt6::Widgets)

