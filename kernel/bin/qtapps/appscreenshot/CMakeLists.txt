cmake_minimum_required(VERSION 3.16)
project(appscreenshot VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(appscreenshot 
    main.cpp
    screenshotwidget.cpp
    selectionoverlay.cpp
    previewdialog.cpp
)

target_link_libraries(appscreenshot PRIVATE Qt6::Widgets)

