cmake_minimum_required(VERSION 3.16)
project(colorfinder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----- Platform detection -----

if(APPLE)
  set(PLATFORM_MACOS TRUE)
elseif(UNIX)
  set(PLATFORM_LINUX TRUE)
endif()

# ----- Sokol (header-only, vendored under third_party/sokol/) -----
# Fetch: https://github.com/floooh/sokol  (just the headers you need)

add_library(sokol_impl STATIC third_party/sokol/sokol_impl.cpp)
target_include_directories(sokol_impl PUBLIC third_party/sokol)

if(PLATFORM_LINUX)
  target_compile_definitions(sokol_impl PUBLIC SOKOL_GLCORE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GL     REQUIRED gl)
  pkg_check_modules(X11    REQUIRED x11)
  pkg_check_modules(XI     REQUIRED xi)
  pkg_check_modules(XCURSOR REQUIRED xcursor)
  target_link_libraries(sokol_impl PUBLIC
    ${GL_LIBRARIES} ${X11_LIBRARIES} ${XI_LIBRARIES} ${XCURSOR_LIBRARIES}
    dl pthread
  )
  target_include_directories(sokol_impl PUBLIC
    ${GL_INCLUDE_DIRS} ${X11_INCLUDE_DIRS}
  )
elseif(PLATFORM_MACOS)
  target_compile_definitions(sokol_impl PUBLIC SOKOL_METAL)
  target_link_libraries(sokol_impl PUBLIC
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework QuartzCore"
  )
endif()

# ----- stb (header-only, vendored under third_party/stb/) -----
# stb_image.h and stb_truetype.h from https://github.com/nothings/stb

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE third_party/stb)

# ----- App sources -----

add_executable(colorfinder
  main.cpp
  app.cpp
  ui.cpp
  operations.cpp
  config.cpp
  uiconfig.cpp
)

target_include_directories(colorfinder PRIVATE
  third_party/sokol
  third_party/stb
)

target_link_libraries(colorfinder PRIVATE sokol_impl)

if(PLATFORM_LINUX)
  target_link_libraries(colorfinder PRIVATE
    ${GL_LIBRARIES} ${X11_LIBRARIES} dl pthread
  )
elseif(PLATFORM_MACOS)
  target_link_libraries(colorfinder PRIVATE
    "-framework Metal" "-framework MetalKit"
    "-framework Cocoa" "-framework QuartzCore"
  )
  set_target_properties(colorfinder PROPERTIES MACOSX_BUNDLE TRUE)
endif()
