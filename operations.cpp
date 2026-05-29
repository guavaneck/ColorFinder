#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include "operations.h"

namespace fs = std::filesystem;

void create_file(const fs::path &filePath) {
  std::ofstream file(filePath);
  if (file) {
    std::cout << "File created: " << filePath << "\n";
  } else {
    std::cout << "Failed to create file!\n";
  }
}

void make_directory(const fs::path &dirPath) {
  if (fs::create_directory(dirPath)) {
    std::cout << "Directory created: " << dirPath << "\n";
  } else {
    std::cout << "Failed to create directory!\n";
  }
}

void delete_item(const fs::path &target) {
  if (!fs::exists(target)) {
    std::cout << "Item does not exist!\n";
    return;
  }
  if (fs::is_directory(target)) {
    fs::remove_all(target);
  } else {
    fs::remove(target);
  }
  std::cout << "Deleted: " << target << "\n";
}


