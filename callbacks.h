#pragma once
#include <gtk/gtk.h>
#include <filesystem>
#include <string>

// --- Shared application state ---
// Passed as user_data to every GTK signal handler so callbacks can reach
// both the filesystem state and the widgets they need to update.

struct AppState {
  // Filesystem
  std::filesystem::path current_path;

  // Widgets callbacks need to update
  GtkWidget       *tree_view;
  GtkListStore    *list_store;
  GtkWidget       *path_entry;
  GtkWidget       *status_bar;
  guint            status_ctx;
};

AppState *app_state_new();
void      app_state_free(AppState *state);

// Rebuilds the file list for state->current_path.
// Call this any time the directory changes.
void refresh_file_list(AppState *state);

// --- Signal handlers ---

// Toolbar buttons
void on_navigate_up   (GtkToolButton *btn,  AppState *state);
void on_mkdir_clicked (GtkToolButton *btn,  AppState *state);
void on_mkfile_clicked(GtkToolButton *btn,  AppState *state);
void on_delete_clicked(GtkToolButton *btn,  AppState *state);

// Path bar
void on_path_entry_activate(GtkWidget *widget, AppState *state);

// File list
void on_row_activated(GtkTreeView       *tree,
                      GtkTreePath       *path,
                      GtkTreeViewColumn *col,
                      AppState          *state);
