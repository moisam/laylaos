cmake_minimum_required(VERSION 3.16)
project(systray-widgets LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(widget-battery widget_battery.cpp)
target_link_libraries(widget-battery PRIVATE Qt6::Widgets)

add_executable(widget-volume widget_volume.cpp)
target_link_libraries(widget-volume PRIVATE Qt6::Widgets)

