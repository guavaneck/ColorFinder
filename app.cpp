#include "app.h"
#include "operations.h"
#include "config.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <ctime>

namespace fs = std::filesystem;

// ----- sidebar -----

std::vector<SidebarItem> sidebar_default_items() {
  const char *home = std::getenv("HOME");
  std::string h = home ? home : "/";
  return {
    { "Home",      fs::path(h),                ICON_FOLDER_HOME  },
    { "Downloads", fs::path(h) / "Downloads",  ICON_FOLDER_DL    },
    { "Documents", fs::path(h) / "Documents",  ICON_FOLDER_DOCS  },
    { "Pictures",  fs::path(h) / "Pictures",   ICON_FOLDER_PIC   },
    { "Music",     fs::path(h) / "Music",      ICON_FOLDER_MUSIC },
    { "Videos",    fs::path(h) / "Videos",     ICON_FOLDER_VID   },
    { "Desktop",   fs::path(h) / "Desktop",    ICON_FOLDER_DESK  },
  };
}

// ----- icon selection -----

int icon_for_entry(const FileEntry &fe, bool selected) {
  if (fe.is_dir) return selected ? ICON_FOLDER_SEL : ICON_FOLDER;

  std::string ext;
  auto dot = fe.name.rfind('.');
  if (dot != std::string::npos) ext = fe.name.substr(dot + 1);
  for (auto &ch : ext) ch = (char)std::tolower((unsigned char)ch);

  if (ext=="png"||ext=="jpg"||ext=="jpeg"||ext=="gif"||ext=="svg"||ext=="bmp"||ext=="webp")
    return selected ? ICON_FILE_IMAGE_SEL : ICON_FILE_IMAGE;
  if (ext=="mp3"||ext=="flac"||ext=="wav"||ext=="ogg"||ext=="aac")
    return selected ? ICON_FILE_AUDIO_SEL : ICON_FILE_AUDIO;
  if (ext=="mp4"||ext=="mkv"||ext=="avi"||ext=="mov"||ext=="webm")
    return selected ? ICON_FILE_VIDEO_SEL : ICON_FILE_VIDEO;
  if (ext=="txt"||ext=="md"||ext=="rst"||ext=="log")
    return selected ? ICON_FILE_TEXT_SEL : ICON_FILE_TEXT;
  if (ext=="cpp"||ext=="c"||ext=="h"||ext=="py"||ext=="rs"||
      ext=="go"||ext=="js"||ext=="ts"||ext=="sh"||ext=="lua")
    return selected ? ICON_FILE_CODE_SEL : ICON_FILE_CODE;
  if (ext=="zip"||ext=="tar"||ext=="gz"||ext=="xz"||ext=="bz2"||ext=="7z"||ext=="rar")
    return selected ? ICON_FILE_ARCHIVE_SEL : ICON_FILE_ARCHIVE;
  if (ext=="pdf")
    return selected ? ICON_FILE_PDF_SEL : ICON_FILE_PDF;
  return selected ? ICON_FILE_SEL : ICON_FILE;
}

// ----- helpers -----

static std::string format_mtime(const fs::path &p) {
  std::error_code ec;
  auto ftime = fs::last_write_time(p, ec);
  if (ec) return "";
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
  std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&tt));
  return buf;
}

static void sort_entries(std::vector<FileEntry> &entries) {
  std::sort(entries.begin(), entries.end(),
    [](const FileEntry &a, const FileEntry &b) {
      if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
      return a.name < b.name;
    });
}

// ----- lifecycle -----

AppState *app_state_new(std::unordered_map<std::string, std::string> cfg) {
  AppState *s = new AppState();
  s->cfg      = std::move(cfg);
  s->current_path = fs::current_path();
  refresh_entries(s);
  return s;
}

void app_state_free(AppState *state) { delete state; }

// ----- navigation -----

void refresh_entries(AppState *state) {
  state->entries.clear();
  state->selected_index = -1;
  state->hovered_index  = -1;

  std::error_code ec;
  for (const auto &e : fs::directory_iterator(state->current_path, ec)) {
    FileEntry fe;
    fe.name     = e.path().filename().string();
    fe.is_dir   = fs::is_directory(e, ec);
    fe.size     = fe.is_dir ? 0 : fs::file_size(e, ec);
    fe.modified = format_mtime(e.path());
    state->entries.push_back(std::move(fe));
  }

  sort_entries(state->entries);

  std::strncpy(state->path_buf,
               state->current_path.string().c_str(),
               sizeof(state->path_buf) - 1);

  state->status_msg = ec
    ? "Error: " + ec.message()
    : state->current_path.string();

  state->preview_entries.clear();
}

void navigate_to(AppState *state, const fs::path &path) {
  std::error_code ec;
  fs::path canon = fs::canonical(path, ec);
  if (!ec && fs::is_directory(canon, ec)) {
    state->current_path = canon;
    refresh_entries(state);
  } else {
    state->status_msg = "Not a valid directory.";
    std::strncpy(state->path_buf,
                 state->current_path.string().c_str(),
                 sizeof(state->path_buf) - 1);
  }
}

void navigate_up(AppState *state) {
  fs::path parent = state->current_path.parent_path();
  if (parent != state->current_path)
    navigate_to(state, parent);
}

void navigate_into_selected(AppState *state) {
  if (state->selected_index < 0 ||
      state->selected_index >= (int)state->entries.size()) return;
  const FileEntry &fe = state->entries[state->selected_index];
  if (fe.is_dir)
    navigate_to(state, state->current_path / fe.name);
}

// ----- preview -----

void refresh_preview(AppState *state) {
  state->preview_entries.clear();

  if (state->selected_index < 0 ||
      state->selected_index >= (int)state->entries.size()) return;

  const FileEntry &fe = state->entries[state->selected_index];
  if (!fe.is_dir) return;

  fs::path dir = state->current_path / fe.name;
  std::error_code ec;
  for (const auto &e : fs::directory_iterator(dir, ec)) {
    FileEntry pfe;
    pfe.name   = e.path().filename().string();
    pfe.is_dir = fs::is_directory(e, ec);
    pfe.size   = pfe.is_dir ? 0 : fs::file_size(e, ec);
    state->preview_entries.push_back(std::move(pfe));
  }
  sort_entries(state->preview_entries);
}

// ----- actions -----

void action_open_selected(AppState *state,
                          const std::unordered_map<std::string, std::string> &cfg) {
  if (state->selected_index < 0 ||
      state->selected_index >= (int)state->entries.size()) return;

  const FileEntry &fe = state->entries[state->selected_index];
  if (fe.is_dir) {
    navigate_to(state, state->current_path / fe.name);
    return;
  }

  fs::path full = state->current_path / fe.name;
  std::string ext = full.extension().string();
  if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);

  std::string key     = "opener_ext_" + ext;
  std::string command = config_get(cfg, key,
                          config_get(cfg, "opener_fallback", "xdg-open %f"));
  std::string quoted  = "\"" + full.string() + "\"";
  size_t pos;
  while ((pos = command.find("%f")) != std::string::npos)
    command.replace(pos, 2, quoted);
  command += " &";
  std::system(command.c_str());
}

void action_delete_selected(AppState *state) {
  if (state->selected_index < 0 ||
      state->selected_index >= (int)state->entries.size()) return;
  const std::string &name = state->entries[state->selected_index].name;
  delete_item(state->current_path / name);
  state->status_msg = "Deleted: " + name;
  refresh_entries(state);
}

void action_create_file(AppState *state, const std::string &name) {
  if (name.empty()) return;
  create_file(state->current_path / name);
  state->status_msg = "Created: " + name;
  refresh_entries(state);
}

void action_create_folder(AppState *state, const std::string &name) {
  if (name.empty()) return;
  make_directory(state->current_path / name);
  state->status_msg = "Created folder: " + name;
  refresh_entries(state);
}

void action_rename(AppState *state, const std::string &new_name) {
  if (new_name.empty() || state->popup.target_name.empty()) return;
  std::error_code ec;
  fs::rename(state->current_path / state->popup.target_name,
             state->current_path / new_name, ec);
  state->status_msg = ec
    ? "Rename failed: " + ec.message()
    : "Renamed to: " + new_name;
  refresh_entries(state);
}

// ----- selection -----

void select_move(AppState *state, int delta) {
  if (state->entries.empty()) return;
  int n = (int)state->entries.size();
  if (state->selected_index < 0)
    state->selected_index = delta > 0 ? 0 : n - 1;
  else
    state->selected_index = std::clamp(state->selected_index + delta, 0, n - 1);
  refresh_preview(state);
}
