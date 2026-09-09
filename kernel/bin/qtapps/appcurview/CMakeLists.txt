cmake_minimum_required(VERSION 3.16)
project(appcurview VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(appcurview 
    main.cpp
    CursorViewer.cpp
)

target_link_libraries(appcurview PRIVATE Qt6::Widgets)

