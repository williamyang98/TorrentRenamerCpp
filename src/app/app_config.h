#pragma once

#include <string>
#include <vector>

namespace app {

struct AppConfig {
    std::string credentials_filepath;
    std::vector<std::string> whitelist_folders;
    std::vector<std::string> whitelist_filenames;
    std::vector<std::string> blacklist_extensions; 
    std::vector<std::string> whitelist_tags; 
};

AppConfig load_app_config_from_filepath(const char *filename);

}