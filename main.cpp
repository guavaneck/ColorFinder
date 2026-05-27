#include <gtk/gtk.h>
#include "ui.h"
#include "config.h"

int main(int argc, char *argv[]) {
  config_load();
  gtk_init(&argc, &argv);

  GtkWidget *window = create_main_window();
  gtk_widget_show_all(window);

  gtk_main();
  return 0;
}
