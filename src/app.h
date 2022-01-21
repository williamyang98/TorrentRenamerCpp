#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <list>
#include <atomic>
#include <mutex>
#include <functional>
#include <memory>

#include "tvdb_api.h"
#include "se_regex.h"
#include "scanner.h"
#include "file_loading.h"
#include "ctpl_stl.h"

namespace app 
{

// App object for managing a folder
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
    TVDB_Cache m_cache;
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
    std::vector<SeriesInfo> m_search_result;
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

struct AppConfig {
    std::string credentials_filepath;
    std::string schema_filepath;
    std::vector<std::string> whitelist_folders;
    std::vector<std::string> whitelist_filenames;
    std::vector<std::string> blacklist_extensions; 
    std::vector<std::string> whitelist_tags; 
};

AppConfig load_app_config_from_filepath(const char *filename);

// Main app object which contains all folders
class App 
{
public:
    std::filesystem::path m_root;
    RenamingConfig m_cfg;
    std::string m_token;
    std::string m_credentials_filepath;

    std::list<std::shared_ptr<SeriesFolder>> m_folders;
    std::shared_ptr<SeriesFolder> m_current_folder;

    std::vector<std::string> m_app_errors;
    std::mutex m_app_errors_mutex;
private:
    ctpl::thread_pool m_thread_pool;
    std::atomic<int> m_global_busy_count;
public:
    App(const char *config_filepath);
    void authenticate();
    void refresh_folders();
    int get_folder_busy_count() { return m_global_busy_count; }
    void queue_async_call(std::function<void (int)> call);
    void queue_app_error(const std::string &error);
};

};