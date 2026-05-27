cmake_minimum_required(VERSION 3.16)
project(colorfinder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

add_executable(colorfinder
  main.cpp
  ui.cpp
  callbacks.cpp
  operations.cpp
  config.cpp
)

target_link_libraries(colorfinder ${GTK3_LIBRARIES})
