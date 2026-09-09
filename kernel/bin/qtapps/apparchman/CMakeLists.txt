cmake_minimum_required(VERSION 3.16)
project(apparchman VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Dependencies
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LibArchive REQUIRED IMPORTED_TARGET libarchive)

add_executable(apparchman
    main.cpp
    archive_backend.cpp
    archive_backend.hpp
)

target_link_libraries(apparchman 
    PRIVATE 
        Qt6::Widgets
        Qt6::Core
        PkgConfig::LibArchive
)

