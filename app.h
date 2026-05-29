#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

// ----- icon index constants (must match icons.png layout) -----
// Row 0
static constexpr int ICON_FOLDER           = 0;
static constexpr int ICON_FOLDER_SEL       = 1;
static constexpr int ICON_FOLDER_DL        = 2;
static constexpr int ICON_FOLDER_DL_SEL    = 3;
static constexpr int ICON_FOLDER_PIC       = 4;
static constexpr int ICON_FOLDER_PIC_SEL   = 5;
static constexpr int ICON_FOLDER_MUSIC     = 6;
static constexpr int ICON_FOLDER_MUSIC_SEL = 7;
static constexpr int ICON_FOLDER_DOCS      = 8;
static constexpr int ICON_FOLDER_DOCS_SEL  = 9;
// Row 1
static constexpr int ICON_FOLDER_DESK      = 10;
static constexpr int ICON_FOLDER_DESK_SEL  = 11;
static constexpr int ICON_FOLDER_VID       = 12;
static constexpr int ICON_FOLDER_VID_SEL   = 13;
static constexpr int ICON_FOLDER_HOME      = 14;
static constexpr int ICON_FOLDER_HOME_SEL  = 15;
static constexpr int ICON_FILE             = 16;
static constexpr int ICON_FILE_SEL         = 17;
static constexpr int ICON_FILE_IMAGE       = 18;
static constexpr int ICON_FILE_IMAGE_SEL   = 19;
// Row 2
static constexpr int ICON_FILE_AUDIO       = 20;
static constexpr int ICON_FILE_AUDIO_SEL   = 21;
static constexpr int ICON_FILE_VIDEO       = 22;
static constexpr int ICON_FILE_VIDEO_SEL   = 23;
static constexpr int ICON_FILE_TEXT        = 24;
static constexpr int ICON_FILE_TEXT_SEL    = 25;
static constexpr int ICON_FILE_CODE        = 26;
static constexpr int ICON_FILE_CODE_SEL    = 27;
static constexpr int ICON_FILE_ARCHIVE     = 28;
static constexpr int ICON_FILE_ARCHIVE_SEL = 29;
// Row 3

// ----- file entry -----

struct FileEntry {
  std::string name;
  std::filesystem::path path;
  bool        is_dir   = false;
  uintmax_t   size     = 0;
  std::string modified;
};

// ----- clipboard -----

enum class ClipboardOp { None, Copy, Cut };

struct ClipboardState {
  ClipboardOp              op = ClipboardOp::None;
  std::filesystem::path    source_dir;
  std::vector<std::string> names;
};

// ----- popup kind -----

enum class PopupKind {
  None,
  NewFile,
  NewFolder,
  Rename,
  DeleteConfirm,
};

struct PopupState {
  PopupKind   kind        = PopupKind::None;
  char        input[256]  = {};
  std::string target_name;
};

// ----- focus panel -----

enum class FocusPanel { Sidebar, Main };

// ----- app state -----

struct AppState {
  std::filesystem::path  current_path;
  std::vector<FileEntry> entries;

  int selected_index = -1;
  int hovered_index  = -1;
  int sidebar_hover  = -1;
  int sidebar_select = -1;

  std::vector<int> selected_indices;

  FocusPanel focus = FocusPanel::Main;

  std::vector<FileEntry> preview_entries;

  char path_buf[1024] = {};
  bool path_editing   = false;

  std::string status_msg;
  PopupState  popup;

  std::unordered_map<std::string, std::string> cfg;

  bool preview_visible = true;
  bool show_hidden     = false;

  bool search_active         = false;
  char search_buf[256]       = {};
  std::vector<FileEntry> filtered_entries;

  std::unordered_map<std::string, int> nav_cache;

  ClipboardState clipboard;
};

// ----- sidebar -----

struct SidebarItem {
  std::string           label;
  std::filesystem::path path;
  int                   icon_index;
};

std::vector<SidebarItem> sidebar_default_items();

// ----- icon selection -----

int icon_for_entry(const FileEntry &fe, bool selected);

// ----- helpers -----

void search_filter(AppState *state);

// ----- selection -----

bool is_selected(const AppState *state, int i);
void selection_set(AppState *state, int i);
void selection_toggle(AppState *state, int i);
void selection_add_range(AppState *state, int from, int to);
void selection_clear(AppState *state);
void selection_all(AppState *state);
void select_move(AppState *state, int delta);

// ----- lifecycle -----

AppState *app_state_new(std::unordered_map<std::string, std::string> cfg);
void      app_state_free(AppState *state);

// ----- navigation -----

void refresh_entries(AppState *state);
void navigate_to(AppState *state, const std::filesystem::path &path);
void navigate_up(AppState *state);
void navigate_into_selected(AppState *state);

// ----- preview -----

void refresh_preview(AppState *state);

// ----- actions -----

void action_open_selected(AppState *state,
                          const std::unordered_map<std::string, std::string> &cfg);
void action_delete_selected(AppState *state);
void action_create_file(AppState *state, const std::string &name);
void action_create_folder(AppState *state, const std::string &name);
void action_rename(AppState *state, const std::string &new_name);
void action_copy_selected(AppState *state);
void action_cut_selected(AppState *state);
void action_paste(AppState *state);
