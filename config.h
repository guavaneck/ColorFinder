#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>

// --- Config keys ---

static constexpr const char *KEY_COMPILE_COMMAND = "compile_command";

// Navigation
static constexpr const char *KEY_KB_ENTER_DIR    = "keybind_enter_dir";
static constexpr const char *KEY_KB_BACK_DIR     = "keybind_back_dir";
static constexpr const char *KEY_KB_OPEN_SEARCH  = "keybind_open_search";
static constexpr const char *KEY_SHOW_HIDDEN     = "show_hidden";

// File actions
static constexpr const char *KEY_KB_OPEN_FILE      = "keybind_open_file";
static constexpr const char *KEY_KB_DELETE         = "keybind_delete";
static constexpr const char *KEY_KB_RENAME         = "keybind_rename";
static constexpr const char *KEY_KB_CREATE_FILE    = "keybind_create_file";
static constexpr const char *KEY_KB_CREATE_FOLDER  = "keybind_create_folder";
static constexpr const char *KEY_KB_COPY           = "keybind_copy";
static constexpr const char *KEY_KB_CUT            = "keybind_cut";
static constexpr const char *KEY_KB_PASTE          = "keybind_paste";

// Selection
static constexpr const char *KEY_KB_SELECT_ALL = "keybind_select_all";

// --- Config API ---

std::unordered_map<std::string, std::string> config_load();

std::string config_get(const std::unordered_map<std::string, std::string> &cfg,
                       const std::string &key,
                       const std::string &fallback = "");
