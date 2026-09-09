cmake_minimum_required(VERSION 3.16)
project(apppdfviewer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# You might need to install libopenjp2 and jbig2dec.
# On Ubuntu: sudo apt install libmujs-dev libgumbo-dev libopenjp2-7-dev libjbig2dec0-dev libjpeg-dev zlib1g-dev libgumbo-dev

find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
pkg_check_modules(FREETYPE REQUIRED freetype2)
pkg_check_modules(OPENJPEG REQUIRED libopenjp2)
pkg_check_modules(JBIG2DEC REQUIRED jbig2dec)

if(NOT LAYLAOS)
    pkg_check_modules(MUJS REQUIRED mujs)
    pkg_check_modules(GUMBO REQUIRED gumbo)
    find_package(JPEG REQUIRED)
endif()

add_executable(apppdfviewer main.cpp)

target_compile_definitions(apppdfviewer PRIVATE FZ_ENABLE_DOCX_OUTPUT=0)

target_include_directories(apppdfviewer PRIVATE 
    ${HARFBUZZ_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${OPENJPEG_INCLUDE_DIRS}
    ${JBIG2DEC_INCLUDE_DIRS}
    ${MUJS_INCLUDE_DIRS}
    ${GUMBO_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIRS}
)

target_link_libraries(apppdfviewer PRIVATE 
    Qt6::Widgets
    mupdf
    ${MUJS_LIBRARIES}
    ${GUMBO_LIBRARIES}
    ${HARFBUZZ_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${OPENJPEG_LIBRARIES}
    ${JBIG2DEC_LIBRARIES}
    ${JPEG_LIBRARIES}
    z
    m
)

if(NOT LAYLAOS)
    target_link_libraries(apppdfviewer PRIVATE 
        mupdf-third
    )
endif()

