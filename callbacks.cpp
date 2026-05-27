#include "callbacks.h"
#include "operations.h"
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// Column indices - keep in sync with ui.cpp
enum {
  COL_ICON = 0,
  COL_NAME,
  COL_TYPE,
  NUM_COLS
};

// --- AppState lifecycle ---

AppState *app_state_new() {
  AppState *state  = new AppState();
  state->current_path = fs::current_path();
  return state;
}

void app_state_free(AppState *state) {
  delete state;
}

// --- Helpers ---

static void set_status(AppState *state, const std::string &msg) {
  gtk_statusbar_pop(GTK_STATUSBAR(state->status_bar), state->status_ctx);
  gtk_statusbar_push(GTK_STATUSBAR(state->status_bar),
                     state->status_ctx, msg.c_str());
}

// Prompts the user for a single string. Returns empty string on cancel.
static std::string prompt_text(GtkWidget *parent,
                               const char *title,
                               const char *label_text) {
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    title, GTK_WINDOW(gtk_widget_get_toplevel(parent)),
    GTK_DIALOG_MODAL,
    "_OK",     GTK_RESPONSE_OK,
    "_Cancel", GTK_RESPONSE_CANCEL,
    NULL
  );

  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *hbox    = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);

  gtk_box_pack_start(GTK_BOX(hbox),
                     gtk_label_new(label_text), FALSE, FALSE, 0);

  GtkWidget *entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(content), hbox);

  // Allow pressing Enter to confirm
  g_signal_connect_swapped(entry, "activate",
    G_CALLBACK(gtk_window_activate_default), dialog);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

  gtk_widget_show_all(dialog);

  std::string result;
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
    result = gtk_entry_get_text(GTK_ENTRY(entry));
  }
  gtk_widget_destroy(dialog);
  return result;
}

// --- refresh_file_list ---

void refresh_file_list(AppState *state) {
  gtk_list_store_clear(state->list_store);

  gtk_entry_set_text(GTK_ENTRY(state->path_entry),
                     state->current_path.string().c_str());

  std::error_code ec;
  for (const auto &entry :
       fs::directory_iterator(state->current_path, ec)) {
    bool is_dir = fs::is_directory(entry, ec);

    GtkTreeIter iter;
    gtk_list_store_append(state->list_store, &iter);
    gtk_list_store_set(state->list_store, &iter,
      COL_ICON, is_dir ? "folder" : "text-x-generic",
      COL_NAME, entry.path().filename().string().c_str(),
      COL_TYPE, is_dir ? "Directory" : "File",
      -1
    );
  }

  if (ec) {
    set_status(state, "Error reading directory: " + ec.message());
  } else {
    set_status(state, state->current_path.string());
  }
}

// --- Toolbar handlers ---

void on_navigate_up(GtkToolButton * /*btn*/, AppState *state) {
  fs::path parent = state->current_path.parent_path();
  if (parent != state->current_path) {
    state->current_path = parent;
    refresh_file_list(state);
  }
}

void on_mkdir_clicked(GtkToolButton *btn, AppState *state) {
  std::string name = prompt_text(
    GTK_WIDGET(btn), "New Folder", "Folder name:"
  );
  if (name.empty()) return;

  make_directory(state->current_path / name);
  refresh_file_list(state);
  set_status(state, "Created folder: " + name);
}

void on_mkfile_clicked(GtkToolButton *btn, AppState *state) {
  std::string name = prompt_text(
    GTK_WIDGET(btn), "New File", "File name:"
  );
  if (name.empty()) return;

  create_file(state->current_path / name);
  refresh_file_list(state);
  set_status(state, "Created file: " + name);
}

void on_delete_clicked(GtkToolButton * /*btn*/, AppState *state) {
  GtkTreeSelection *sel =
    gtk_tree_view_get_selection(GTK_TREE_VIEW(state->tree_view));

  GtkTreeModel *model;
  GtkTreeIter   iter;
  if (!gtk_tree_selection_get_selected(sel, &model, &iter)) {
    set_status(state, "Nothing selected.");
    return;
  }

  gchar *name = nullptr;
  gtk_tree_model_get(model, &iter, COL_NAME, &name, -1);

  // Confirm before deleting
  GtkWidget *confirm = gtk_message_dialog_new(
    GTK_WINDOW(gtk_widget_get_toplevel(state->tree_view)),
    GTK_DIALOG_MODAL,
    GTK_MESSAGE_WARNING,
    GTK_BUTTONS_OK_CANCEL,
    "Delete \"%s\"?", name
  );
  gint response = gtk_dialog_run(GTK_DIALOG(confirm));
  gtk_widget_destroy(confirm);

  if (response == GTK_RESPONSE_OK) {
    delete_item(state->current_path / name);
    refresh_file_list(state);
    set_status(state, std::string("Deleted: ") + name);
  }

  g_free(name);
}

// --- Path bar handler ---

void on_path_entry_activate(GtkWidget * /*widget*/, AppState *state) {
  const char *text =
    gtk_entry_get_text(GTK_ENTRY(state->path_entry));

  fs::path p(text);
  std::error_code ec;
  if (fs::is_directory(p, ec)) {
    state->current_path = fs::canonical(p, ec);
    refresh_file_list(state);
  } else {
    set_status(state, "Not a directory.");
    // Reset entry to current path
    gtk_entry_set_text(GTK_ENTRY(state->path_entry),
                       state->current_path.string().c_str());
  }
}

// --- File list handler ---

void on_row_activated(GtkTreeView       *tree,
                      GtkTreePath       *path,
                      GtkTreeViewColumn * /*col*/,
                      AppState          *state) {
  GtkTreeModel *model = gtk_tree_view_get_model(tree);
  GtkTreeIter   iter;
  if (!gtk_tree_model_get_iter(model, &iter, path)) return;

  gchar *name = nullptr;
  gchar *type = nullptr;
  gtk_tree_model_get(model, &iter,
                     COL_NAME, &name,
                     COL_TYPE, &type, -1);

  if (g_strcmp0(type, "Directory") == 0) {
    state->current_path /= name;
    refresh_file_list(state);
  }

  g_free(name);
  g_free(type);
}
