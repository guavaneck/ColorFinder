#pragma once
#include "app.h"
#include "uiconfig.h"
#include <sokol_app.h>

// ----- UI lifecycle -----

// Call once after sokol_gfx is initialized.
void ui_init(const UIConfig &cfg);

// Call once at shutdown.
void ui_shutdown();

// ----- Per-frame -----

// Draw everything. fb_width/fb_height are the framebuffer pixel dimensions.
void ui_draw(AppState *state,
             const UIConfig &cfg,
             const std::vector<SidebarItem> &sidebar,
             float fb_width, float fb_height);

// ----- Input -----

void ui_on_key(AppState *state, const UIConfig &cfg,
               const sapp_event *e);

void ui_on_mouse_move(AppState *state, const UIConfig &cfg,
                      const std::vector<SidebarItem> &sidebar,
                      const sapp_event *e,
                      float fb_width, float fb_height);

void ui_on_mouse_btn(AppState *state, const UIConfig &cfg,
                     const std::vector<SidebarItem> &sidebar,
                     const sapp_event *e,
                     float fb_width, float fb_height);

void ui_on_scroll(AppState *state, const sapp_event *e);

void ui_on_char(AppState *state, const sapp_event *e);
