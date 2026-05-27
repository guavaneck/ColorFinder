#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>

// --- Config keys ---
// All keys available in the config file.
static constexpr const char *KEY_COMPILE_COMMAND = "compile_command";

// --- Config API ---

// Loads config from disk. If the file doesn't exist, creates it with defaults.
// Returns a map of key -> value.
std::unordered_map<std::string, std::string> config_load();

// Returns a value from the map, or the given fallback if the key is missing.
std::string config_get(const std::unordered_map<std::string, std::string> &cfg,
                       const std::string &key,
                       const std::string &fallback = "");
