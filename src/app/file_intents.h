#pragma once

#include <stdint.h>
#include <filesystem>
#include <vector>
#include <string>
#include <optional>
#include "tvdb_api/tvdb_models.h"

namespace app 
{

// TODO: The metadata for different actions is stored in the same struct
//        We could use a variant here to save memory possibly
struct FileIntent {
    enum class Action: uint8_t {
        RENAME      = 1<<0,
        COMPLETE    = 1<<1,
        IGNORE      = 1<<2,
        DELETE      = 1<<3,
        WHITELIST   = 1<<4,
    };

    std::string src;
    std::string dest; 
    Action action = Action::IGNORE;
    bool is_conflict = false; 
    bool is_active = false;   
    std::optional<tvdb_api::EpisodeKey> descriptor = std::nullopt;
};

struct FilterRules {
    std::vector<std::string> blacklist_extensions;  // delete these extensions
    std::vector<std::string> whitelist_folders;     // whitelist files in these folders
    std::vector<std::string> whitelist_files;       // whitelist files with these names
    std::vector<std::string> whitelist_tags;        // keep these tags in filename
};

FileIntent get_file_intent(
    const std::string& relative_path, 
    const FilterRules& rules, 
    const tvdb_api::TVDB_Cache& api_cache);

std::vector<FileIntent> get_directory_file_intents(
    const std::filesystem::path& root, 
    const FilterRules& rules, 
    const tvdb_api::TVDB_Cache& api_cache);

// THROWS: If there is an IO exception it will propagate upwards
void execute_file_intent(const std::filesystem::path& root, const FileIntent& intent);

};
