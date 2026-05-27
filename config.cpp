#include "config.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

static const std::string DEFAULT_CONFIG = R"(
# colorfinder.conf
# -----------------

# --- Build ---

# Command used to compile the project.
compile_command = bash -c 'g++ ./*.cpp -o app $(pkg-config --cflags --libs gtk+-3.0)'


# --- Keybinds ---
# Use standard key names: Return, BackSpace, Delete, Escape, slash, etc.
# Modifiers: <ctrl>, <shift>, <alt>
# Examples: <ctrl>c, <shift>Delete, Return

# Navigation
keybind_enter_dir   = Return
keybind_back_dir    = BackSpace
keybind_open_search = <ctrl>f

# File actions
keybind_open_file     = o
keybind_delete        = Delete
keybind_rename        = F2
keybind_create_file   = <ctrl>n
keybind_create_folder = <ctrl><shift>n
keybind_copy          = <ctrl>c
keybind_cut           = <ctrl>x
keybind_paste         = <ctrl>v

# Selection
keybind_select_all = <ctrl>a


# --- Default openers ---
# Format: opener_ext_<extension> = <command>
# Use %f as a placeholder for the file path.
# opener_fallback is used when no extension match is found.

opener_fallback     = xdg-open %f

opener_ext_pdf      = zathura %f
opener_ext_png      = feh %f
opener_ext_jpg      = feh %f
opener_ext_jpeg     = feh %f
opener_ext_gif      = feh %f
opener_ext_svg      = feh %f
opener_ext_mp4      = mpv %f
opener_ext_mkv      = mpv %f
opener_ext_mp3      = mpv %f
opener_ext_flac     = mpv %f
opener_ext_txt      = $EDITOR %f
opener_ext_md       = $EDITOR %f
opener_ext_cpp      = $EDITOR %f
opener_ext_h        = $EDITOR %f
opener_ext_py       = $EDITOR %f
opener_ext_sh       = $EDITOR %f
opener_ext_zip      = file-roller %f
opener_ext_tar      = file-roller %f
opener_ext_gz       = file-roller %f
)";

// Returns the platform-appropriate config file path.
static fs::path config_path() {
  const char *home = std::getenv("HOME");
  if (!home) return fs::current_path() / "colorfinder.conf";

#if defined(__APPLE__)
  return fs::path(home) / "Library/Application Support/colorfinder/colorfinder.conf";
#else
  const char *xdg = std::getenv("XDG_CONFIG_HOME");
  fs::path base = xdg && *xdg ? xdg : fs::path(home) / ".config";
  return base / "colorfinder/colorfinder.conf";
#endif
}

// Parses "key = value" lines, skipping comments and blanks.
std::unordered_map<std::string, std::string> config_load() {
  std::unordered_map<std::string, std::string> cfg;
  auto path = config_path();

  if (!fs::exists(path)) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out) { std::cerr << "Warning: could not create config at " << path << "\n"; return cfg; }
    out << DEFAULT_CONFIG;
  }

  std::ifstream in(path);
  if (!in) { std::cerr << "Warning: could not read config at " << path << "\n"; return cfg; }

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::string key, eq, value;
    if (ss >> key && key[0] != '#' && ss >> eq && eq == "=" && std::getline(ss >> std::ws, value))
      cfg[key] = value;
  }

  return cfg;
}

// Looks up a key, returning fallback if absent.
std::string config_get(const std::unordered_map<std::string, std::string> &cfg,
                       const std::string &key,
                       const std::string &fallback) {
  auto it = cfg.find(key);
  return it != cfg.end() ? it->second : fallback;
}
