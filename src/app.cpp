#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <functional>
#include <ranges>
#include <memory>

#include <spdlog/spdlog.h>
#include <fmt/core.h>

#include "app.h"
#include "app_credentials_schema.h"
#include "tvdb_api.h"
#include "tvdb_api_schema.h"
#include "se_regex.h"
#include "scanner.h"
#include "file_loading.h"
#include "renamer.h"

#define EPISODES_CACHE_FN   "episodes.json"
#define SERIES_CACHE_FN     "series.json"

// include the windows libraries here because the macros interfere with our other headers
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#include <windows.h>
#include <shellapi.h>

namespace app 
{

namespace fs = std::filesystem;

class ScopedAtomic 
{
public:
    std::atomic<bool> &m_var;
    std::atomic<int> &m_busy_count;
    ScopedAtomic(std::atomic<bool> &var, std::atomic<int> &busy_count)
    : m_var(var), m_busy_count(busy_count) {
        m_var = true;
        m_busy_count++;
    }

    ~ScopedAtomic() {
        m_var = false;
        m_busy_count--;
    }
};

SeriesFolder::SeriesFolder(
    const fs::path &path, 
    RenamingConfig &cfg,
    std::atomic<int> &busy_count) 
: m_path(path), m_cfg(cfg), m_global_busy_count(busy_count) 
{
    m_is_info_cached = false;
    m_is_busy = false;
    m_status = SeriesFolder::Status::UNKNOWN;
    m_state = std::move(std::make_unique<ManagedFolder>());
}

// thread safe push of error to list
void SeriesFolder::push_error(const std::string &str) {
    std::scoped_lock lock(m_errors_mutex);
    m_errors.push_back(str);
}

// thread safe load series search results from tvdb with validation
bool SeriesFolder::load_search_series_from_tvdb(const char *name, const char *token) {
    if (m_is_busy) return false;

    auto scoped_hold = ScopedAtomic(m_is_busy, m_global_busy_count);

    try {
        auto search_opt = tvdb_api::search_series(name, token);
        if (!search_opt) {
            push_error(std::string("Failed to get search results from tvdb"));
            return false;
        }

        auto search_doc = std::move(search_opt.value());
        auto result = load_search_info(search_doc);
        {
            std::scoped_lock lock(m_search_mutex);
            m_search_result = std::move(result);
        }
    } catch (std::exception &ex) {
        push_error(std::string(ex.what()));
        return false;
    }
    return false;
}

// thread safe load series and episodes data from tvdb with validation, and store as cache
bool SeriesFolder::load_cache_from_tvdb(uint32_t id, const char *token) {
    if (m_is_busy) return false;
    auto scoped_hold = ScopedAtomic(m_is_busy, m_global_busy_count);

    try {
        auto series_opt = tvdb_api::get_series(id, token);
        if (!series_opt) {
            push_error(std::string("Failed to fetch series info from tvdb"));
            return false;
        }

        auto episodes_opt = tvdb_api::get_series_episodes(id, token);
        if (!episodes_opt) {
            push_error(std::string("Failed to fetch episodes info from tvdb"));
            return false;
        }

        auto series_doc = std::move(series_opt.value());
        auto episodes_doc = std::move(episodes_opt.value());
        auto series_cache = load_series_info(series_doc);
        auto episodes_cache = load_series_episodes_info(episodes_doc);
        // update cache
        {
            std::scoped_lock lock(m_cache_mutex);
            TVDB_Cache cache = { series_cache, episodes_cache };
            m_cache = std::move(cache);
            m_is_info_cached = true;
        }

        auto series_cache_path = fs::absolute(m_path / SERIES_CACHE_FN);
        if (!write_document_to_file(series_cache_path.string().c_str(), series_doc)) {
            std::scoped_lock lock(m_errors_mutex);
            push_error(std::string("Failed to write series cache file"));
            return false;
        }

        auto episodes_cache_path = fs::absolute(m_path / EPISODES_CACHE_FN);
        if (!write_document_to_file(episodes_cache_path.string().c_str(), episodes_doc)) {
            std::scoped_lock lock(m_errors_mutex);
            push_error(std::string("Failed to write series cache file"));
            return false;
        }
    } catch (std::exception &ex) {
        push_error(std::string(ex.what()));
        return false;
    }

    return true;
}

// thread safe load the cache from local file (series.json, episodes.json)
bool SeriesFolder::load_cache_from_file() {
    if (m_is_busy) return false;
    auto scoped_hold = ScopedAtomic(m_is_busy, m_global_busy_count);

    const fs::path series_cache_fn = m_path / SERIES_CACHE_FN;
    auto series_res = load_document_from_file(series_cache_fn.string().c_str());
    if (series_res.code != DocumentLoadCode::OK) {
        return false;
    }

    const fs::path episodes_cache_fn = m_path / EPISODES_CACHE_FN;
    auto episodes_res = load_document_from_file(episodes_cache_fn.string().c_str());
    if (episodes_res.code != DocumentLoadCode::OK) {
        return false;
    }

    auto series_doc = std::move(series_res.doc);
    auto episodes_doc = std::move(episodes_res.doc);

    if (!validate_document(series_doc, tvdb_api::SERIES_DATA_SCHEMA)) {
        push_error(std::string("Failed to validate series data"));
        return false;
    }

    if (!validate_document(episodes_doc, tvdb_api::EPISODES_DATA_SCHEMA)) {
        push_error(std::string("Failed to validate episodes data"));
        return false;
    }

    auto series_cache = load_series_info(series_doc);
    auto episodes_cache = load_series_episodes_info(episodes_doc);

    {
        std::scoped_lock lock(m_cache_mutex);
        TVDB_Cache cache = { series_cache, episodes_cache };
        m_cache = std::move(cache);
        m_is_info_cached = true;
    }
    return true;
}

// update folder diff after cache has been loaded
void SeriesFolder::update_state_from_cache() {
    if (m_is_busy) return;
    if (!m_is_info_cached && !load_cache_from_file()) {
        return;
    }

    auto scoped_hold = ScopedAtomic(m_is_busy, m_global_busy_count);
    auto intents = scan_directory(m_path, m_cache, m_cfg);

    auto new_state = std::make_unique<ManagedFolder>();
    for (auto &intent: intents) {
        new_state->AddIntent(intent);
    }

    auto &counts = new_state->GetActionCount();
    auto &conflict_table = new_state->GetConflicts();

    if (counts.deletes > 0) {
        m_status = Status::PENDING_DELETES;
    } else if (conflict_table.size() > 0) {
        m_status = Status::CONFLICTS;
    } else if ((counts.renames > 0) || (counts.ignores > 0)) {
        m_status = Status::PENDING_RENAME;
    } else if (counts.completes > 0) {
        m_status = Status::COMPLETED;
    } else {
        m_status = Status::EMPTY;
    }

    std::scoped_lock lock(m_state_mutex);
    m_state = std::move(new_state);
}

// thread safe execute all actions in folder diff
bool SeriesFolder::execute_actions() {
    if (m_is_busy) return false;
    auto scoped_hold = ScopedAtomic(m_is_busy, m_global_busy_count);

    auto intents = m_state->GetIntents() | std::views::values | std::views::transform([](auto &intent) {
        return std::reference_wrapper(intent.GetUnmanagedIntent());
    });

    std::scoped_lock lock(m_state_mutex);
    return rename_series_directory(m_path, intents);
}

// execute the shell command to open the folder or file
void SeriesFolder::open_folder(const std::string &path) {
    auto filepath = m_path / path;
    auto parent_dir = filepath.remove_filename();
    auto parent_dir_str = parent_dir.string();
    ShellExecuteA(NULL, "open", parent_dir_str.c_str(), NULL, NULL, SW_SHOW);
}

void SeriesFolder::open_file(const std::string &path) {
    auto filepath = m_path / path;
    auto filepath_str = filepath.string();
    ShellExecuteA(NULL, "open", filepath_str.c_str(), NULL, NULL, SW_SHOW);
}

App::App(const char *config_filepath)
: m_thread_pool(std::thread::hardware_concurrency()) 
{
    m_current_folder = nullptr;
    m_global_busy_count = 0;

    try {
        auto cfg = load_app_config_from_filepath(config_filepath);

        // setup our renaming config
        for (auto &v: cfg.blacklist_extensions) {
            m_cfg.blacklist_extensions.push_back(v);
        }
        for (auto &v: cfg.whitelist_folders) {
            m_cfg.whitelist_folders.push_back(v);
        }
        for (auto &v: cfg.whitelist_filenames) {
            m_cfg.whitelist_files.push_back(v);
        }
        for (auto &v: cfg.whitelist_tags) {
            m_cfg.whitelist_tags.push_back(v);
        }

        m_credentials_filepath = cfg.credentials_filepath;
        authenticate();
    } catch (std::exception &e) {
        queue_app_error(e.what());
    }
}

// get a new token which can be used for a few hours
void App::authenticate() {
    auto cred_fp = m_credentials_filepath.c_str();
    auto cred_res = load_document_from_file(cred_fp);
    if (cred_res.code != DocumentLoadCode::OK) {
        auto err = "Failed to load credentials file";
        spdlog::error(err);
        queue_app_error(err);
        return;
    }

    auto cred_doc = std::move(cred_res.doc);

    if (!validate_document(cred_doc, CREDENTIALS_SCHEMA)) {
        auto err = fmt::format("Credentials file is in the wrong format ({})", cred_fp);
        spdlog::error(err);
        queue_app_error(err);
        return;
    }

    {
        auto res = tvdb_api::login(
            cred_doc["credentials"]["apikey"].GetString(), 
            cred_doc["credentials"]["userkey"].GetString(),
            cred_doc["credentials"]["username"].GetString());
        
        if (!res) {
            spdlog::error("Failed to login");
            queue_app_error("Failed to login");
            return;
        }

        m_token = res.value();
    }
}

// create folder objects for each folder in the root directory
void App::refresh_folders() {
    if (m_global_busy_count > 0) {
        return;
    }

    m_folders.clear();
    m_current_folder = nullptr;

    try {
        for (auto &subdir: fs::directory_iterator(m_root)) {
            if (!subdir.is_directory()) {
                continue;
            }
            auto folder = std::make_shared<SeriesFolder>(subdir, m_cfg, m_global_busy_count);
            m_folders.push_back(std::move(folder));
        }
    } catch (std::exception &e) {
        queue_app_error(e.what());
    }
}

// push a call into the thread pool and queue any exceptions that occur
// we do this so our gui can get a list of exceptions and render them
void App::queue_async_call(std::function<void (int)> call) {
    m_thread_pool.push([call, this](int pid) {
        try {
            call(pid);
        } catch (std::exception &e) {
            queue_app_error(e.what());
        }
    });
}

// mutex protected addition of error
void App::queue_app_error(const std::string &error) {
    std::scoped_lock lock(m_app_errors_mutex);
    m_app_errors.push_back(error);
}

};