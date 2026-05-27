#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

// ----- File entry -----

struct FileEntry {
  std::string name;
  bool        is_dir  = false;
  uintmax_t   size    = 0;
  std::string modified;
};

// ----- Popup kind -----

enum class PopupKind {
  None,
  NewFile,
  NewFolder,
  Rename,
  DeleteConfirm,
};

struct PopupState {
  PopupKind   kind          = PopupKind::None;
  char        input[256]    = {};
  std::string target_name;
};

// ----- Focus panel -----

enum class FocusPanel {
  Sidebar,
  Main,
};

// ----- App state -----

struct AppState {
  // Filesystem
  std::filesystem::path  current_path;
  std::vector<FileEntry> entries;

  // Selection / hover
  int selected_index  = -1;
  int hovered_index   = -1;
  int sidebar_hover   = -1;
  int sidebar_select  = -1;

  // Focus
  FocusPanel focus = FocusPanel::Main;

  // Scroll offsets (rows)
  float scroll_offset         = 0.f;
  float preview_scroll_offset = 0.f;

  // Preview panel contents (populated when selected is a dir)
  std::vector<FileEntry> preview_entries;

  // Path bar
  char path_buf[1024] = {};
  bool path_editing   = false;

  // Status
  std::string status_msg;

  // Popup
  PopupState popup;

  // File opener config
  std::unordered_map<std::string, std::string> cfg;

  // Whether preview panel is visible (toggled with tab)
  bool preview_visible = true;
};

// ----- Sidebar pinned locations -----

struct SidebarItem {
  std::string           label;
  std::filesystem::path path;
  int                   icon_index; // cell index in icons.png
};

std::vector<SidebarItem> sidebar_default_items();

// ----- Lifecycle -----

AppState *app_state_new(std::unordered_map<std::string, std::string> cfg);
void      app_state_free(AppState *state);

// ----- Navigation -----

void refresh_entries(AppState *state);
void navigate_to(AppState *state, const std::filesystem::path &path);
void navigate_up(AppState *state);
void navigate_into_selected(AppState *state);

// ----- Preview -----

void refresh_preview(AppState *state);

// ----- Actions -----

void action_open_selected(AppState *state,
                          const std::unordered_map<std::string,std::string> &cfg);
void action_delete_selected(AppState *state);
void action_create_file(AppState *state, const std::string &name);
void action_create_folder(AppState *state, const std::string &name);
void action_rename(AppState *state, const std::string &new_name);

// ----- Selection -----

void select_move(AppState *state, int delta);
