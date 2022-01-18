#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <filesystem>
#include <list>

#include "file_loading.h"
#include "folder_diff.h"

namespace app
{

// config for renaming
struct RenamingConfig {
    std::vector<std::string> blacklist_extensions;
    std::vector<std::string> whitelist_folders;
    std::vector<std::string> whitelist_files;
    std::vector<std::string> whitelist_tags;
};

// produce an initial folder diff given the config 
// this uses a cache for fast lookup for episode names
FolderDiff scan_directory(
    const std::filesystem::path &root, 
    const TVDB_Cache &cache,
    const RenamingConfig &cfg);

};