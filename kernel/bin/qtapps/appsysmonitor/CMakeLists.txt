cmake_minimum_required(VERSION 3.16)
project(appsysmonitor VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(appsysmonitor
    main.cpp
    SystemMonitorWindow.h
    SystemMonitorWindow.cpp
    ProcessWorker.h
    ProcessWorker.cpp
    HistoryGraph.h
    HistoryGraph.cpp
)

target_link_libraries(appsysmonitor PRIVATE Qt6::Widgets)

