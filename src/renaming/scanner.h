#pragma once

#include <filesystem>

#include "file_intent.h"
#include "renaming_config.h"
#include "tvdb_api/tvdb_models.h"


namespace app
{

// produce an initial folder diff given the config 
// this uses a cache for fast lookup for episode names
std::vector<FileIntent> scan_directory(
    const std::filesystem::path &root, 
    const tvdb_api::TVDB_Cache &cache,
    const RenamingConfig &cfg);

};