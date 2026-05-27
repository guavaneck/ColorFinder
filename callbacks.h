#pragma once
#include <gtk/gtk.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include "config.h"

struct AppState {
  // Filesystem
  std::filesystem::path current_path;

  // Config
  std::unordered_map<std::string, std::string> cfg;

  // Widgets callbacks need to update
  GtkWidget    *tree_view;
  GtkListStore *list_store;
  GtkWidget    *path_entry;
  GtkWidget    *status_bar;
  guint         status_ctx;
};

AppState *app_state_new(std::unordered_map<std::string, std::string> cfg);
void      app_state_free(AppState *state);

void refresh_file_list(AppState *state);

// Toolbar buttons
void on_navigate_up   (GtkToolButton *btn, AppState *state);
void on_mkdir_clicked (GtkToolButton *btn, AppState *state);
void on_mkfile_clicked(GtkToolButton *btn, AppState *state);
void on_delete_clicked(GtkToolButton *btn, AppState *state);

// Path bar
void on_path_entry_activate(GtkWidget *widget, AppState *state);

// File list
void on_row_activated(GtkTreeView       *tree,
                      GtkTreePath       *path,
                      GtkTreeViewColumn *col,
                      AppState          *state);
