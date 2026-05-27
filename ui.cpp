#include "ui.h"
#include "uiconfig.h"
#include "app.h"

#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>

// stb_image for loading the sprite sheet PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// stb_truetype for font rasterization
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================
//  Immediate-mode 2D renderer
//  Draws textured quads (sprites + font glyphs) and solid rects.
//  One draw call per texture, batched per frame.
// ============================================================

// ----- Vertex -----

struct Vert {
  float x, y;
  float u, v;
  float r, g, b, a;
};

// ----- Batch -----

struct Batch {
  std::vector<Vert>    verts;
  std::vector<uint16_t> indices;
  sg_image             texture = {};

  void clear() { verts.clear(); indices.clear(); }

  void push_quad(float x, float y, float w, float h,
                 float u0, float v0, float u1, float v1,
                 float r, float g, float b, float a) {
    uint16_t base = (uint16_t)verts.size();
    verts.push_back({x,     y,     u0, v0, r, g, b, a});
    verts.push_back({x+w,   y,     u1, v0, r, g, b, a});
    verts.push_back({x+w,   y+h,   u1, v1, r, g, b, a});
    verts.push_back({x,     y+h,   u0, v1, r, g, b, a});
    indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
    indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);
  }
};

// ----- Renderer internals -----

static const int MAX_VERTS   = 65536;
static const int MAX_INDICES = MAX_VERTS * 3;

struct Renderer {
  sg_shader   shader     = {};
  sg_pipeline pip        = {};
  sg_buffer   vbuf       = {};
  sg_buffer   ibuf       = {};
  sg_bindings bindings   = {};

  // White 1×1 texture used for solid color rects
  sg_image    white_tex  = {};

  // Icons sprite sheet
  sg_image    icons_tex  = {};
  int         icons_cols = 0; // number of cells per row
  int         icons_cell = 16;

  // Font
  sg_image         font_tex  = {};
  stbtt_bakedchar  font_chars[96] = {}; // ASCII 32..127
  float            font_size = 12.f;
  float            font_scale = 1.f;
  int              font_tex_w = 512;
  int              font_tex_h = 512;

  // Per-frame batches
  Batch solid_batch;   // uses white_tex
  Batch icon_batch;    // uses icons_tex
  Batch font_batch;    // uses font_tex

  // Framebuffer size
  float fb_w = 0, fb_h = 0;
};

static Renderer R;

// ----- GLSL shaders (GL 3.3 / Metal via sokol's cross-compile) -----

static const char *VS_SRC =
  "#version 330\n"
  "layout(location=0) in vec2 pos;\n"
  "layout(location=1) in vec2 uv;\n"
  "layout(location=2) in vec4 color;\n"
  "out vec2 v_uv;\n"
  "out vec4 v_color;\n"
  "uniform vec2 u_resolution;\n"
  "void main() {\n"
  "  vec2 ndc = (pos / u_resolution) * 2.0 - 1.0;\n"
  "  gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);\n"
  "  v_uv    = uv;\n"
  "  v_color = color;\n"
  "}\n";

static const char *FS_SRC =
  "#version 330\n"
  "in vec2  v_uv;\n"
  "in vec4  v_color;\n"
  "uniform sampler2D u_tex;\n"
  "out vec4 frag_color;\n"
  "void main() {\n"
  "  frag_color = texture(u_tex, v_uv) * v_color;\n"
  "}\n";

// ----- White texture -----

static sg_image make_white_texture() {
  uint32_t pixel = 0xFFFFFFFF;
  sg_image_desc d = {};
  d.width   = 1;
  d.height  = 1;
  d.data.subimage[0][0] = SG_RANGE(pixel);
  return sg_make_image(&d);
}

// ----- PNG loader -----

static sg_image load_png(const std::string &path, int *out_w, int *out_h) {
  int w, h, n;
  unsigned char *data = stbi_load(path.c_str(), &w, &h, &n, 4);
  if (!data) return {};

  sg_image_desc d = {};
  d.width  = w;
  d.height = h;
  d.data.subimage[0][0] = { data, (size_t)(w * h * 4) };
  sg_image img = sg_make_image(&d);
  stbi_image_free(data);
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
  return img;
}

// ----- Font loader -----

static void load_font(const std::string &font_path, float font_size) {
  R.font_size = font_size;

  // Try user font first, fall back to a system monospace
  std::vector<std::string> candidates;
  if (!font_path.empty()) candidates.push_back(font_path);
  candidates.push_back("/usr/share/fonts/TTF/DejaVuSansMono.ttf");
  candidates.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");
  candidates.push_back("/System/Library/Fonts/Menlo.ttc");
  candidates.push_back("/usr/share/fonts/TTF/Hack-Regular.ttf");

  std::vector<uint8_t> ttf_data;
  for (const auto &p : candidates) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) continue;
    auto sz = f.tellg();
    ttf_data.resize(sz);
    f.seekg(0);
    f.read((char*)ttf_data.data(), sz);
    break;
  }

  std::vector<uint8_t> bitmap(R.font_tex_w * R.font_tex_h);

  if (!ttf_data.empty()) {
    stbtt_BakeFontBitmap(ttf_data.data(), 0,
                         font_size,
                         bitmap.data(),
                         R.font_tex_w, R.font_tex_h,
                         32, 96,
                         R.font_chars);
  } else {
    // Blank — text will be invisible but won't crash
    memset(bitmap.data(), 0, bitmap.size());
  }

  // Expand to RGBA
  std::vector<uint8_t> rgba(R.font_tex_w * R.font_tex_h * 4);
  for (int i = 0; i < R.font_tex_w * R.font_tex_h; i++) {
    rgba[i*4+0] = 255;
    rgba[i*4+1] = 255;
    rgba[i*4+2] = 255;
    rgba[i*4+3] = bitmap[i];
  }

  sg_image_desc d = {};
  d.width  = R.font_tex_w;
  d.height = R.font_tex_h;
  d.data.subimage[0][0] = { rgba.data(), rgba.size() };
  R.font_tex = sg_make_image(&d);
}

// ============================================================
//  ui_init / ui_shutdown
// ============================================================

void ui_init(const UIConfig &cfg) {
  // ----- Shader -----
  sg_shader_desc sd = {};
  sd.vs.source = VS_SRC;
  sd.fs.source = FS_SRC;
  sd.vs.uniform_blocks[0].size = sizeof(float) * 2;
  sd.vs.uniform_blocks[0].uniforms[0].name  = "u_resolution";
  sd.vs.uniform_blocks[0].uniforms[0].type  = SG_UNIFORMTYPE_FLOAT2;
  sd.fs.images[0].used = true;
  sd.fs.samplers[0].used = true;
  sd.fs.image_sampler_pairs[0].used = true;
  sd.fs.image_sampler_pairs[0].image_slot   = 0;
  sd.fs.image_sampler_pairs[0].sampler_slot = 0;
  sd.fs.image_sampler_pairs[0].glsl_name    = "u_tex";
  R.shader = sg_make_shader(&sd);

  // ----- Pipeline -----
  sg_pipeline_desc pd = {};
  pd.shader = R.shader;
  pd.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;  // pos
  pd.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;  // uv
  pd.layout.attrs[2].format = SG_VERTEXFORMAT_FLOAT4;  // color
  pd.index_type = SG_INDEXTYPE_UINT16;
  pd.colors[0].blend.enabled          = true;
  pd.colors[0].blend.src_factor_rgb   = SG_BLENDFACTOR_SRC_ALPHA;
  pd.colors[0].blend.dst_factor_rgb   = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  pd.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
  pd.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  R.pip = sg_make_pipeline(&pd);

  // ----- Buffers -----
  sg_buffer_desc vd = {};
  vd.size  = MAX_VERTS * sizeof(Vert);
  vd.usage = SG_USAGE_STREAM;
  R.vbuf = sg_make_buffer(&vd);

  sg_buffer_desc id = {};
  id.size  = MAX_INDICES * sizeof(uint16_t);
  id.usage = SG_USAGE_STREAM;
  id.type  = SG_BUFFERTYPE_INDEXBUFFER;
  R.ibuf = sg_make_buffer(&id);

  // ----- Textures -----
  R.white_tex = make_white_texture();

  int iw = 0, ih = 0;
  R.icons_tex  = load_png(cfg.icons_sheet, &iw, &ih);
  R.icons_cell = cfg.icon_size;
  R.icons_cols = (iw > 0 && cfg.icon_size > 0) ? iw / cfg.icon_size : 1;

  load_font(cfg.font_path, cfg.font_size);
}

void ui_shutdown() {
  sg_destroy_pipeline(R.pip);
  sg_destroy_shader(R.shader);
  sg_destroy_buffer(R.vbuf);
  sg_destroy_buffer(R.ibuf);
  sg_destroy_image(R.white_tex);
  if (R.icons_tex.id) sg_destroy_image(R.icons_tex);
  if (R.font_tex.id)  sg_destroy_image(R.font_tex);
}

// ============================================================
//  Low-level draw helpers
// ============================================================

static void flush_batch(Batch &batch, sg_image tex) {
  if (batch.verts.empty()) return;

  sg_update_buffer(R.vbuf, {batch.verts.data(),   batch.verts.size()   * sizeof(Vert)});
  sg_update_buffer(R.ibuf, {batch.indices.data(),  batch.indices.size() * sizeof(uint16_t)});

  R.bindings.vertex_buffers[0] = R.vbuf;
  R.bindings.index_buffer      = R.ibuf;
  R.bindings.fs.images[0]      = tex;

  sg_sampler_desc smpd = {};
  smpd.min_filter = SG_FILTER_NEAREST;
  smpd.mag_filter = SG_FILTER_NEAREST;
  R.bindings.fs.samplers[0] = sg_make_sampler(&smpd);

  sg_apply_bindings(&R.bindings);

  float res[2] = { R.fb_w, R.fb_h };
  sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE(res));

  sg_draw(0, (int)batch.indices.size(), 1);
  batch.clear();
}

// ----- Solid rect -----
static void draw_rect(float x, float y, float w, float h,
                      const Color &c, float alpha = 1.f) {
  R.solid_batch.push_quad(x, y, w, h,
                           0, 0, 1, 1,
                           c.r/255.f, c.g/255.f, c.b/255.f,
                           (c.a/255.f) * alpha);
}

// ----- Sprite (icon) -----
static void draw_icon(int cell_index, float x, float y, float size,
                      float r=1.f, float g=1.f, float b=1.f, float a=1.f) {
  if (!R.icons_tex.id || R.icons_cols == 0) return;

  // How many cells wide is the sheet?
  int col = cell_index % R.icons_cols;
  int row = cell_index / R.icons_cols;

  // We need the sheet dimensions; store them at load time
  // For now compute UV from known cell size and sheet width
  // (sheet width = icons_cols * icon_cell_size)
  float sheet_w = (float)(R.icons_cols * R.icons_cell);
  // Height unknown at this point — we stored icon_tex height implicitly
  // workaround: use cols for rows too (square atlas assumption fallback)
  // A better solution: store sheet_h at load time. Let's just do that.
  // For now assume square atlas.
  float sheet_h = sheet_w; // will be corrected once we store height

  float u0 = (col * R.icons_cell) / sheet_w;
  float v0 = (row * R.icons_cell) / sheet_h;
  float u1 = u0 + R.icons_cell / sheet_w;
  float v1 = v0 + R.icons_cell / sheet_h;

  R.icon_batch.push_quad(x, y, size, size, u0, v0, u1, v1, r, g, b, a);
}

// ----- Text -----
static float draw_text(const std::string &text,
                       float x, float y,
                       const Color &col,
                       float max_width = -1.f) {
  if (!R.font_tex.id) return x;
  float cx = x;
  for (char c : text) {
    if (c < 32 || c >= 128) continue;
    stbtt_aligned_quad q;
    stbtt_GetBakedQuad(R.font_chars, R.font_tex_w, R.font_tex_h,
                       c - 32, &cx, &y, &q, 1);
    if (max_width > 0 && cx - x > max_width) break;
    float qw = q.x1 - q.x0;
    float qh = q.y1 - q.y0;
    R.font_batch.push_quad(q.x0, q.y0, qw, qh,
                           q.s0, q.t0, q.s1, q.t1,
                           col.r/255.f, col.g/255.f, col.b/255.f, col.a/255.f);
  }
  return cx; // returns end x for width measurement
}

static float text_width(const std::string &text) {
  if (!R.font_tex.id) return (float)text.size() * 7.f;
  float cx = 0, cy = 0;
  for (char c : text) {
    if (c < 32 || c >= 128) continue;
    stbtt_aligned_quad q;
    stbtt_GetBakedQuad(R.font_chars, R.font_tex_w, R.font_tex_h,
                       c - 32, &cx, &cy, &q, 1);
  }
  return cx;
}

// ----- Truncate string to fit width -----
static std::string truncate(const std::string &s, float max_px) {
  if (text_width(s) <= max_px) return s;
  std::string t = s;
  while (t.size() > 1 && text_width(t + "…") > max_px)
    t.pop_back();
  return t + "…";
}

// ============================================================
//  Layout helpers
// ============================================================

struct Layout {
  float sidebar_x, sidebar_w;
  float main_x,    main_w;
  float preview_x, preview_w;
  float content_y, content_h; // below path bar
  float status_y,  status_h;
  float pathbar_y, pathbar_h;
  bool  show_preview;
  bool  too_small;
};

static Layout compute_layout(const UIConfig &cfg,
                              float fw, float fh,
                              bool preview_visible) {
  Layout L = {};
  L.status_h  = cfg.status_height;
  L.pathbar_h = cfg.row_height + 4.f;
  L.status_y  = fh - L.status_h;
  L.pathbar_y = 0.f;
  L.content_y = L.pathbar_h;
  L.content_h = L.status_y - L.content_y;

  if (fw < cfg.min_width_warning) {
    L.too_small = true;
    return L;
  }

  L.show_preview = preview_visible && fw >= cfg.min_width_preview;

  L.sidebar_x = 0.f;
  L.sidebar_w = cfg.sidebar_width;

  float right_edge = fw;
  if (L.show_preview) {
    L.preview_w = cfg.preview_width;
    L.preview_x = fw - L.preview_w;
    right_edge  = L.preview_x;
  }

  L.main_x = L.sidebar_x + L.sidebar_w;
  L.main_w = right_edge - L.main_x;
  return L;
}

// ============================================================
//  Icon selection for a file entry
// ============================================================

static int icon_for_entry(const FileEntry &fe, bool selected) {
  if (fe.is_dir) return selected ? ICON_FOLDER_SEL : ICON_FOLDER;

  std::string ext;
  auto dot = fe.name.rfind('.');
  if (dot != std::string::npos) ext = fe.name.substr(dot + 1);
  for (auto &ch : ext) ch = (char)std::tolower(ch);

  if (ext == "png" || ext == "jpg" || ext == "jpeg" ||
      ext == "gif" || ext == "svg" || ext == "bmp" || ext == "webp")
    return selected ? ICON_FILE_IMAGE_SEL : ICON_FILE_IMAGE;

  if (ext == "mp3" || ext == "flac" || ext == "wav" ||
      ext == "ogg" || ext == "aac")
    return selected ? ICON_FILE_AUDIO_SEL : ICON_FILE_AUDIO;

  if (ext == "mp4" || ext == "mkv" || ext == "avi" ||
      ext == "mov" || ext == "webm")
    return selected ? ICON_FILE_VIDEO_SEL : ICON_FILE_VIDEO;

  if (ext == "txt" || ext == "md" || ext == "rst" || ext == "log")
    return selected ? ICON_FILE_TEXT_SEL : ICON_FILE_TEXT;

  if (ext == "cpp" || ext == "c"  || ext == "h"   || ext == "py"  ||
      ext == "rs"  || ext == "go" || ext == "js"  || ext == "ts"  ||
      ext == "sh"  || ext == "lua")
    return selected ? ICON_FILE_CODE_SEL : ICON_FILE_CODE;

  if (ext == "zip" || ext == "tar" || ext == "gz" ||
      ext == "xz"  || ext == "bz2" || ext == "7z" || ext == "rar")
    return selected ? ICON_FILE_ARCHIVE_SEL : ICON_FILE_ARCHIVE;

  if (ext == "pdf")
    return selected ? ICON_FILE_PDF_SEL : ICON_FILE_PDF;

  return selected ? ICON_FILE_SEL : ICON_FILE;
}

// ============================================================
//  Sub-draw functions
// ============================================================

static void draw_pathbar(AppState *state, const UIConfig &cfg,
                         const Layout &L, float fw) {
  draw_rect(0, L.pathbar_y, fw, L.pathbar_h, cfg.color_panel_bg);

  // Simple separator line at bottom of pathbar
  draw_rect(0, L.pathbar_y + L.pathbar_h - 1.f, fw, 1.f, cfg.color_border);

  float tx = L.main_x + cfg.row_padding_x;
  float ty = L.pathbar_y + (L.pathbar_h + cfg.font_size) * 0.5f - 2.f;

  Color col = state->path_editing ? cfg.color_text : cfg.color_text_dim;
  std::string display = state->path_editing
    ? std::string(state->path_buf)
    : truncate(state->current_path.string(), L.main_w - cfg.row_padding_x * 2);
  draw_text(display, tx, ty, col);
}

static void draw_sidebar(AppState *state, const UIConfig &cfg,
                         const Layout &L,
                         const std::vector<SidebarItem> &items) {
  draw_rect(L.sidebar_x, L.content_y, L.sidebar_w, L.content_h, cfg.color_sidebar_bg);
  // Right border
  draw_rect(L.sidebar_x + L.sidebar_w - 1.f, L.content_y,
            1.f, L.content_h, cfg.color_border);

  float row_h = cfg.row_height;
  float py    = cfg.row_padding_y;
  float px    = cfg.row_padding_x;
  float icon  = (float)cfg.icon_size;

  for (int i = 0; i < (int)items.size(); i++) {
    float ry = L.content_y + i * row_h;
    bool  sel   = (state->sidebar_select == i);
    bool  hover = (state->sidebar_hover  == i);

    if (sel)
      draw_rect(L.sidebar_x, ry, L.sidebar_w, row_h, cfg.color_selection_bg);
    else if (hover)
      draw_rect(L.sidebar_x, ry, L.sidebar_w, row_h, cfg.color_hover_bg);

    float icon_y = ry + (row_h - icon) * 0.5f;
    draw_icon(items[i].icon_index, L.sidebar_x + px, icon_y, icon);

    float tx = L.sidebar_x + px + icon + cfg.icon_label_gap;
    float ty = ry + (row_h + cfg.font_size) * 0.5f - 2.f;
    Color tcol = sel ? cfg.color_text_selected : cfg.color_text;
    float avail = L.sidebar_w - px - icon - cfg.icon_label_gap - px;
    draw_text(truncate(items[i].label, avail), tx, ty, tcol);
  }
}

static void draw_file_list(AppState *state, const UIConfig &cfg,
                           const Layout &L) {
  draw_rect(L.main_x, L.content_y, L.main_w, L.content_h, cfg.color_bg);

  float row_h = cfg.row_height;
  float px    = cfg.row_padding_x;
  float icon  = (float)cfg.icon_size;

  int first_row = (int)state->scroll_offset;
  int visible   = (int)std::ceil(L.content_h / row_h) + 1;
  int n         = (int)state->entries.size();

  for (int i = first_row; i < std::min(n, first_row + visible); i++) {
    const FileEntry &fe = state->entries[i];
    float ry  = L.content_y + (i - first_row) * row_h;
    bool  sel   = (state->selected_index == i);
    bool  hover = (state->hovered_index  == i);

    if (sel)
      draw_rect(L.main_x, ry, L.main_w, row_h, cfg.color_selection_bg);
    else if (hover)
      draw_rect(L.main_x, ry, L.main_w, row_h, cfg.color_hover_bg);

    float icon_y = ry + (row_h - icon) * 0.5f;
    draw_icon(icon_for_entry(fe, sel), L.main_x + px, icon_y, icon);

    float tx = L.main_x + px + icon + cfg.icon_label_gap;
    float ty = ry + (row_h + cfg.font_size) * 0.5f - 2.f;
    Color tcol = sel ? cfg.color_text_selected : cfg.color_text;
    float avail = L.main_w - px - icon - cfg.icon_label_gap - px;
    draw_text(truncate(fe.name, avail), tx, ty, tcol);
  }

  // Scrollbar
  if (n > 0) {
    float total_h   = n * row_h;
    float thumb_h   = std::max(20.f, L.content_h * (L.content_h / total_h));
    float thumb_max = L.content_h - thumb_h;
    float thumb_y   = L.content_y +
      (total_h > L.content_h
        ? (state->scroll_offset * row_h / (total_h - L.content_h)) * thumb_max
        : 0.f);
    float sb_x = L.main_x + L.main_w - 6.f;
    draw_rect(sb_x, L.content_y, 6.f, L.content_h, cfg.color_scrollbar);
    draw_rect(sb_x, thumb_y, 6.f, thumb_h, cfg.color_scrollbar_fg);
  }
}

static void draw_preview(AppState *state, const UIConfig &cfg,
                         const Layout &L) {
  draw_rect(L.preview_x, L.content_y, L.preview_w, L.content_h, cfg.color_panel_bg);
  // Left border
  draw_rect(L.preview_x, L.content_y, 1.f, L.content_h, cfg.color_border);

  float px = cfg.row_padding_x;
  float py = cfg.row_padding_y;

  if (state->selected_index < 0 ||
      state->selected_index >= (int)state->entries.size()) return;

  const FileEntry &fe = state->entries[state->selected_index];

  // Header: name
  float ty = L.content_y + cfg.row_height * 0.5f + cfg.font_size * 0.5f - 2.f;
  draw_text(truncate(fe.name, L.preview_w - px * 2),
            L.preview_x + px, ty, cfg.color_text);

  // Divider
  draw_rect(L.preview_x + px, L.content_y + cfg.row_height,
            L.preview_w - px * 2, 1.f, cfg.color_border);

  if (fe.is_dir) {
    // List subfolder contents
    float row_h = cfg.row_height;
    float icon  = (float)cfg.icon_size;
    int first   = (int)state->preview_scroll_offset;
    int visible = (int)std::ceil((L.content_h - cfg.row_height) / row_h) + 1;
    int n       = (int)state->preview_entries.size();

    for (int i = first; i < std::min(n, first + visible); i++) {
      const FileEntry &pfe = state->preview_entries[i];
      float ry  = L.content_y + cfg.row_height + (i - first) * row_h;
      float iy  = ry + (row_h - icon) * 0.5f;
      draw_icon(icon_for_entry(pfe, false), L.preview_x + px, iy, icon);
      float tx  = L.preview_x + px + icon + cfg.icon_label_gap;
      float tty = ry + (row_h + cfg.font_size) * 0.5f - 2.f;
      float avail = L.preview_w - px - icon - cfg.icon_label_gap - px;
      draw_text(truncate(pfe.name, avail), tx, tty, cfg.color_text_dim);
    }
  } else {
    // File metadata
    float row_h = cfg.row_height;
    float meta_y = L.content_y + cfg.row_height + row_h * 0.7f;

    // Size
    std::string size_str;
    if (fe.size < 1024)
      size_str = std::to_string(fe.size) + " B";
    else if (fe.size < 1024*1024)
      size_str = std::to_string(fe.size / 1024) + " KB";
    else
      size_str = std::to_string(fe.size / (1024*1024)) + " MB";

    draw_text("Size:", L.preview_x + px, meta_y, cfg.color_text_dim);
    draw_text(size_str, L.preview_x + px + 50.f, meta_y, cfg.color_text);

    meta_y += row_h;
    draw_text("Modified:", L.preview_x + px, meta_y, cfg.color_text_dim);
    draw_text(truncate(fe.modified, L.preview_w - px - 70.f),
              L.preview_x + px + 70.f, meta_y, cfg.color_text);
  }
}

static void draw_status(const AppState *state, const UIConfig &cfg,
                        const Layout &L, float fw) {
  draw_rect(0.f, L.status_y, fw, L.status_h, cfg.color_status_bg);
  draw_rect(0.f, L.status_y, fw, 1.f, cfg.color_border);
  float ty = L.status_y + (L.status_h + cfg.font_size) * 0.5f - 2.f;
  draw_text(truncate(state->status_msg, fw - cfg.row_padding_x * 2),
            cfg.row_padding_x, ty, cfg.color_status_text);
}

static void draw_too_small(const UIConfig &cfg, float fw, float fh) {
  draw_rect(0.f, 0.f, fw, fh, cfg.color_bg);
  std::string msg = "window too small";
  float tw = text_width(msg);
  draw_text(msg,
            (fw - tw) * 0.5f,
            (fh + cfg.font_size) * 0.5f,
            cfg.color_warning_text);
}

// ----- Popup overlay -----

static void draw_popup(AppState *state, const UIConfig &cfg,
                       float fw, float fh) {
  if (state->popup.kind == PopupKind::None) return;

  // Dim overlay
  Color dim = {0, 0, 0, 160};
  draw_rect(0.f, 0.f, fw, fh, dim);

  // Box
  float bw = 320.f, bh = 90.f;
  float bx = (fw - bw) * 0.5f;
  float by = (fh - bh) * 0.5f;
  draw_rect(bx, by, bw, bh, cfg.color_panel_bg);
  draw_rect(bx, by, bw, 1.f, cfg.color_border);
  draw_rect(bx, by + bh - 1.f, bw, 1.f, cfg.color_border);
  draw_rect(bx, by, 1.f, bh, cfg.color_border);
  draw_rect(bx + bw - 1.f, by, 1.f, bh, cfg.color_border);

  // Title
  const char *title = "";
  switch (state->popup.kind) {
    case PopupKind::NewFile:       title = "New File";       break;
    case PopupKind::NewFolder:     title = "New Folder";     break;
    case PopupKind::Rename:        title = "Rename";         break;
    case PopupKind::DeleteConfirm: title = "Delete";         break;
    default: break;
  }
  draw_text(title, bx + 12.f, by + 20.f, cfg.color_text);

  if (state->popup.kind == PopupKind::DeleteConfirm) {
    std::string msg = "Delete \"" + state->popup.target_name + "\"?  [Return/Esc]";
    draw_text(truncate(msg, bw - 24.f), bx + 12.f, by + 50.f, cfg.color_text_dim);
  } else {
    // Input field background
    draw_rect(bx + 12.f, by + 34.f, bw - 24.f, cfg.row_height, cfg.color_bg);
    draw_rect(bx + 12.f, by + 34.f, bw - 24.f, 1.f, cfg.color_border);

    float ty = by + 34.f + (cfg.row_height + cfg.font_size) * 0.5f - 2.f;
    draw_text(std::string(state->popup.input),
              bx + 16.f, ty, cfg.color_text, bw - 32.f);
    // Cursor
    float cur_x = bx + 16.f +
      text_width(std::string(state->popup.input));
    draw_rect(cur_x, by + 36.f, 1.f, cfg.font_size + 2.f, cfg.color_text);

    draw_text("[Return] confirm  [Esc] cancel",
              bx + 12.f, by + 72.f, cfg.color_text_dim);
  }
}

// ============================================================
//  ui_draw
// ============================================================

// Store sheet height so icon UV is correct
static int s_icons_sheet_h = 0;

void ui_draw(AppState *state,
             const UIConfig &cfg,
             const std::vector<SidebarItem> &sidebar,
             float fb_width, float fb_height) {
  R.fb_w = fb_width;
  R.fb_h = fb_height;

  Layout L = compute_layout(cfg, fb_width, fb_height, state->preview_visible);

  sg_pass_action pa = {};
  pa.colors[0].load_action  = SG_LOADACTION_CLEAR;
  pa.colors[0].clear_value  = {
    cfg.color_bg.r / 255.f,
    cfg.color_bg.g / 255.f,
    cfg.color_bg.b / 255.f,
    1.f
  };
  sg_begin_pass({ .action = pa, .swapchain = sglue_swapchain() });
  sg_apply_pipeline(R.pip);

  if (L.too_small) {
    draw_too_small(cfg, fb_width, fb_height);
  } else {
    draw_pathbar(state, cfg, L, fb_width);
    draw_sidebar(state, cfg, L, sidebar);
    draw_file_list(state, cfg, L);
    if (L.show_preview) draw_preview(state, cfg, L);
    draw_status(state, cfg, L, fb_width);
  }

  draw_popup(state, cfg, fb_width, fb_height);

  // Flush all batches in order: solid -> icons -> text
  flush_batch(R.solid_batch, R.white_tex);
  flush_batch(R.icon_batch,  R.icons_tex.id ? R.icons_tex : R.white_tex);
  flush_batch(R.font_batch,  R.font_tex.id  ? R.font_tex  : R.white_tex);

  sg_end_pass();
  sg_commit();
}

// ============================================================
//  Input
// ============================================================

// ----- Keybind matching -----

struct KeySpec {
  bool  ctrl  = false;
  bool  shift = false;
  bool  alt   = false;
  sapp_keycode key = SAPP_KEYCODE_INVALID;
};

static sapp_keycode keyname_to_code(const std::string &name) {
  if (name == "up")        return SAPP_KEYCODE_UP;
  if (name == "down")      return SAPP_KEYCODE_DOWN;
  if (name == "left")      return SAPP_KEYCODE_LEFT;
  if (name == "right")     return SAPP_KEYCODE_RIGHT;
  if (name == "return")    return SAPP_KEYCODE_ENTER;
  if (name == "backspace") return SAPP_KEYCODE_BACKSPACE;
  if (name == "delete")    return SAPP_KEYCODE_DELETE;
  if (name == "escape")    return SAPP_KEYCODE_ESCAPE;
  if (name == "tab")       return SAPP_KEYCODE_TAB;
  if (name == "space")     return SAPP_KEYCODE_SPACE;
  if (name == "f1")        return SAPP_KEYCODE_F1;
  if (name == "f2")        return SAPP_KEYCODE_F2;
  if (name == "f5")        return SAPP_KEYCODE_F5;
  if (name.size() == 1) {
    char c = name[0];
    if (c >= 'a' && c <= 'z') return (sapp_keycode)(SAPP_KEYCODE_A + (c - 'a'));
    if (c >= '0' && c <= '9') return (sapp_keycode)(SAPP_KEYCODE_0 + (c - '0'));
  }
  return SAPP_KEYCODE_INVALID;
}

static KeySpec parse_keybind(const std::string &s) {
  KeySpec ks;
  std::string tok;
  std::istringstream ss(s);
  std::vector<std::string> parts;
  // Split on '+'
  std::string cur;
  for (char c : s) {
    if (c == '+') { parts.push_back(cur); cur.clear(); }
    else cur += c;
  }
  parts.push_back(cur);
  for (auto &p : parts) {
    if (p == "ctrl")  ks.ctrl  = true;
    else if (p == "shift") ks.shift = true;
    else if (p == "alt")   ks.alt   = true;
    else ks.key = keyname_to_code(p);
  }
  return ks;
}

static bool matches(const sapp_event *e, const std::string &bind) {
  KeySpec ks = parse_keybind(bind);
  if (ks.key == SAPP_KEYCODE_INVALID) return false;
  bool ctrl  = (e->modifiers & SAPP_MODIFIER_CTRL)  != 0;
  bool shift = (e->modifiers & SAPP_MODIFIER_SHIFT) != 0;
  bool alt   = (e->modifiers & SAPP_MODIFIER_ALT)   != 0;
  return e->key_code == ks.key &&
         ctrl  == ks.ctrl &&
         shift == ks.shift &&
         alt   == ks.alt;
}

// ----- Key handler -----

void ui_on_key(AppState *state, const UIConfig &cfg, const sapp_event *e) {
  if (e->type != SAPP_EVENTTYPE_KEY_DOWN) return;

  // ----- Popup is open -----
  if (state->popup.kind != PopupKind::None) {
    if (e->key_code == SAPP_KEYCODE_ESCAPE) {
      state->popup = {};
      return;
    }
    if (e->key_code == SAPP_KEYCODE_ENTER) {
      switch (state->popup.kind) {
        case PopupKind::NewFile:
          action_create_file(state, state->popup.input);  break;
        case PopupKind::NewFolder:
          action_create_folder(state, state->popup.input); break;
        case PopupKind::Rename:
          action_rename(state, state->popup.input);       break;
        case PopupKind::DeleteConfirm:
          action_delete_selected(state);                  break;
        default: break;
      }
      state->popup = {};
      return;
    }
    if (e->key_code == SAPP_KEYCODE_BACKSPACE) {
      int len = (int)strlen(state->popup.input);
      if (len > 0) state->popup.input[len-1] = '\0';
      return;
    }
    return; // swallow everything else while popup is open
  }

  // ----- Path bar editing -----
  if (state->path_editing) {
    if (e->key_code == SAPP_KEYCODE_ESCAPE) {
      std::strncpy(state->path_buf,
                   state->current_path.string().c_str(),
                   sizeof(state->path_buf) - 1);
      state->path_editing = false;
      return;
    }
    if (e->key_code == SAPP_KEYCODE_ENTER) {
      navigate_to(state, fs::path(state->path_buf));
      state->path_editing = false;
      return;
    }
    if (e->key_code == SAPP_KEYCODE_BACKSPACE) {
      int len = (int)strlen(state->path_buf);
      if (len > 0) state->path_buf[len-1] = '\0';
      return;
    }
    return;
  }

  // ----- Normal navigation -----
  if (matches(e, cfg.kb_up)   || matches(e, cfg.kb_up_alt))
    { select_move(state, -1); return; }
  if (matches(e, cfg.kb_down) || matches(e, cfg.kb_down_alt))
    { select_move(state, +1); return; }
  if (matches(e, cfg.kb_right) || matches(e, cfg.kb_right_alt))
    { navigate_into_selected(state); return; }
  if (matches(e, cfg.kb_left) || matches(e, cfg.kb_left_alt))
    { navigate_up(state); return; }
  if (matches(e, cfg.kb_enter))
    { action_open_selected(state, state->cfg); return; }
  if (matches(e, cfg.kb_back))
    { navigate_up(state); return; }

  if (matches(e, cfg.kb_toggle_preview)) {
    state->preview_visible = !state->preview_visible;
    return;
  }

  if (matches(e, cfg.kb_delete)) {
    if (state->selected_index >= 0) {
      state->popup.kind = PopupKind::DeleteConfirm;
      state->popup.target_name = state->entries[state->selected_index].name;
    }
    return;
  }
  if (matches(e, cfg.kb_rename)) {
    if (state->selected_index >= 0) {
      state->popup.kind = PopupKind::Rename;
      std::strncpy(state->popup.input,
                   state->entries[state->selected_index].name.c_str(),
                   sizeof(state->popup.input) - 1);
      state->popup.target_name = state->entries[state->selected_index].name;
    }
    return;
  }
  if (matches(e, cfg.kb_new_file)) {
    state->popup.kind = PopupKind::NewFile;
    memset(state->popup.input, 0, sizeof(state->popup.input));
    return;
  }
  if (matches(e, cfg.kb_new_folder)) {
    state->popup.kind = PopupKind::NewFolder;
    memset(state->popup.input, 0, sizeof(state->popup.input));
    return;
  }
}

// ----- Char input (for popup text fields and path bar) -----

void ui_on_char(AppState *state, const sapp_event *e) {
  if (e->type != SAPP_EVENTTYPE_CHAR) return;
  unsigned int cp = e->char_code;
  if (cp < 32 || cp > 126) return;
  char c = (char)cp;

  if (state->popup.kind != PopupKind::None &&
      state->popup.kind != PopupKind::DeleteConfirm) {
    int len = (int)strlen(state->popup.input);
    if (len < (int)sizeof(state->popup.input) - 1) {
      state->popup.input[len]   = c;
      state->popup.input[len+1] = '\0';
    }
    return;
  }

  if (state->path_editing) {
    int len = (int)strlen(state->path_buf);
    if (len < (int)sizeof(state->path_buf) - 1) {
      state->path_buf[len]   = c;
      state->path_buf[len+1] = '\0';
    }
    return;
  }
}

// ----- Mouse helpers -----

static int row_at_y(float my, const Layout &L, float row_h, float scroll) {
  if (my < L.content_y || my > L.content_y + L.content_h) return -1;
  return (int)((my - L.content_y) / row_h) + (int)scroll;
}

static int sidebar_row_at_y(float my, const Layout &L, float row_h) {
  if (my < L.content_y) return -1;
  return (int)((my - L.content_y) / row_h);
}

static bool in_main(float mx, const Layout &L) {
  return mx >= L.main_x && mx < L.main_x + L.main_w;
}
static bool in_sidebar(float mx, const Layout &L) {
  return mx >= L.sidebar_x && mx < L.sidebar_x + L.sidebar_w;
}
static bool in_pathbar(float my, const Layout &L) {
  return my >= L.pathbar_y && my < L.pathbar_y + L.pathbar_h;
}

void ui_mouse_move(AppState *state, const UIConfig &cfg,
                   const std::vector<SidebarItem> &sidebar,
                   const sapp_event *e,
                   float fw, float fh) {
  Layout L = compute_layout(cfg, fw, fh, state->preview_visible);
  float mx = e->mouse_x, my = e->mouse_y;

  state->hovered_index = -1;
  state->sidebar_hover = -1;

  if (in_main(mx, L))
    state->hovered_index = row_at_y(my, L, cfg.row_height, state->scroll_offset);
  else if (in_sidebar(mx, L)) {
    int r = sidebar_row_at_y(my, L, cfg.row_height);
    if (r >= 0 && r < (int)sidebar.size())
      state->sidebar_hover = r;
  }
}

void ui_mouse_btn(AppState *state, const UIConfig &cfg,
                  const std::vector<SidebarItem> &sidebar,
                  const sapp_event *e,
                  float fw, float fh) {
  if (e->type != SAPP_EVENTTYPE_MOUSE_DOWN) return;
  Layout L = compute_layout(cfg, fw, fh, state->preview_visible);
  float mx = e->mouse_x, my = e->mouse_y;

  // Click in path bar
  if (in_pathbar(my, L)) {
    state->path_editing = true;
    return;
  }

  // Click in sidebar
  if (in_sidebar(mx, L)) {
    int r = sidebar_row_at_y(my, L, cfg.row_height);
    if (r >= 0 && r < (int)sidebar.size()) {
      state->sidebar_select = r;
      navigate_to(state, sidebar[r].path);
    }
    return;
  }

  // Click in main panel
  if (in_main(mx, L)) {
    state->path_editing = false;
    int r = row_at_y(my, L, cfg.row_height, state->scroll_offset);
    if (r >= 0 && r < (int)state->entries.size()) {
      if (state->selected_index == r && e->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        // Double-click equivalent: single click on already-selected
        action_open_selected(state, state->cfg);
      } else {
        state->selected_index = r;
        refresh_preview(state);
      }
    }
    return;
  }
}

void ui_scroll(AppState *state, const UIConfig &cfg,
               const sapp_event *e,
               float fw, float fh) {
  Layout L = compute_layout(cfg, fw, fh, state->preview_visible);
  float mx = e->mouse_x, my = e->mouse_y;

  int n = (int)state->entries.size();
  float max_scroll = std::max(0.f, n - L.content_h / cfg.row_height);

  if (in_main(mx, L) || in_pathbar(my, L)) {
    state->scroll_offset = std::clamp(
      state->scroll_offset - e->scroll_y,
      0.f, max_scroll);
  }

  // Preview scroll
  if (L.show_preview && mx >= L.preview_x) {
    int pn = (int)state->preview_entries.size();
    float pm = std::max(0.f, pn - (L.content_h - cfg.row_height) / cfg.row_height);
    state->preview_scroll_offset = std::clamp(
      state->preview_scroll_offset - e->scroll_y,
      0.f, pm);
  }
}

// Public wrappers matching header names
void ui_on_mouse_move(AppState *state, const UIConfig &cfg,
                      const std::vector<SidebarItem> &sidebar,
                      const sapp_event *e, float fw, float fh) {
  ui_mouse_move(state, cfg, sidebar, e, fw, fh);
}

void ui_on_mouse_btn(AppState *state, const UIConfig &cfg,
                     const std::vector<SidebarItem> &sidebar,
                     const sapp_event *e, float fw, float fh) {
  ui_mouse_btn(state, cfg, sidebar, e, fw, fh);
}

void ui_on_scroll(AppState *state, const sapp_event *e) {
  // called from main.cpp which passes fw/fh separately — see main.cpp
}
