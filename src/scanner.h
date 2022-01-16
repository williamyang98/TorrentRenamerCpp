#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

#include "file_loading.h"

namespace app
{

// pending rename
// user is free to modify dest
struct PendingFile {
    std::string target;
    std::string dest;
    bool active;
};

// conflict files
typedef std::unordered_map<std::string, std::vector<PendingFile>> ConflictFiles;

// delete a file
struct DeleteFile {
    std::string filename;
    bool active;
};

// stores all pending diffs made to a series folder
struct SeriesState {
    std::vector<std::string> completed;
    std::vector<std::string> ignores;
    std::vector<DeleteFile> deletes;
    ConflictFiles conflicts;
    std::vector<PendingFile> pending;
};

// config for renaming
struct RenamingConfig {
    std::vector<std::string> blacklist_extensions;
    std::vector<std::string> special_folders;
};

SeriesState scan_directory(
    const std::filesystem::path &root, 
    const TVDB_Cache &cache,
    const RenamingConfig &cfg);

};