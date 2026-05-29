// ui.cpp
// Rendering via Dear ImGui + sokol_imgui.h.
// Architecture: Input -> Cmd -> apply_cmd() -> State -> Render
// UI never mutates AppState directly.

#include "ui.h"
#include "app.h"
#include "uiconfig.h"

#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#include <sokol_imgui.h>
#include <imgui.h>
#include <stb_image.h>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================
// Command system
// ============================================================

enum class CmdType {
  None,
  MoveUp,
  MoveDown,
  Open,
  Back,
  SelectIndex,
  TogglePreview,
  Delete,
  Rename,
  NewFile,
  NewFolder,
  OpenFile,
  GoHome,
  OpenSearch,
  CloseSearch,
  Copy,
  Cut,
  Paste,
  SelectAll,
  SelectToggle,
  SelectRange,
};

struct Cmd {
  CmdType type  = CmdType::None;
  int     index = -1;
  int     steps = 1;
};

// ============================================================
// Color helpers
// ============================================================

static ImVec4 to_imvec4(const Color &c) {
  return ImVec4(c.r/255.f, c.g/255.f, c.b/255.f, c.a/255.f);
}

static ImU32 to_imu32(const Color &c) {
  return IM_COL32(c.r, c.g, c.b, c.a);
}

// ============================================================
// Text truncation
// ============================================================

static std::string truncate(const std::string &s, float max_px) {
  if (ImGui::CalcTextSize(s.c_str()).x <= max_px) return s;
  std::string t = s;
  while (t.size() > 1 && ImGui::CalcTextSize((t + "...").c_str()).x > max_px)
    t.pop_back();
  return t + "...";
}

// ============================================================
// Icon sprite sheet
// ============================================================

static sg_image    s_icons_tex  = {};
static sg_view     s_icons_view = {};
static sg_sampler  s_icons_smp  = {};
static ImTextureID s_icons_id   = 0;
static int         s_icons_cols = 1;
static int         s_icons_cell = 16;
static int         s_icons_w    = 16;
static int         s_icons_h    = 16;

static void load_icons(const UIConfig &cfg) {
  int w = 0, h = 0, n = 0;
  unsigned char *data = stbi_load(cfg.icons_sheet.c_str(), &w, &h, &n, 4);
  if (!data) {
    fprintf(stderr, "icons: failed to load '%s'\n", cfg.icons_sheet.c_str());
    return;
  }
  fprintf(stderr, "icons: loaded %dx%d from '%s'\n", w, h, cfg.icons_sheet.c_str());

  sg_image_desc d = {};
  d.width  = w;
  d.height = h;
  d.data.mip_levels[0] = { data, (size_t)(w * h * 4) };
  s_icons_tex = sg_make_image(&d);
  stbi_image_free(data);

  sg_view_desc vd = {};
  vd.texture.image = s_icons_tex;
  s_icons_view = sg_make_view(&vd);

  sg_sampler_desc sd = {};
  sd.min_filter = SG_FILTER_NEAREST;
  sd.mag_filter = SG_FILTER_NEAREST;
  s_icons_smp = sg_make_sampler(&sd);

  s_icons_id   = simgui_imtextureid_with_sampler(s_icons_view, s_icons_smp);
  s_icons_cell = cfg.icon_size;
  s_icons_w    = w;
  s_icons_h    = h;
  s_icons_cols = (cfg.icon_size > 0) ? w / cfg.icon_size : 1;
}

static void draw_icon_cell(int cell_index, float size) {
  if (!s_icons_tex.id || s_icons_cols == 0) {
    ImGui::Dummy({size, size});
    return;
  }
  int col = cell_index % s_icons_cols;
  int row = cell_index / s_icons_cols;
  float iw = (float)s_icons_w, ih = (float)s_icons_h;
  float cs = (float)s_icons_cell;
  ImVec2 uv0 = { col * cs / iw, row * cs / ih };
  ImVec2 uv1 = { uv0.x + cs / iw, uv0.y + cs / ih };
  ImGui::Image(s_icons_id, {size, size}, uv0, uv1);
}

// ============================================================
// Keybind parsing
// ============================================================

struct KeySpec {
  bool     ctrl = false, shift = false, alt = false;
  ImGuiKey key  = ImGuiKey_None;
};

static ImGuiKey keyname_to_imkey(const std::string &n) {
  if (n == "up")        return ImGuiKey_UpArrow;
  if (n == "down")      return ImGuiKey_DownArrow;
  if (n == "left")      return ImGuiKey_LeftArrow;
  if (n == "right")     return ImGuiKey_RightArrow;
  if (n == "return")    return ImGuiKey_Enter;
  if (n == "backspace") return ImGuiKey_Backspace;
  if (n == "delete")    return ImGuiKey_Delete;
  if (n == "escape")    return ImGuiKey_Escape;
  if (n == "tab")       return ImGuiKey_Tab;
  if (n == "space")     return ImGuiKey_Space;
  if (n == "f1")        return ImGuiKey_F1;
  if (n == "f2")        return ImGuiKey_F2;
  if (n == "f5")        return ImGuiKey_F5;
  if (n == "~")         return ImGuiKey_GraveAccent;
  if (n.size() == 1) {
    char c = n[0];
    if (c >= 'a' && c <= 'z') return (ImGuiKey)(ImGuiKey_A + (c - 'a'));
    if (c >= '0' && c <= '9') return (ImGuiKey)(ImGuiKey_0 + (c - '0'));
  }
  return ImGuiKey_None;
}

static KeySpec parse_keybind(const std::string &s) {
  KeySpec ks;
  std::string cur;
  std::vector<std::string> parts;
  for (char c : s) {
    if (c == '+') { parts.push_back(cur); cur.clear(); }
    else cur += c;
  }
  parts.push_back(cur);
  for (auto &p : parts) {
    if (p == "ctrl")       ks.ctrl  = true;
    else if (p == "shift") ks.shift = true;
    else if (p == "alt")   ks.alt   = true;
    else                   ks.key   = keyname_to_imkey(p);
  }
  return ks;
}

struct ResolvedKeys {
  KeySpec up, down, left, right;
  KeySpec up_alt, down_alt, left_alt, right_alt;
  KeySpec enter, back;
  KeySpec del, rename;
  KeySpec new_file, new_folder;
  KeySpec toggle_preview;
  KeySpec open_file;
  KeySpec go_home;
  KeySpec open_search;
};

static ResolvedKeys g_keys;

static void resolve_keys(const UIConfig &cfg) {
  g_keys.up             = parse_keybind(cfg.kb_up);
  g_keys.down           = parse_keybind(cfg.kb_down);
  g_keys.left           = parse_keybind(cfg.kb_left);
  g_keys.right          = parse_keybind(cfg.kb_right);
  g_keys.up_alt         = parse_keybind(cfg.kb_up_alt);
  g_keys.down_alt       = parse_keybind(cfg.kb_down_alt);
  g_keys.left_alt       = parse_keybind(cfg.kb_left_alt);
  g_keys.right_alt      = parse_keybind(cfg.kb_right_alt);
  g_keys.enter          = parse_keybind(cfg.kb_enter);
  g_keys.back           = parse_keybind(cfg.kb_back);
  g_keys.del            = parse_keybind(cfg.kb_delete);
  g_keys.rename         = parse_keybind(cfg.kb_rename);
  g_keys.new_file       = parse_keybind(cfg.kb_new_file);
  g_keys.new_folder     = parse_keybind(cfg.kb_new_folder);
  g_keys.toggle_preview = parse_keybind(cfg.kb_toggle_preview);
  g_keys.open_file = parse_keybind(cfg.kb_open_file);
  g_keys.go_home = parse_keybind(cfg.kb_go_home);
  g_keys.open_search = parse_keybind(cfg.kb_open_search);
}

// Check a resolved key, with optional repeat.
static bool key(const KeySpec &k, bool repeat = false) {
  if (k.key == ImGuiKey_None) return false;
  ImGuiIO &io = ImGui::GetIO();
  if (!ImGui::IsKeyPressed(k.key, repeat)) return false;
  if (k.ctrl  && !io.KeyCtrl)  return false;
  if (k.shift && !io.KeyShift) return false;
  if (k.alt   && !io.KeyAlt)   return false;
  return true;
}

// ============================================================
// Input -> Cmd translator
// ============================================================

static Cmd collect_input(const AppState *state) {
  ImGuiIO &io = ImGui::GetIO();

  // Block navigation while text input is active.
  if (io.WantTextInput || state->path_editing || state->popup.kind != PopupKind::None)
    return {};

  // ----- scroll = move selection -----
  if (io.MouseWheel != 0.f) {
    // positive = scroll up = move selection up
    int steps = (int)std::round(std::abs(io.MouseWheel));
    if (steps < 1) steps = 1;
    return { io.MouseWheel > 0 ? CmdType::MoveUp : CmdType::MoveDown, steps };
  }

  // ----- navigation (repeating) -----
  if (key(g_keys.up,   true) || key(g_keys.up_alt,   true)) return {CmdType::MoveUp};
  if (key(g_keys.down, true) || key(g_keys.down_alt, true)) return {CmdType::MoveDown};

  // ----- navigation (single press) -----
  if (key(g_keys.left,  false) || key(g_keys.left_alt,  false) || key(g_keys.back,  false)) return {CmdType::Back};
  if (key(g_keys.right, false) || key(g_keys.right_alt, false) || key(g_keys.enter, false)) return {CmdType::Open};

  // ----- actions -----
  if (key(g_keys.toggle_preview, false)) return {CmdType::TogglePreview};

  if (key(g_keys.del,    false) && state->selected_index >= 0) return {CmdType::Delete};
  if (key(g_keys.rename, false) && state->selected_index >= 0) return {CmdType::Rename};
  if (key(g_keys.new_file,   false)) return {CmdType::NewFile};
  if (key(g_keys.new_folder, false)) return {CmdType::NewFolder};

  if (key(g_keys.open_file, false) && state->selected_index >= 0) return {CmdType::OpenFile};

  if (key(g_keys.go_home, false)) return {CmdType::GoHome};

  if (key(g_keys.open_search, false)) return {CmdType::OpenSearch};

  return {};
}

// ============================================================
// State machine - the only place AppState changes
// ============================================================

static void apply_cmd(AppState *state, const Cmd &cmd) {
  switch (cmd.type) {
    case CmdType::MoveUp:
      select_move(state, -cmd.steps);
      break;

    case CmdType::MoveDown:
      select_move(state, +cmd.steps);
      break;

    case CmdType::Open:
      navigate_into_selected(state);
      break;

    case CmdType::Back:
      navigate_up(state);
      break;

    case CmdType::SelectIndex:
      if (cmd.index >= 0 && cmd.index < (int)state->entries.size()) {
        state->selected_index = cmd.index;
        refresh_preview(state);
      }
      break;

    case CmdType::TogglePreview:
      state->preview_visible = !state->preview_visible;
      break;

    case CmdType::Delete:
      state->popup.kind        = PopupKind::DeleteConfirm;
      state->popup.target_name = state->entries[state->selected_index].name;
      break;

    case CmdType::Rename:
      state->popup.kind = PopupKind::Rename;
      std::strncpy(state->popup.input,
                   state->entries[state->selected_index].name.c_str(),
                   sizeof(state->popup.input) - 1);
      state->popup.target_name = state->entries[state->selected_index].name;
      break;

    case CmdType::NewFile:
      state->popup.kind = PopupKind::NewFile;
      memset(state->popup.input, 0, sizeof(state->popup.input));
      break;

    case CmdType::NewFolder:
      state->popup.kind = PopupKind::NewFolder;
      memset(state->popup.input, 0, sizeof(state->popup.input));
      break;

    case CmdType::OpenFile:
      action_open_selected(state, state->cfg);
      break;

    case CmdType::GoHome: {
      const char *home = std::getenv("HOME");
      if (home) navigate_to(state, fs::path(home));
      break;
    }

    case CmdType::OpenSearch:
      state->search_active = true;
      memset(state->search_buf, 0, sizeof(state->search_buf));
      state->filtered_entries.clear();
      state->selected_index = -1;
      break;

    case CmdType::CloseSearch:
      state->search_active = false;
      memset(state->search_buf, 0, sizeof(state->search_buf));
      state->filtered_entries.clear();
      state->selected_index = -1;
      break;

    default:
      break;
  }
}

// ============================================================
// Scroll system - centered on selected item, lerp smoothed
// ============================================================

static void update_scroll(AppState *state, float content_h, float row_h) {
  if (state->selected_index < 0) return;

  float target  = state->selected_index * row_h - content_h * 0.5f;
  float current = ImGui::GetScrollY();

  float speed = 14.0f;
  float dt    = ImGui::GetIO().DeltaTime;
  float t     = 1.0f - expf(-speed * dt);

  ImGui::SetScrollY(current + (target - current) * t);
}

// ============================================================
// Sub-panels
// ============================================================

static void draw_pathbar(AppState *state, const UIConfig &cfg,
                         float bar_w, float bar_h) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, to_imvec4(cfg.color_panel_bg));
  ImGui::BeginChild("##pathbar", {bar_w, bar_h}, false,
                    ImGuiWindowFlags_NoScrollbar);

  ImGui::SetCursorPos({cfg.row_padding_x, (bar_h - ImGui::GetFontSize()) * 0.5f});

  if (state->path_editing) {
    static bool focus_next = false;
    if (focus_next) { ImGui::SetKeyboardFocusHere(); focus_next = false; }

    ImGui::SetNextItemWidth(bar_w - cfg.row_padding_x * 2);
    if (ImGui::InputText("##path", state->path_buf, sizeof(state->path_buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      navigate_to(state, fs::path(state->path_buf));
      state->path_editing = false;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      std::strncpy(state->path_buf,
                   state->current_path.string().c_str(),
                   sizeof(state->path_buf) - 1);
      state->path_editing = false;
    }
    if (ImGui::IsWindowAppearing()) focus_next = true;
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text_dim));
    std::string display = truncate(state->current_path.string(),
                                   bar_w - cfg.row_padding_x * 2);
    if (ImGui::Selectable(display.c_str(), false,
                          ImGuiSelectableFlags_None, {bar_w, bar_h})) {
      state->path_editing = true;
      std::strncpy(state->path_buf,
                   state->current_path.string().c_str(),
                   sizeof(state->path_buf) - 1);
    }
    ImGui::PopStyleColor();
  }

  ImDrawList *dl = ImGui::GetWindowDrawList();
  ImVec2 wp = ImGui::GetWindowPos();
  dl->AddLine({wp.x, wp.y + bar_h - 1},
              {wp.x + bar_w, wp.y + bar_h - 1},
              to_imu32(cfg.color_border));

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

static void draw_sidebar(AppState *state, const UIConfig &cfg,
                         const std::vector<SidebarItem> &items,
                         float sb_w, float content_h) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, to_imvec4(cfg.color_sidebar_bg));
  ImGui::BeginChild("##sidebar", {sb_w, content_h}, false,
                    ImGuiWindowFlags_NoScrollbar);

  float icon_sz = (float)cfg.icon_size;
  float row_h   = cfg.row_height;

  for (int i = 0; i < (int)items.size(); i++) {
    bool sel = (state->sidebar_select == i);

    ImGui::SetCursorPosX(0);
    ImGui::Selectable(("##sb" + std::to_string(i)).c_str(), sel,
                      ImGuiSelectableFlags_None, {sb_w, row_h});

    if (ImGui::IsItemClicked()) {
      state->sidebar_select = i;
      navigate_to(state, items[i].path);
    }

    // clear highlight if we've navigated away from this sidebar item's path
    if (sel && state->current_path != items[i].path)
      state->sidebar_select = -1;

    ImVec2 rmin = ImGui::GetItemRectMin();
    ImVec2 rmax = { rmin.x + sb_w, rmin.y + row_h };

    if (sel)
      ImGui::GetWindowDrawList()->AddRectFilled(rmin, rmax,
        to_imu32(cfg.color_selection_bg));

    ImVec2 win = ImGui::GetWindowPos();
    ImGui::SetCursorPos({cfg.row_padding_x,
                         rmin.y - win.y + (row_h - icon_sz) * 0.5f});
    draw_icon_cell(items[i].icon_index, icon_sz);
    ImGui::SameLine(0, cfg.icon_label_gap);
    float avail = sb_w - cfg.row_padding_x - icon_sz - cfg.icon_label_gap - cfg.row_padding_x;
    ImGui::SetCursorPosY(rmin.y - win.y + (row_h - ImGui::GetFontSize()) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text,
      sel ? to_imvec4(cfg.color_text_selected) : to_imvec4(cfg.color_text));
    ImGui::TextUnformatted(truncate(items[i].label, avail).c_str());
    ImGui::PopStyleColor();
  }

  ImDrawList *dl = ImGui::GetWindowDrawList();
  ImVec2 wp = ImGui::GetWindowPos();
  dl->AddLine({wp.x + sb_w - 1, wp.y},
              {wp.x + sb_w - 1, wp.y + content_h},
              to_imu32(cfg.color_border));

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

static void draw_file_list(AppState *state, const UIConfig &cfg,
                           float list_w, float content_h) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, to_imvec4(cfg.color_bg));
  ImGui::BeginChild("##filelist", {list_w, content_h}, false,
                    ImGuiWindowFlags_NoScrollWithMouse);

  const auto &visible = (state->search_active && state->search_buf[0] != '\0')
    ? state->filtered_entries
    : state->entries;

  float icon_sz = (float)cfg.icon_size;
  float row_h   = cfg.row_height;
  int   n       = (int)visible.size();

  ImGuiListClipper clipper;
  clipper.Begin(n, row_h);
  while (clipper.Step()) {
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
      if (i < 0 || i >= n) continue;

      const FileEntry &fe = visible[i];
      bool sel = (state->selected_index == i);

      ImGui::PushID(i);

      ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0,0,0,0));
      ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0,0,0,0));

      ImGui::Selectable("##row", sel,
        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
        {list_w, row_h});

      bool hovered = ImGui::IsItemHovered();
      ImGui::PopStyleColor(3);

      if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        apply_cmd(state, {CmdType::SelectIndex, i});
      if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        apply_cmd(state, {CmdType::Open});

      // ----- row background -----
      ImVec2 rmin = ImGui::GetItemRectMin();
      ImVec2 rmax = { rmin.x + list_w, rmin.y + row_h };
      if (sel)
        ImGui::GetWindowDrawList()->AddRectFilled(rmin, rmax,
          to_imu32(cfg.color_selection_bg));
      else if (hovered)
        ImGui::GetWindowDrawList()->AddRectFilled(rmin, rmax,
          to_imu32(cfg.color_hover_bg));

      // ----- icon -----
      ImVec2 win = ImGui::GetWindowPos();
      ImGui::SetCursorPos({
        cfg.row_padding_x,
        rmin.y - win.y + ImGui::GetScrollY() + (row_h - icon_sz) * 0.5f
      });
      draw_icon_cell(icon_for_entry(fe, sel), icon_sz);

      // ----- label -----
      ImGui::SameLine(0, cfg.icon_label_gap);
      float avail = list_w
        - cfg.row_padding_x
        - icon_sz
        - cfg.icon_label_gap
        - 6.f
        - cfg.row_padding_x;
      ImGui::SetCursorPosY(
        rmin.y - win.y + ImGui::GetScrollY() + (row_h - ImGui::GetFontSize()) * 0.5f);
      ImGui::PushStyleColor(ImGuiCol_Text,
        sel ? to_imvec4(cfg.color_text_selected) : to_imvec4(cfg.color_text));
      ImGui::TextUnformatted(truncate(fe.name, avail).c_str());
      ImGui::PopStyleColor();

      ImGui::PopID();
    }
  }
  clipper.End();

  update_scroll(state, content_h, row_h);
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

static void draw_preview(AppState *state, const UIConfig &cfg,
                         float pv_w, float content_h) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, to_imvec4(cfg.color_panel_bg));

  if (!ImGui::BeginChild("##preview", {pv_w, content_h}, false)) {
    ImGui::EndChild();
    ImGui::PopStyleColor();
    return;
  }

  ImDrawList *dl = ImGui::GetWindowDrawList();
  ImVec2 wp = ImGui::GetWindowPos();
  dl->AddLine({wp.x, wp.y}, {wp.x, wp.y + content_h}, to_imu32(cfg.color_border));

  bool valid = state->selected_index >= 0 &&
               state->selected_index < (int)state->entries.size();
  if (!valid) {
    ImGui::EndChild();
    ImGui::PopStyleColor();
    return;
  }

  const FileEntry &fe = state->entries[state->selected_index];
  float px    = cfg.row_padding_x + 1.f;
  float row_h = cfg.row_height;

  // ----- name -----
  ImGui::SetCursorPos({px, (row_h - ImGui::GetFontSize()) * 0.5f});
  ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text));
  ImGui::TextUnformatted(truncate(fe.name, pv_w - px * 2).c_str());
  ImGui::PopStyleColor();

  dl->AddLine({wp.x + px,        wp.y + row_h},
              {wp.x + pv_w - px, wp.y + row_h},
              to_imu32(cfg.color_border));

  ImGui::SetCursorPosY(row_h + 2.f);

  if (fe.is_dir) {
    float icon_sz = (float)cfg.icon_size;
    ImGui::BeginChild("##pv_list", {pv_w, content_h - row_h - 2.f}, false);
    for (int i = 0; i < (int)state->preview_entries.size(); i++) {
      const FileEntry &pfe = state->preview_entries[i];
      ImGui::PushID(i + 10000);
      ImGui::BeginGroup();
      ImGui::SetCursorPosX(px);
      draw_icon_cell(icon_for_entry(pfe, false), icon_sz);
      ImGui::SameLine(0, cfg.icon_label_gap);
      float avail = pv_w - px - icon_sz - cfg.icon_label_gap - px;
      ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text_dim));
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(truncate(pfe.name, avail).c_str());
      ImGui::PopStyleColor();
      ImGui::EndGroup();
      ImGui::Dummy(ImVec2(0.f, row_h - ImGui::GetItemRectSize().y));
      ImGui::PopID();
    }
    ImGui::EndChild();
  } else {
    std::string size_str;
    if (fe.size < 1024)
      size_str = std::to_string(fe.size) + " B";
    else if (fe.size < 1024 * 1024)
      size_str = std::to_string(fe.size / 1024) + " KB";
    else
      size_str = std::to_string(fe.size / (1024 * 1024)) + " MB";

    ImGui::SetCursorPos({px, row_h + row_h * 0.5f});
    ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text_dim));
    ImGui::Text("Size:");
    ImGui::PopStyleColor();
    ImGui::SameLine(px + 50.f);
    ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text));
    ImGui::TextUnformatted(size_str.c_str());
    ImGui::PopStyleColor();

    ImGui::SetCursorPos({px, row_h + row_h * 1.5f});
    ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text_dim));
    ImGui::Text("Modified:");
    ImGui::PopStyleColor();
    ImGui::SameLine(px + 70.f);
    ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text));
    ImGui::TextUnformatted(truncate(fe.modified, pv_w - px - 70.f).c_str());
    ImGui::PopStyleColor();
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

static void draw_status(AppState *state, const UIConfig &cfg,
                        float fw, float sh) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, to_imvec4(cfg.color_status_bg));
  ImGui::BeginChild("##status", {fw, sh}, false, ImGuiWindowFlags_NoScrollbar);

  ImDrawList *dl = ImGui::GetWindowDrawList();
  ImVec2 wp = ImGui::GetWindowPos();
  dl->AddLine({wp.x, wp.y}, {wp.x + fw, wp.y}, to_imu32(cfg.color_border));

  ImGui::SetCursorPos({cfg.row_padding_x, (sh - ImGui::GetFontSize()) * 0.5f});
  ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_status_text));
  ImGui::TextUnformatted(
    truncate(state->status_msg, fw - cfg.row_padding_x * 2).c_str());
  ImGui::PopStyleColor();

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

static void draw_popup(AppState *state, const UIConfig &cfg) {
  if (state->popup.kind == PopupKind::None) return;

  const char *popup_id = "##popup";
  ImGui::OpenPopup(popup_id);

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
  ImGui::SetNextWindowSize({320, 100}, ImGuiCond_Always);

  ImGui::PushStyleColor(ImGuiCol_PopupBg, to_imvec4(cfg.color_panel_bg));
  ImGui::PushStyleColor(ImGuiCol_Border,  to_imvec4(cfg.color_border));

  if (ImGui::BeginPopupModal(popup_id, nullptr,
                             ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize)) {
    const char *title = "";
    switch (state->popup.kind) {
      case PopupKind::NewFile:       title = "New File";   break;
      case PopupKind::NewFolder:     title = "New Folder"; break;
      case PopupKind::Rename:        title = "Rename";     break;
      case PopupKind::DeleteConfirm: title = "Delete";     break;
      default: break;
    }

    ImGui::SetCursorPos({12, 10});
    ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    bool confirmed = false;

    if (state->popup.kind == PopupKind::DeleteConfirm) {
      ImGui::SetCursorPos({12, 40});
      ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text_dim));
      std::string msg = "Delete \"" + state->popup.target_name + "\"?  [Return/Esc]";
      ImGui::TextUnformatted(truncate(msg, 296).c_str());
      ImGui::PopStyleColor();
      confirmed = ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    } else {
      static bool focus_input = false;
      if (ImGui::IsWindowAppearing()) focus_input = true;
      if (focus_input) { ImGui::SetKeyboardFocusHere(); focus_input = false; }

      ImGui::SetCursorPos({12, 30});
      ImGui::SetNextItemWidth(296);
      if (ImGui::InputText("##popinput", state->popup.input,
                           sizeof(state->popup.input),
                           ImGuiInputTextFlags_EnterReturnsTrue))
        confirmed = true;

      ImGui::SetCursorPos({12, 72});
      ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text_dim));
      ImGui::TextUnformatted("[Return] confirm  [Esc] cancel");
      ImGui::PopStyleColor();
    }

    if (confirmed) {
      switch (state->popup.kind) {
        case PopupKind::NewFile:       action_create_file(state,   state->popup.input); break;
        case PopupKind::NewFolder:     action_create_folder(state, state->popup.input); break;
        case PopupKind::Rename:        action_rename(state,        state->popup.input); break;
        case PopupKind::DeleteConfirm: action_delete_selected(state);                   break;
        default: break;
      }
      state->popup = {};
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      state->popup = {};
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  ImGui::PopStyleColor(2);
}

static void draw_search_bar(AppState *state, const UIConfig &cfg,
                            float fw, float sh) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, to_imvec4(cfg.color_panel_bg));
  ImGui::BeginChild("##searchbar", {fw, sh}, false, ImGuiWindowFlags_NoScrollbar);

  ImDrawList *dl = ImGui::GetWindowDrawList();
  ImVec2 wp = ImGui::GetWindowPos();
  dl->AddLine({wp.x, wp.y}, {wp.x + fw, wp.y}, to_imu32(cfg.color_border));

  // label
  ImGui::SetCursorPos({cfg.row_padding_x, (sh - ImGui::GetFontSize()) * 0.5f});
  ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text_dim));
  ImGui::TextUnformatted("search: ");
  ImGui::PopStyleColor();

  float label_w = ImGui::CalcTextSize("search: ").x + cfg.row_padding_x;

  static bool focus_search = false;
  if (ImGui::IsWindowAppearing()) focus_search = true;
  if (focus_search) { ImGui::SetKeyboardFocusHere(); focus_search = false; }

  ImGui::SameLine(label_w);
  ImGui::SetNextItemWidth(fw - label_w - cfg.row_padding_x);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, to_imvec4(cfg.color_panel_bg));
  ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_text));

  bool changed = ImGui::InputText("##search", state->search_buf,
                                  sizeof(state->search_buf));
  ImGui::PopStyleColor(2);

  if (changed) {
    state->selected_index = -1;
    search_filter(state);
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    state->search_active = false;
    memset(state->search_buf, 0, sizeof(state->search_buf));
    state->filtered_entries.clear();
    state->selected_index = -1;
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

// ============================================================
// ui_init / ui_shutdown
// ============================================================

void ui_init(const UIConfig &cfg) {
  simgui_desc_t d = {};
  simgui_setup(&d);

  // ----- style -----
  ImGuiStyle &st = ImGui::GetStyle();
  st.WindowPadding     = {0, 0};
  st.ItemSpacing       = {0, 0};
  st.FramePadding      = {cfg.row_padding_x, cfg.row_padding_y};
  st.ScrollbarSize     = 6.f;
  st.WindowBorderSize  = 0.f;
  st.ChildBorderSize   = 0.f;
  st.PopupBorderSize   = 1.f;
  st.WindowRounding    = 0.f;
  st.ChildRounding     = 0.f;
  st.FrameRounding     = 0.f;
  st.PopupRounding     = 0.f;
  st.ScrollbarRounding = 0.f;

  ImVec4 *c = st.Colors;
  c[ImGuiCol_WindowBg]             = to_imvec4(cfg.color_bg);
  c[ImGuiCol_ChildBg]              = to_imvec4(cfg.color_bg);
  c[ImGuiCol_PopupBg]              = to_imvec4(cfg.color_panel_bg);
  c[ImGuiCol_Border]               = to_imvec4(cfg.color_border);
  c[ImGuiCol_FrameBg]              = to_imvec4(cfg.color_bg);
  c[ImGuiCol_FrameBgActive]        = to_imvec4(cfg.color_selection_bg);
  c[ImGuiCol_TitleBg]              = to_imvec4(cfg.color_panel_bg);
  c[ImGuiCol_TitleBgActive]        = to_imvec4(cfg.color_panel_bg);
  c[ImGuiCol_ScrollbarBg]          = to_imvec4(cfg.color_scrollbar);
  c[ImGuiCol_ScrollbarGrab]        = to_imvec4(cfg.color_scrollbar_fg);
  c[ImGuiCol_ScrollbarGrabHovered] = to_imvec4(cfg.color_scrollbar_fg);
  c[ImGuiCol_ScrollbarGrabActive]  = to_imvec4(cfg.color_text_dim);
  c[ImGuiCol_Header]               = {0, 0, 0, 0};
  c[ImGuiCol_HeaderHovered]        = {0, 0, 0, 0};
  c[ImGuiCol_HeaderActive]         = {0, 0, 0, 0};
  c[ImGuiCol_Button]               = to_imvec4(cfg.color_bg);
  c[ImGuiCol_ButtonActive]         = to_imvec4(cfg.color_selection_bg);
  c[ImGuiCol_Text]                 = to_imvec4(cfg.color_text);
  c[ImGuiCol_TextDisabled]         = to_imvec4(cfg.color_text_dim);
  c[ImGuiCol_Separator]            = to_imvec4(cfg.color_border);
  c[ImGuiCol_SeparatorHovered]     = to_imvec4(cfg.color_border);
  c[ImGuiCol_SeparatorActive]      = to_imvec4(cfg.color_border);

  // ----- font -----
  if (!cfg.font_path.empty()) {
    ImFontConfig fc;
    fc.OversampleH = 2;
    fc.OversampleV = 2;
    ImGui::GetIO().Fonts->AddFontFromFileTTF(
      cfg.font_path.c_str(), cfg.font_size, &fc);
  }

  load_icons(cfg);
  resolve_keys(cfg);
}

void ui_shutdown() {
  if (s_icons_tex.id) sg_destroy_image(s_icons_tex);
  if (s_icons_smp.id) sg_destroy_sampler(s_icons_smp);
  simgui_shutdown();
}

// ============================================================
// ui_draw
// ============================================================

void ui_draw(AppState *state,
             const UIConfig &cfg,
             const std::vector<SidebarItem> &sidebar,
             float fb_width, float fb_height) {
  simgui_new_frame({ (int)fb_width, (int)fb_height, 1.0/60.0, 1.0f });

  // ----- collect input, apply commands -----
  Cmd cmd = collect_input(state);
  if (cmd.type != CmdType::None)
    apply_cmd(state, cmd);

  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize({fb_width, fb_height});
  ImGui::Begin("##root", nullptr,
               ImGuiWindowFlags_NoTitleBar      |
               ImGuiWindowFlags_NoResize        |
               ImGuiWindowFlags_NoMove          |
               ImGuiWindowFlags_NoScrollbar     |
               ImGuiWindowFlags_NoSavedSettings |
               ImGuiWindowFlags_NoBringToFrontOnFocus);

  if (fb_width < cfg.min_width_warning) {
    ImGui::SetCursorPos({
      (fb_width  - ImGui::CalcTextSize("window too small").x) * 0.5f,
      (fb_height - ImGui::GetFontSize()) * 0.5f});
    ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(cfg.color_warning_text));
    ImGui::TextUnformatted("window too small");
    ImGui::PopStyleColor();
    ImGui::End();
  } else {
    float pathbar_h = cfg.row_height + 4.f;
    float status_h  = cfg.status_height;
    float content_h = fb_height - pathbar_h - status_h;
    bool  show_pv   = state->preview_visible && fb_width >= cfg.min_width_preview;
    float preview_w = show_pv ? cfg.preview_width : 0.f;
    float list_w    = fb_width - cfg.sidebar_width - preview_w;

    ImGui::SetCursorPos({0, 0});
    draw_pathbar(state, cfg, fb_width, pathbar_h);

    ImGui::SetCursorPos({0, pathbar_h});
    draw_sidebar(state, cfg, sidebar, cfg.sidebar_width, content_h);
    ImGui::SameLine(0, 0);
    draw_file_list(state, cfg, list_w, content_h);
    if (show_pv) { ImGui::SameLine(0, 0); draw_preview(state, cfg, preview_w, content_h); }

    ImGui::SetCursorPos({0, pathbar_h + content_h});
    if (state->search_active)
      draw_search_bar(state, cfg, fb_width, status_h);
    else
      draw_status(state, cfg, fb_width, status_h);

    draw_popup(state, cfg);
    ImGui::End();
  }

  sg_pass_action pa = {};
  pa.colors[0].load_action = SG_LOADACTION_CLEAR;
  pa.colors[0].clear_value = {
    cfg.color_bg.r/255.f, cfg.color_bg.g/255.f, cfg.color_bg.b/255.f, 1.f };
  sg_begin_pass({ .action = pa, .swapchain = sglue_swapchain() });
  simgui_render();
  sg_end_pass();
  sg_commit();
}

// ============================================================
// Event forwarding
// ============================================================

void ui_handle_event(const sapp_event *e) {
  simgui_handle_event(e);
}
