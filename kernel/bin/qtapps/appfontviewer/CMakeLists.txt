cmake_minimum_required(VERSION 3.16)
project(appfontviewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(appfontviewer main.cpp)

target_link_libraries(appfontviewer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

