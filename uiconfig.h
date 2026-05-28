#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>

// ----- Parsed color -----

struct Color {
  uint8_t r, g, b, a;
};

Color color_from_hex(const std::string &hex);  // RRGGBB or RRGGBBAA

// ----- UIConfig -----

struct UIConfig {
  // Font
  std::string font_path;
  float       font_size       = 12.f;

  // Colors
  Color color_bg              = {26,  26,  26,  255};
  Color color_panel_bg        = {20,  20,  20,  255};
  Color color_sidebar_bg      = {17,  17,  17,  255};
  Color color_border          = {42,  42,  42,  255};
  Color color_text            = {208, 208, 208, 255};
  Color color_text_dim        = {96,  96,  96,  255};
  Color color_text_selected   = {255, 255, 255, 255};
  Color color_selection_bg    = {42,  74,  122, 255};
  Color color_hover_bg        = {34,  34,  34,  255};
  Color color_scrollbar       = {42,  42,  42,  255};
  Color color_scrollbar_fg    = {68,  68,  68,  255};
  Color color_status_bg       = {15,  15,  15,  255};
  Color color_status_text     = {128, 128, 128, 255};
  Color color_warning_text    = {136, 136, 136, 255};

  // Layout
  float sidebar_width         = 1.f;
  float preview_width         = 200.f;
  float min_width_warning     = 300.f;
  float min_width_preview     = 640.f;
  float row_height            = 22.f;
  float status_height         = 20.f;
  float row_padding_x         = 8.f;
  float row_padding_y         = 3.f;
  float icon_label_gap        = 6.f;

  // Sprites
  std::string icons_sheet     = "third_party/assets/icons.png";
  int         icon_size       = 16;

  // Keybinds (raw strings from config, resolved at input time)
  std::string kb_up           = "up";
  std::string kb_down         = "down";
  std::string kb_left         = "left";
  std::string kb_right        = "right";
  std::string kb_up_alt       = "k";
  std::string kb_down_alt     = "j";
  std::string kb_left_alt     = "h";
  std::string kb_right_alt    = "l";
  std::string kb_enter        = "return";
  std::string kb_back         = "backspace";
  std::string kb_go_home      = "~";
  std::string kb_delete       = "delete";
  std::string kb_rename       = "f2";
  std::string kb_new_file     = "ctrl+n";
  std::string kb_new_folder   = "ctrl+shift+n";
  std::string kb_copy         = "ctrl+c";
  std::string kb_cut          = "ctrl+x";
  std::string kb_paste        = "ctrl+v";
  std::string kb_select_all   = "ctrl+a";
  std::string kb_toggle_preview = "tab";
};

// ----- Loader -----

// Loads uiconfig from the platform config dir, creating defaults if absent.
UIConfig uiconfig_load();

// Looks up a raw key/value from the flat map.
std::string uiconfig_get(const std::unordered_map<std::string, std::string> &map,
                         const std::string &key,
                         const std::string &fallback = "");
