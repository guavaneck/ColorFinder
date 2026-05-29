#include "uiconfig.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

// ----- color_from_hex -----

Color color_from_hex(const std::string &hex) {
  std::string h = hex;
  if (!h.empty() && h[0] == '#') h = h.substr(1);

  auto byte = [&](int pos) -> uint8_t {
    if (pos + 1 >= (int)h.size()) return 0;
    return (uint8_t)std::stoi(h.substr(pos, 2), nullptr, 16);
  };

  Color c;
  c.r = byte(0);
  c.g = byte(2);
  c.b = byte(4);
  c.a = h.size() >= 8 ? byte(6) : 255;
  return c;
}

// ----- Config file location -----

static fs::path uiconfig_path() {
  const char *home = std::getenv("HOME");
  if (!home) return fs::current_path() / "uiconfig";

#if defined(__APPLE__)
  return fs::path(home) / "Library/Application Support/colorfinder/uiconfig";
#else
  const char *xdg = std::getenv("XDG_CONFIG_HOME");
  fs::path base = (xdg && *xdg) ? fs::path(xdg) : fs::path(home) / ".config";
  return base / "colorfinder/uiconfig";
#endif
}

// ----- Raw map loader (same format as config.cpp) -----

static std::unordered_map<std::string, std::string>
load_raw(const fs::path &path) {
  std::unordered_map<std::string, std::string> map;
  std::ifstream in(path);
  if (!in) return map;

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::string key, eq, value;
    if (ss >> key && key[0] != '#' && ss >> eq && eq == "=" &&
        std::getline(ss >> std::ws, value))
      map[key] = value;
  }
  return map;
}

std::string uiconfig_get(const std::unordered_map<std::string, std::string> &map,
                         const std::string &key,
                         const std::string &fallback) {
  auto it = map.find(key);
  return it != map.end() ? it->second : fallback;
}

// ----- uiconfig_load -----

// Default file content — mirrors the uiconfig template shipped with the project.
static const char *DEFAULT_UICONFIG = R"(# uiconfig
font_path =
font_size = 12
color_bg = 1a1a1a
color_panel_bg = 141414
color_sidebar_bg = 111111
color_border = 2a2a2a
color_text = d0d0d0
color_text_dim = 606060
color_text_selected = ffffff
color_selection_bg = 2a4a7a
color_scrollbar = 2a2a2a
color_scrollbar_fg = 444444
color_status_bg = 0f0f0f
color_status_text = 808080
color_warning_text = 888888
sidebar_width = 160
preview_width = 200
min_width_warning = 300
min_width_preview = 640
row_height = 22
status_height = 20
row_padding_x = 8
row_padding_y = 3
icon_label_gap = 6
icons_sheet = icons.png
icon_size = 16
keybind_up = up
keybind_down = down
keybind_left = left
keybind_right = right
keybind_up_alt = k
keybind_down_alt = j
keybind_left_alt = h
keybind_right_alt = l
keybind_enter = return
keybind_back = backspace
keybind_go_home = ~
keybind_delete = delete
keybind_rename = f2
keybind_new_file = ctrl+n
keybind_new_folder = ctrl+shift+n
keybind_copy = ctrl+c
keybind_cut = ctrl+x
keybind_paste = ctrl+v
keybind_select_all = ctrl+a
keybind_toggle_preview = tab
)";

UIConfig uiconfig_load() {
  UIConfig cfg;
  fs::path path = uiconfig_path();

  // create config if missing
  if (!fs::exists(path)) {
    fs::create_directories(path.parent_path());

    std::ofstream out(path);
    if (out) {
      out << DEFAULT_UICONFIG;
    } else {
      std::cerr << "Warning: could not create uiconfig at " << path << "\n";
    }
  }
  // ensure icons exist every run
  try {
    fs::path exe =
      fs::read_symlink("/proc/self/exe");
    fs::path bin_dir = exe.parent_path();
    fs::path icons_src =
      fs::weakly_canonical(
          bin_dir / "../third_party/assets/icons.png"
      );
    fs::path icons_dst =
      path.parent_path() / "icons.png";
    if (!fs::exists(icons_dst) &&
      fs::exists(icons_src)) {
      fs::copy_file(
          icons_src,
          icons_dst,
          fs::copy_options::skip_existing
      );
    }

  }
  catch (const fs::filesystem_error& e) {
    std::cerr << "Icon copy failed: "
              << e.what() << "\n";
  }

  auto map = load_raw(path);

  auto get = [&](const std::string &k, const std::string &fb) {
    return uiconfig_get(map, k, fb);
  };
  auto getf = [&](const std::string &k, float fb) -> float {
    auto it = map.find(k);
    if (it == map.end()) return fb;
    try { return std::stof(it->second); } catch (...) { return fb; }
  };
  auto getc = [&](const std::string &k, Color fb) -> Color {
    auto it = map.find(k);
    if (it == map.end() || it->second.empty()) return fb;
    try { return color_from_hex(it->second); } catch (...) { return fb; }
  };

  cfg.font_path   = get("font_path", "");
  cfg.font_size   = getf("font_size", 12.f);

  cfg.color_bg            = getc("color_bg",            cfg.color_bg);
  cfg.color_panel_bg      = getc("color_panel_bg",      cfg.color_panel_bg);
  cfg.color_sidebar_bg    = getc("color_sidebar_bg",    cfg.color_sidebar_bg);
  cfg.color_border        = getc("color_border",        cfg.color_border);
  cfg.color_text          = getc("color_text",          cfg.color_text);
  cfg.color_text_dim      = getc("color_text_dim",      cfg.color_text_dim);
  cfg.color_text_selected = getc("color_text_selected", cfg.color_text_selected);
  cfg.color_selection_bg  = getc("color_selection_bg",  cfg.color_selection_bg);
  cfg.color_scrollbar     = getc("color_scrollbar",     cfg.color_scrollbar);
  cfg.color_scrollbar_fg  = getc("color_scrollbar_fg",  cfg.color_scrollbar_fg);
  cfg.color_status_bg     = getc("color_status_bg",     cfg.color_status_bg);
  cfg.color_status_text   = getc("color_status_text",   cfg.color_status_text);
  cfg.color_warning_text  = getc("color_warning_text",  cfg.color_warning_text);

  cfg.sidebar_width       = getf("sidebar_width",       160.f);
  cfg.preview_width       = getf("preview_width",       200.f);
  cfg.min_width_warning   = getf("min_width_warning",   300.f);
  cfg.min_width_preview   = getf("min_width_preview",   640.f);
  cfg.row_height          = getf("row_height",          22.f);
  cfg.status_height       = getf("status_height",       20.f);
  cfg.row_padding_x       = getf("row_padding_x",       8.f);
  cfg.row_padding_y       = getf("row_padding_y",       3.f);
  cfg.icon_label_gap      = getf("icon_label_gap",      6.f);

  cfg.icons_sheet         = get("icons_sheet",  "icons.png");
  if (!fs::path(cfg.icons_sheet).is_absolute())
    cfg.icons_sheet = (path.parent_path() / cfg.icons_sheet).string();
  cfg.icon_size           = (int)getf("icon_size", 16.f);

  cfg.kb_up             = get("keybind_up",             "up");
  cfg.kb_down           = get("keybind_down",           "down");
  cfg.kb_left           = get("keybind_left",           "left");
  cfg.kb_right          = get("keybind_right",          "right");
  cfg.kb_up_alt         = get("keybind_up_alt",         "k");
  cfg.kb_down_alt       = get("keybind_down_alt",       "j");
  cfg.kb_left_alt       = get("keybind_left_alt",       "h");
  cfg.kb_right_alt      = get("keybind_right_alt",      "l");
  cfg.kb_enter          = get("keybind_enter",          "return");
  cfg.kb_back           = get("keybind_back",           "backspace");
  cfg.kb_go_home        = get("keybind_go_home",        "~");
  cfg.kb_delete         = get("keybind_delete",         "delete");
  cfg.kb_rename         = get("keybind_rename",         "f2");
  cfg.kb_new_file       = get("keybind_new_file",       "ctrl+n");
  cfg.kb_new_folder     = get("keybind_new_folder",     "ctrl+shift+n");
  cfg.kb_copy           = get("keybind_copy",           "ctrl+c");
  cfg.kb_cut            = get("keybind_cut",            "ctrl+x");
  cfg.kb_paste          = get("keybind_paste",          "ctrl+v");
  cfg.kb_select_all     = get("keybind_select_all",     "ctrl+a");
  cfg.kb_toggle_preview = get("keybind_toggle_preview", "tab");
  cfg.kb_open_file      = get("keybind_open_file",      "o");
  cfg.kb_open_search    = get("keybind_open_search",    "ctrl+f");

  return cfg;
}
