#pragma once
#include <filesystem>
#include <string>

void list_directory(const std::filesystem::path &current);
void change_directory(std::filesystem::path &current, const std::string &dir);
void create_file(const std::filesystem::path &filePath);
void make_directory(const std::filesystem::path &dirPath);
void delete_item(const std::filesystem::path &target);
