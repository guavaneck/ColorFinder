#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#include <sokol_log.h>

#include "app.h"
#include "ui.h"
#include "config.h"
#include "uiconfig.h"

// ----- App globals -----

static AppState               *g_state   = nullptr;
static UIConfig                g_uicfg;
static std::vector<SidebarItem> g_sidebar;

// ----- sokol_app callbacks -----

static void init_cb() {
  sg_desc desc = {};
  desc.environment = sglue_environment();
  desc.logger.func = slog_func;
  sg_setup(&desc);

  auto cfg    = config_load();
  g_uicfg     = uiconfig_load();
  g_state     = app_state_new(std::move(cfg));
  g_sidebar   = sidebar_default_items();

  ui_init(g_uicfg);
}

static void frame_cb() {
  float fw = (float)sapp_width();
  float fh = (float)sapp_height();
  ui_draw(g_state, g_uicfg, g_sidebar, fw, fh);
}

static void event_cb(const sapp_event *e) {
  float fw = (float)sapp_width();
  float fh = (float)sapp_height();

  switch (e->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
      ui_on_key(g_state, g_uicfg, e);
      break;
    case SAPP_EVENTTYPE_CHAR:
      ui_on_char(g_state, e);
      break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
      ui_on_mouse_move(g_state, g_uicfg, g_sidebar, e, fw, fh);
      break;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
      ui_on_mouse_btn(g_state, g_uicfg, g_sidebar, e, fw, fh);
      break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL: {
      // Inline scroll here so we have fw/fh
      extern void ui_scroll(AppState*, const UIConfig&, const sapp_event*, float, float);
      ui_scroll(g_state, g_uicfg, e, fw, fh);
      break;
    }
    default: break;
  }
}

static void cleanup_cb() {
  ui_shutdown();
  app_state_free(g_state);
  sg_shutdown();
}

// ----- Entry point -----

sapp_desc sokol_main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  sapp_desc d = {};
  d.init_cb    = init_cb;
  d.frame_cb   = frame_cb;
  d.event_cb   = event_cb;
  d.cleanup_cb = cleanup_cb;
  d.width      = 900;
  d.height     = 560;
  d.window_title = "colorfinder";
  d.logger.func  = slog_func;
  return d;
}
