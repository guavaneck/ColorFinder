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
  if (fe.is_dir) {
    const char *home = std::getenv("HOME");
    if (home) {
      std::error_code ec;
      fs::path h(home);
      fs::path canon = fs::weakly_canonical(fe.path, ec);
      if (!ec) {
        if (canon == h)                return selected ? ICON_FOLDER_HOME_SEL  : ICON_FOLDER_HOME;
        if (canon == h / "Downloads")  return selected ? ICON_FOLDER_DL_SEL    : ICON_FOLDER_DL;
        if (canon == h / "Documents")  return selected ? ICON_FOLDER_DOCS_SEL  : ICON_FOLDER_DOCS;
        if (canon == h / "Pictures")   return selected ? ICON_FOLDER_PIC_SEL   : ICON_FOLDER_PIC;
        if (canon == h / "Music")      return selected ? ICON_FOLDER_MUSIC_SEL : ICON_FOLDER_MUSIC;
        if (canon == h / "Videos")     return selected ? ICON_FOLDER_VID_SEL   : ICON_FOLDER_VID;
        if (canon == h / "Desktop")    return selected ? ICON_FOLDER_DESK_SEL  : ICON_FOLDER_DESK;
      }
    }
    return selected ? ICON_FOLDER_SEL : ICON_FOLDER;
  }

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

void search_filter(AppState *state) {
  if (!state->search_active || state->search_buf[0] == '\0') return;

  std::string needle = state->search_buf;
  for (auto &ch : needle) ch = (char)std::tolower((unsigned char)ch);

  for (int i = 0; i < (int)state->entries.size(); i++) {
    std::string name = state->entries[i].name;
    for (auto &ch : name) ch = (char)std::tolower((unsigned char)ch);
    if (name.find(needle) != std::string::npos) {
      state->selected_index = i;
      state->selected_indices = { i };
      refresh_preview(state);
      return;
    }
  }
}

// ----- selection -----

bool is_selected(const AppState *state, int i) {
  for (int s : state->selected_indices)
    if (s == i) return true;
  return false;
}

void selection_set(AppState *state, int i) {
  state->selected_indices = { i };
  state->selected_index   = i;
}

void selection_toggle(AppState *state, int i) {
  for (auto it = state->selected_indices.begin();
       it != state->selected_indices.end(); ++it) {
    if (*it == i) {
      state->selected_indices.erase(it);
      // keep selected_index pointing at last remaining, or -1
      state->selected_index = state->selected_indices.empty()
        ? -1 : state->selected_indices.back();
      return;
    }
  }
  state->selected_indices.push_back(i);
  state->selected_index = i;
}

void selection_add_range(AppState *state, int from, int to) {
  if (from > to) std::swap(from, to);
  for (int i = from; i <= to; i++)
    if (!is_selected(state, i))
      state->selected_indices.push_back(i);
  state->selected_index = to;
}

void selection_clear(AppState *state) {
  state->selected_indices.clear();
  state->selected_index = -1;
}

void selection_all(AppState *state) {
  state->selected_indices.clear();
  for (int i = 0; i < (int)state->entries.size(); i++)
    state->selected_indices.push_back(i);
  state->selected_index = state->entries.empty() ? -1 : (int)state->entries.size() - 1;
}

void select_move(AppState *state, int delta) {
  if (state->entries.empty()) return;
  int n = (int)state->entries.size();
  if (state->selected_index < 0)
    state->selected_index = delta > 0 ? 0 : n - 1;
  else
    state->selected_index = std::clamp(state->selected_index + delta, 0, n - 1);
  state->selected_indices = { state->selected_index };
  refresh_preview(state);
}

// ----- lifecycle -----

AppState *app_state_new(std::unordered_map<std::string, std::string> cfg) {
  AppState *s = new AppState();
  s->cfg      = std::move(cfg);
  s->current_path = fs::current_path();
  s->show_hidden = config_get(cfg, "show_hidden", "false") == "true";
  refresh_entries(s);
  return s;
}

void app_state_free(AppState *state) { delete state; }

// ----- navigation -----

void refresh_entries(AppState *state) {
  state->entries.clear();
  selection_clear(state);
  state->hovered_index = -1;

  std::error_code ec;
  for (const auto &e : fs::directory_iterator(state->current_path, ec)) {
    if (!state->show_hidden && e.path().filename().string()[0] == '.')
      continue;
    FileEntry fe;
    fe.name     = e.path().filename().string();
    fe.is_dir   = fs::is_directory(e, ec);
    fe.size     = fe.is_dir ? 0 : fs::file_size(e, ec);
    fe.modified = format_mtime(e.path());
    fe.path     = state->current_path / fe.name;
    state->entries.push_back(std::move(fe));
  }

  sort_entries(state->entries);

  if (state->search_active)
    search_filter(state);

  auto it = state->nav_cache.find(state->current_path.string());
  if (it != state->nav_cache.end() && it->second < (int)state->entries.size())
    selection_set(state, it->second);
  else if (!state->entries.empty())
    selection_set(state, 0);

  std::strncpy(state->path_buf, state->current_path.string().c_str(),
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
    if (state->selected_index >= 0)
      state->nav_cache[state->current_path.string()] = state->selected_index;
    state->current_path = canon;
    refresh_entries(state);
    refresh_preview(state);
  } else {
    state->status_msg = "Not a valid directory.";
    std::strncpy(state->path_buf,
                 state->current_path.string().c_str(),
                 sizeof(state->path_buf) - 1);
  }
}

void navigate_up(AppState *state) {
  fs::path parent = state->current_path.parent_path();
  if (parent == state->current_path) return;

  if (state->selected_index >= 0)
    state->nav_cache[state->current_path.string()] = state->selected_index;

  std::string came_from = state->current_path.filename().string();
  state->current_path = parent;
  refresh_entries(state);

  for (int i = 0; i < (int)state->entries.size(); i++) {
    if (state->entries[i].name == came_from) {
      selection_set(state, i);
      state->nav_cache[parent.string()] = i;
      break;
    }
  }

  refresh_preview(state);
}

void navigate_into_selected(AppState *state) {
  state->sidebar_select = -1;
  const auto &list = (state->search_active && state->search_buf[0] != '\0')
    ? state->filtered_entries : state->entries;
  if (state->selected_index < 0 ||
      state->selected_index >= (int)list.size()) return;
  const FileEntry &fe = list[state->selected_index];
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
    if (!state->show_hidden && e.path().filename().string()[0] == '.')
      continue;
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
  if (state->selected_indices.empty()) return;

  for (int i : state->selected_indices) {
    if (i < 0 || i >= (int)state->entries.size()) continue;
    const FileEntry &fe = state->entries[i];
    if (fe.is_dir) {
      if (state->selected_indices.size() == 1)
        navigate_to(state, state->current_path / fe.name);
      continue;
    }
    fs::path full   = state->current_path / fe.name;
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
}

void action_delete_selected(AppState *state) {
  if (state->selected_indices.empty()) return;

  std::vector<std::string> to_delete;
  for (int i : state->selected_indices)
    if (i >= 0 && i < (int)state->entries.size())
      to_delete.push_back(state->entries[i].name);

  for (const auto &name : to_delete)
    delete_item(state->current_path / name);

  state->status_msg = "Deleted " + std::to_string(to_delete.size()) + " item(s)";
  selection_clear(state);
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

void action_copy_selected(AppState *state) {
  if (state->selected_indices.empty()) return;
  state->clipboard.op         = ClipboardOp::Copy;
  state->clipboard.source_dir = state->current_path;
  state->clipboard.names.clear();
  for (int i : state->selected_indices)
    if (i >= 0 && i < (int)state->entries.size())
      state->clipboard.names.push_back(state->entries[i].name);
  state->status_msg = "Copied " + std::to_string(state->clipboard.names.size()) + " item(s)";
}

void action_cut_selected(AppState *state) {
  if (state->selected_indices.empty()) return;
  state->clipboard.op         = ClipboardOp::Cut;
  state->clipboard.source_dir = state->current_path;
  state->clipboard.names.clear();
  for (int i : state->selected_indices)
    if (i >= 0 && i < (int)state->entries.size())
      state->clipboard.names.push_back(state->entries[i].name);
  state->status_msg = "Cut " + std::to_string(state->clipboard.names.size()) + " item(s)";
}

void action_paste(AppState *state) {
  if (state->clipboard.op == ClipboardOp::None ||
      state->clipboard.names.empty()) return;

  std::error_code ec;
  for (const auto &name : state->clipboard.names) {
    fs::path src = state->clipboard.source_dir / name;
    fs::path dst = state->current_path / name;

    if (!fs::exists(src, ec)) {
      state->status_msg = "Paste failed: source gone: " + name;
      continue;
    }

    // pasting into the same dir: make a renamed copy
    if (fs::equivalent(src.parent_path(), state->current_path, ec)) {
      std::string stem = dst.stem().string();
      std::string ext  = dst.extension().string();
      int n = 1;
      while (fs::exists(dst, ec))
        dst = state->current_path / (stem + "_copy" + (n++ > 1 ? std::to_string(n) : "") + ext);
    }

    if (state->clipboard.op == ClipboardOp::Copy) {
      if (fs::is_directory(src, ec))
        fs::copy(src, dst,
                 fs::copy_options::recursive |
                 fs::copy_options::overwrite_existing, ec);
      else
        fs::copy_file(src, dst,
                      fs::copy_options::overwrite_existing, ec);
    } else {
      fs::rename(src, dst, ec);
    }

    if (ec) {
      state->status_msg = "Paste failed: " + ec.message();
      return;
    }
  }

  if (state->clipboard.op == ClipboardOp::Cut)
    state->clipboard = {};

  state->status_msg = "Pasted to: " + state->current_path.string();
  refresh_entries(state);
}
