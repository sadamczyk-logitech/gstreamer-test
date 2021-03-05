cmake_minimum_required(VERSION 3.17)
project(gstreamer_tutorial C)

set(CMAKE_C_STANDARD 99)
find_package(PkgConfig)
pkg_check_modules(GST REQUIRED gstreamer-1.0)

include_directories(${GST_INCLUDE_DIRS})
add_compile_options(${GST_CFLAGS})
link_directories(${GST_LIBRARY_DIRS})
link_libraries(${GST_LIBRARIES})

add_executable(gstreamer_tutorial main.c)
add_executable(bin bin.c)
