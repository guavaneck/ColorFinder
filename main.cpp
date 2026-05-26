#include <iostream>
#include <filesystem>
#include <string>
#include "operations.h"

namespace fs = std::filesystem;

int main() {
  fs::path current = fs::current_path();
  std::string command, arg;

  std::cout << "Simple File Manager\n";
  std::cout << "Commands: list, cd <dir>, mkdir <name>, mkfile <name>, delete <name>, exit\n\n";

  while (true) {
    std::cout << current.string() << " > ";
    std::cin >> command;

    if (command == "exit") {
      break;
    } else if (command == "list") {
      list_directory(current);
    } else if (command == "cd") {
      std::cin >> arg;
      change_directory(current, arg);
    } else if (command == "mkdir") {
      std::cin >> arg;
      make_directory(current / arg);
    } else if (command == "mkfile") {
      std::cin >> arg;
      create_file(current / arg);
    } else if (command == "delete") {
      std::cin >> arg;
      delete_item(current / arg);
    } else {
      std::cout << "Unknown command: " << command << "\n";
    }
  }

  return 0;
}
