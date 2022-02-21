#include "app_folder.h"

#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <ranges>

#include <spdlog/spdlog.h>
#include <fmt/core.h>

#include "app_credentials_schema.h"

#include "tvdb_api/tvdb_api.h"
#include "tvdb_api/tvdb_api_schema.h"
#include "tvdb_api/tvdb_models.h"
#include "tvdb_api/tvdb_json.h"

#include "renaming/managed_folder.h"
#include "renaming/file_intent.h"
#include "renaming/se_regex.h"
#include "renaming/renamer.h"
#include "renaming/scanner.h"

#include "util/file_loading.h"

#define EPISODES_CACHE_FN   "episodes.json"
#define SERIES_CACHE_FN     "series.json"

// for opening folder dialog
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
        auto result = tvdb_api::load_search_info(search_doc);
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
        auto series_cache = tvdb_api::load_series_info(series_doc);
        auto episodes_cache = tvdb_api::load_series_episodes_info(episodes_doc);
        // update cache
        {
            std::scoped_lock lock(m_cache_mutex);
            tvdb_api::TVDB_Cache cache = { series_cache, episodes_cache };
            m_cache = std::move(cache);
            m_is_info_cached = true;
        }

        auto series_cache_path = fs::absolute(m_path / SERIES_CACHE_FN);
        if (!util::write_document_to_file(series_cache_path.string().c_str(), series_doc)) {
            auto lock = std::scoped_lock(m_errors_mutex);
            push_error(std::string("Failed to write series cache file"));
            return false;
        }

        auto episodes_cache_path = fs::absolute(m_path / EPISODES_CACHE_FN);
        if (!util::write_document_to_file(episodes_cache_path.string().c_str(), episodes_doc)) {
            auto lock = std::scoped_lock(m_errors_mutex);
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
    auto series_res = util::load_document_from_file(series_cache_fn.string().c_str());
    if (series_res.code != util::DocumentLoadCode::OK) {
        return false;
    }

    const fs::path episodes_cache_fn = m_path / EPISODES_CACHE_FN;
    auto episodes_res = util::load_document_from_file(episodes_cache_fn.string().c_str());
    if (episodes_res.code != util::DocumentLoadCode::OK) {
        return false;
    }

    auto series_doc = std::move(series_res.doc);
    auto episodes_doc = std::move(episodes_res.doc);

    if (!util::validate_document(series_doc, tvdb_api::SERIES_DATA_SCHEMA)) {
        push_error(std::string("Failed to validate series data"));
        return false;
    }

    if (!util::validate_document(episodes_doc, tvdb_api::EPISODES_DATA_SCHEMA)) {
        push_error(std::string("Failed to validate episodes data"));
        return false;
    }

    auto series_cache = tvdb_api::load_series_info(series_doc);
    auto episodes_cache = tvdb_api::load_series_episodes_info(episodes_doc);

    {
        auto lock = std::scoped_lock(m_cache_mutex);
        tvdb_api::TVDB_Cache cache = { series_cache, episodes_cache };
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

    auto lock = std::scoped_lock(m_state_mutex);
    m_state = std::move(new_state);
}

// thread safe execute all actions in folder diff
bool SeriesFolder::execute_actions() {
    if (m_is_busy) return false;
    auto scoped_hold = ScopedAtomic(m_is_busy, m_global_busy_count);

    auto intents = m_state->GetIntents() | std::views::values | std::views::transform([](auto &intent) {
        return std::reference_wrapper(intent.GetUnmanagedIntent());
    });

    auto lock = std::scoped_lock(m_state_mutex);
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

}