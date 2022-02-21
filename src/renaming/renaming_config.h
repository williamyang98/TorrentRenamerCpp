#pragma once

#include <vector>
#include <string>

namespace app {

// config for renaming
struct RenamingConfig {
    std::vector<std::string> blacklist_extensions;
    std::vector<std::string> whitelist_folders;
    std::vector<std::string> whitelist_files;
    std::vector<std::string> whitelist_tags;
};

}