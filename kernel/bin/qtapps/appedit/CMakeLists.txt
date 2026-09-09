cmake_minimum_required(VERSION 3.16)
project(appedit VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HUNSPELL REQUIRED hunspell)

add_executable(appedit
    main.cpp
    editor.cpp
    editor.hpp
)

target_include_directories(appedit PRIVATE ${HUNSPELL_INCLUDE_DIRS})
target_link_libraries(appedit PRIVATE Qt6::Widgets Qt6::PrintSupport ${HUNSPELL_LIBRARIES})

