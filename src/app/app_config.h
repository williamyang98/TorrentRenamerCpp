#pragma once

#include <string>
#include <vector>
#include "util/expected.hpp"

namespace app {

struct AppConfig {
    std::string credentials_filepath;
    std::vector<std::string> whitelist_folders;
    std::vector<std::string> whitelist_filenames;
    std::vector<std::string> blacklist_extensions; 
    std::vector<std::string> whitelist_tags; 
};

tl::expected<AppConfig, std::string> load_app_config_from_filepath(const char* filename);

}