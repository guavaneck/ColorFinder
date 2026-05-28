#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#include <sokol_log.h>

#include "app.h"
#include "ui.h"
#include "config.h"
#include "uiconfig.h"

// ----- app globals -----

static AppState                *g_state   = nullptr;
static UIConfig                 g_uicfg;
static std::vector<SidebarItem> g_sidebar;

// ----- sokol_app callbacks -----

static void init_cb() {
  sg_desc desc = {};
  desc.environment = sglue_environment();
  desc.logger.func = slog_func;
  sg_setup(&desc);

  auto cfg  = config_load();
  g_uicfg   = uiconfig_load();
  g_state   = app_state_new(std::move(cfg));
  g_state->scroll_y = 0.0f;
  g_state->scroll_target_y = 0.0f;
  g_state->scroll_follow_selection = false;

  g_sidebar = sidebar_default_items();

  ui_init(g_uicfg);
}

static void frame_cb() {
  ui_draw(g_state, g_uicfg, g_sidebar,
          (float)sapp_width(), (float)sapp_height());
}

static void event_cb(const sapp_event *e) {
  ui_handle_event(e);
}

static void cleanup_cb() {
  ui_shutdown();
  app_state_free(g_state);
  sg_shutdown();
}

// ----- entry point -----

sapp_desc sokol_main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  sapp_desc d    = {};
  d.init_cb      = init_cb;
  d.frame_cb     = frame_cb;
  d.event_cb     = event_cb;
  d.cleanup_cb   = cleanup_cb;
  d.width        = 900;
  d.height       = 560;
  d.window_title = "colorfinder";
  d.logger.func  = slog_func;
  return d;
}
