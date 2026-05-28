#pragma once
#include "app.h"
#include "uiconfig.h"
#include <sokol_app.h>

// ----- UI lifecycle -----

void ui_init(const UIConfig &cfg);
void ui_shutdown();

// ----- Per-frame -----

void ui_draw(AppState *state,
             const UIConfig &cfg,
             const std::vector<SidebarItem> &sidebar,
             float fb_width, float fb_height);

// ----- Input -----

// Forward any sokol event that isn't a key_down to ImGui.
void ui_handle_event(const sapp_event *e);

// Handle key_down: forwards to ImGui then applies app keybinds.
void ui_on_key(AppState *state, const UIConfig &cfg, const sapp_event *e);
