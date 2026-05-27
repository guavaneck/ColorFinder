#pragma once
#include <gtk/gtk.h>
#include <filesystem>
#include <unordered_map>
#include <string>

GtkWidget *create_main_window(std::unordered_map<std::string, std::string> cfg);
