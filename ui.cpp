#include "ui.h"
#include "callbacks.h"
#include "config.h"
#include <string>

enum {
  COL_ICON = 0,
  COL_NAME,
  COL_TYPE,
  NUM_COLS
};

static GtkWidget *build_toolbar(AppState *state) {
  GtkWidget *toolbar = gtk_toolbar_new();
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);

  GtkToolItem *btn_up = gtk_tool_button_new(
    gtk_image_new_from_icon_name("go-up", GTK_ICON_SIZE_SMALL_TOOLBAR), "Up"
  );
  g_signal_connect(btn_up, "clicked", G_CALLBACK(on_navigate_up), state);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_up, -1);

  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

  GtkToolItem *btn_mkdir = gtk_tool_button_new(
    gtk_image_new_from_icon_name("folder-new", GTK_ICON_SIZE_SMALL_TOOLBAR), "New Folder"
  );
  g_signal_connect(btn_mkdir, "clicked", G_CALLBACK(on_mkdir_clicked), state);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_mkdir, -1);

  GtkToolItem *btn_mkfile = gtk_tool_button_new(
    gtk_image_new_from_icon_name("document-new", GTK_ICON_SIZE_SMALL_TOOLBAR), "New File"
  );
  g_signal_connect(btn_mkfile, "clicked", G_CALLBACK(on_mkfile_clicked), state);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_mkfile, -1);

  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

  GtkToolItem *btn_delete = gtk_tool_button_new(
    gtk_image_new_from_icon_name("edit-delete", GTK_ICON_SIZE_SMALL_TOOLBAR), "Delete"
  );
  g_signal_connect(btn_delete, "clicked", G_CALLBACK(on_delete_clicked), state);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_delete, -1);

  return toolbar;
}

static GtkWidget *build_path_bar(AppState *state) {
  GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

  GtkWidget *label = gtk_label_new("Path:");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

  state->path_entry = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(state->path_entry),
                     state->current_path.c_str());
  gtk_box_pack_start(GTK_BOX(hbox), state->path_entry, TRUE, TRUE, 0);

  GtkWidget *btn_go = gtk_button_new_with_label("Go");
  g_signal_connect(btn_go, "clicked", G_CALLBACK(on_path_entry_activate), state);
  g_signal_connect(state->path_entry, "activate",
                   G_CALLBACK(on_path_entry_activate), state);
  gtk_box_pack_start(GTK_BOX(hbox), btn_go, FALSE, FALSE, 0);

  return hbox;
}

static GtkWidget *build_file_list(AppState *state) {
  state->list_store = gtk_list_store_new(NUM_COLS,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_STRING
  );

  GtkWidget *tree = gtk_tree_view_new_with_model(
    GTK_TREE_MODEL(state->list_store)
  );
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), TRUE);
  state->tree_view = tree;

  GtkCellRenderer *icon_renderer = gtk_cell_renderer_pixbuf_new();
  GtkCellRenderer *name_renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *name_col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(name_col, "Name");
  gtk_tree_view_column_pack_start(name_col, icon_renderer, FALSE);
  gtk_tree_view_column_add_attribute(name_col, icon_renderer, "icon-name", COL_ICON);
  gtk_tree_view_column_pack_start(name_col, name_renderer, TRUE);
  gtk_tree_view_column_add_attribute(name_col, name_renderer, "text", COL_NAME);
  gtk_tree_view_column_set_expand(name_col, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree), name_col);

  GtkCellRenderer *type_renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *type_col = gtk_tree_view_column_new_with_attributes(
    "Type", type_renderer, "text", COL_TYPE, NULL
  );
  gtk_tree_view_column_set_min_width(type_col, 80);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree), type_col);

  g_signal_connect(tree, "row-activated", G_CALLBACK(on_row_activated), state);

  GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scroll), tree);

  return scroll;
}

static GtkWidget *build_status_bar(AppState *state) {
  state->status_bar = gtk_statusbar_new();
  state->status_ctx = gtk_statusbar_get_context_id(
    GTK_STATUSBAR(state->status_bar), "main"
  );
  gtk_statusbar_push(GTK_STATUSBAR(state->status_bar),
                     state->status_ctx, "Ready");
  return state->status_bar;
}

// -----

static void parse_accel(const std::string &accel,
                        guint *keyval, GdkModifierType *mods) {
  gtk_accelerator_parse(accel.c_str(), keyval, mods);
}

static void setup_keybinds(GtkWidget * /*window*/,
                           GtkAccelGroup *accel_group,
                           AppState *state) {
  auto bind = [&](const char *key, const char *fallback, GCallback cb) {
    std::string accel = config_get(state->cfg, key, fallback);
    guint keyval; GdkModifierType mods;
    parse_accel(accel, &keyval, &mods);
    if (keyval == 0) return;

    GClosure *closure = g_cclosure_new(cb, state, nullptr);
    gtk_accel_group_connect(accel_group, keyval, mods, GTK_ACCEL_VISIBLE, closure);
  };

  // Navigation
  bind(KEY_KB_BACK_DIR,      "BackSpace",      G_CALLBACK(on_navigate_up));
  bind(KEY_KB_OPEN_SEARCH,   "<ctrl>f",        G_CALLBACK(on_mkfile_clicked)); // placeholder

  // File actions
  bind(KEY_KB_DELETE,        "Delete",         G_CALLBACK(on_delete_clicked));
  bind(KEY_KB_CREATE_FILE,   "<ctrl>n",        G_CALLBACK(on_mkfile_clicked));
  bind(KEY_KB_CREATE_FOLDER, "<ctrl><shift>n", G_CALLBACK(on_mkdir_clicked));
}

// -----

GtkWidget *create_main_window(std::unordered_map<std::string, std::string> cfg) {
  AppState *state = app_state_new(std::move(cfg));

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "File Manager");
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 500);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  g_object_set_data_full(G_OBJECT(window), "app-state", state,
                         (GDestroyNotify)app_state_free);

  GtkAccelGroup *accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
  setup_keybinds(window, accel_group, state);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  gtk_box_pack_start(GTK_BOX(vbox), build_toolbar(state),    FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), build_path_bar(state),   FALSE, FALSE, 4);
  gtk_box_pack_start(GTK_BOX(vbox), build_file_list(state),  TRUE,  TRUE,  0);
  gtk_box_pack_start(GTK_BOX(vbox), build_status_bar(state), FALSE, FALSE, 0);

  refresh_file_list(state);
  return window;
}
