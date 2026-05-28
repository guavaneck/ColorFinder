// sokol_impl.cpp
// Compiled once so sokol headers emit their implementations.
// Also emits the sokol_imgui backend implementation here.

#if defined(__APPLE__)
  #define SOKOL_METAL
#else
  #define SOKOL_GLCORE
  #define SOKOL_LINUX_WAYLAND
#endif

#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"

#include "../imgui/imgui.h"

#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
