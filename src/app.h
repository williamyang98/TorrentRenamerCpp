#pragma once

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <list>
#include <atomic>
#include <mutex>

#include "tvdb_api.h"
#include "se_regex.h"
#include "scanner.h"
#include "file_loading.h"
#include "ctpl_stl.h"

namespace app 
{


class SeriesFolder 
{
public:
    enum Status {
        UNKNOWN         = 1<<0, 
        PENDING_DELETES = 1<<1, 
        CONFLICTS       = 1<<2, 
        PENDING_RENAME  = 1<<3, 
        COMPLETED       = 1<<4, 
        EMPTY           = 1<<5,
    };
public:
    const std::filesystem::path m_path;
    app_schema_t &m_schema;
    RenamingConfig &m_cfg;

    TVDB_Cache m_cache;
    bool m_is_info_cached;
    std::mutex m_cache_mutex;

    SeriesState m_state;
    std::atomic<Status> m_status;
    std::mutex m_state_mutex;

    std::list<std::string> m_errors;
    std::mutex m_errors_mutex;

    std::vector<SeriesInfo> m_search_result;
    std::mutex m_search_mutex;

    std::atomic<bool> m_is_busy;
    std::atomic<int> &m_global_busy_count;
public:
    SeriesFolder(
        const std::filesystem::path &path, 
        app_schema_t &schema, 
        RenamingConfig &cfg,
        std::atomic<int> &busy_count);
    bool refresh_cache(uint32_t id, const char *token);
    bool load_cache();
    void update_state();
    bool search_series(const char *name, const char *token);
    bool execute_actions();

private:
    void push_error(std::string &str);
};

class App 
{
public:
    std::string m_token;
    std::list<SeriesFolder> m_folders;
    std::filesystem::path m_root;
    SeriesFolder *m_current_folder;

    RenamingConfig m_cfg;
    app_schema_t m_schema;

    ctpl::thread_pool m_thread_pool;

    std::atomic<int> m_global_busy_count;

public:
    App();
    void authenticate();
    void refresh_folders();
};

};