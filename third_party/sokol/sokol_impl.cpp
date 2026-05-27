// sokol_impl.cpp
// Compiled once so that all sokol headers emit their implementations.
// Must be built as a plain C++ translation unit (not included from elsewhere).

#if defined(__APPLE__)
  #define SOKOL_METAL
#else
  #define SOKOL_GLCORE
#endif

#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
