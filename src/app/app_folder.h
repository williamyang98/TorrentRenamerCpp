#pragma once

#include <string>
#include <filesystem>
#include <memory>
#include <mutex>
#include <atomic>

#include "tvdb_api/tvdb_models.h"
#include "renaming/managed_folder.h"
#include "renaming/renaming_config.h"

namespace app {

// contains the necessary data structures to execute actions on a managed folder
// primarily contains:
// - Managed folder object
// - TVDB_Cache for that series
// - Search results of a tvdb api search
// - The renaming config that is loaded from config file
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
    RenamingConfig &m_cfg;

    // keep a mutex on members which are used in rendering and undergo mutation during actions
    // cache of tvdb data
    tvdb_api::TVDB_Cache m_cache;
    bool m_is_info_cached;
    std::mutex m_cache_mutex;

    // set of current actions
    std::unique_ptr<ManagedFolder> m_state;
    std::atomic<Status> m_status;
    std::mutex m_state_mutex;

    // errors accumulated from operations
    std::list<std::string> m_errors;
    std::mutex m_errors_mutex;

    // store the search result for a tvdb search query
    std::vector<tvdb_api::SeriesInfo> m_search_result;
    std::mutex m_search_mutex;

    // keep track of whether we are busy 
    std::atomic<bool> m_is_busy;
    // use this to keep count of the global count of busy folders
    std::atomic<int> &m_global_busy_count;
public:
    SeriesFolder(
        const std::filesystem::path &path, 
        RenamingConfig &cfg,
        std::atomic<int> &busy_count);

    bool load_cache_from_tvdb(uint32_t id, const char *token);
    bool load_cache_from_file();
    void update_state_from_cache();
    bool load_search_series_from_tvdb(const char *name, const char *token);
    bool execute_actions();
    void open_folder(const std::string &path);
    void open_file(const std::string &path);

private:
    void push_error(const std::string &str);
};

}