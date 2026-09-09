cmake_minimum_required(VERSION 3.16)
project(apppaint VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

add_executable(apppaint
    main.cpp
    paintcanvas.h
    paintcanvas.cpp
    paintmainwindow.h
    paintmainwindow.cpp
)

target_link_libraries(apppaint PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
)

