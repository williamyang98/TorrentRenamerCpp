#pragma once

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

#include "tvdb_api.h"
#include "se_regex.h"
#include "renamer.h"
#include "file_loading.h"

namespace app 
{

class SeriesFolder 
{
public:
    const std::filesystem::path m_path;
    app_schema_t &m_schema;
    RenamingConfig &m_cfg;
    TVDB_Cache m_cache;
    bool m_is_info_cached;

    SeriesState m_state;

public:
    SeriesFolder(const std::filesystem::path &path, app_schema_t &schema, RenamingConfig &cfg);
    void refresh_cache(uint32_t id, const char *token);
    void load_cache();
    void update_state();
};

class App 
{
public:
    std::string m_token;
    std::vector<SeriesFolder> m_folders;
    std::filesystem::path m_root;
    int m_current_folder;

    RenamingConfig m_cfg;
    app_schema_t m_schema;

public:
    App();
    void authenticate();
    void refresh_folders();
    void select_folder(int i); 
    SeriesFolder* get_selected_folder();
};

};