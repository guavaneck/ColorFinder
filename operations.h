#pragma once
#include <filesystem>
#include <string>

void create_file(const std::filesystem::path &filePath);
void make_directory(const std::filesystem::path &dirPath);
void delete_item(const std::filesystem::path &target);
