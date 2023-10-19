#include "app_folder.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <spdlog/spdlog.h>
#include <fmt/core.h>

#include "app_folder_state.h"
#include "app_folder_bookmarks_json.h"
#include "file_intents.h"
#include "tvdb_api/tvdb_api.h"
#include "tvdb_api/tvdb_models.h"
#include "tvdb_api/tvdb_json.h"
#include "util/file_loading.h"
#include "os_dep.h"

constexpr const char* EPISODES_CACHE_FN = "episodes.json";
constexpr const char* SERIES_CACHE_FN = "series.json";
constexpr const char* BOOKMARKS_FN = "bookmarks.json";


namespace app 
{

namespace fs = std::filesystem;

// TODO: We force all operations on a folder to be atomic
//       This means operations coming in from multiple threads will block each other
//       This is because there is alot of inter data dependencies in our data structure 
//       However it may be possible to have certain operations occur in parallel with other ones
class BusyLock 
{
public:
    std::unique_lock<std::mutex> m_is_busy_lock;
    bool& m_is_busy;
    std::atomic<int>& m_busy_count;
    BusyLock(std::mutex& is_busy_mutex, bool& is_busy, std::atomic<int>& busy_count)
    : m_is_busy_lock(is_busy_mutex), m_is_busy(is_busy), m_busy_count(busy_count) 
    {
        m_is_busy = true;
        m_busy_count++;
    }
    ~BusyLock() {
        m_is_busy = false;
        m_busy_count--;
    }
    BusyLock(const BusyLock&) = delete;
    BusyLock(BusyLock&&) = delete;
    BusyLock& operator=(const BusyLock&) = delete;
    BusyLock& operator=(BusyLock&&) = delete;
};

AppFolder::AppFolder(
    const fs::path& path, 
    FilterRules& cfg,
    std::atomic<int>& busy_count) 
: m_path(path), m_cfg(cfg), m_global_busy_count(busy_count) 
{
    m_is_info_cached = false;
    m_status = AppFolder::Status::UNKNOWN;
    m_state = std::make_unique<AppFolderState>();
    m_is_busy = false;
}

void AppFolder::push_error(const std::string& str) {
    std::scoped_lock lock(m_errors_mutex);
    m_errors.push_back(str);
}

bool AppFolder::load_search_series_from_tvdb(const char* name, const char* token) {
    auto busy_lock = BusyLock(m_is_busy_mutex, m_is_busy, m_global_busy_count);

    auto search_opt = tvdb_api::search_series(name, token);
    if (!search_opt) {
        push_error(search_opt.error());
        return false;
    }

    auto& search_doc = search_opt.value();
    auto result = tvdb_api::load_search_info(search_doc);
    if (!result) {
        push_error(result.error());
        return false;
    }

    std::scoped_lock lock(m_search_mutex);
    m_search_result = std::move(result.value());
    return true;
}

bool AppFolder::load_cache_from_tvdb(uint32_t id, const char* token) {
    auto busy_lock = BusyLock(m_is_busy_mutex, m_is_busy, m_global_busy_count);

    // attempt to fetch data from tvdb api
    auto series_opt = tvdb_api::get_series(id, token);
    if (!series_opt) {
        push_error(series_opt.error());
        return false;
    }

    auto episodes_opt = tvdb_api::get_series_episodes(id, token);
    if (!episodes_opt) {
        push_error(episodes_opt.error());
        return false;
    }

    auto& series_doc = series_opt.value();
    auto& episodes_doc = episodes_opt.value();

    // update cache
    {
        auto series_cache_opt = tvdb_api::load_series_info(series_doc);
        if (!series_cache_opt) {
            push_error(series_cache_opt.error());
            return false;
        }

        auto episodes_cache_opt = tvdb_api::load_series_episodes_info(episodes_doc);
        if (!episodes_cache_opt) {
            push_error(episodes_cache_opt.error());
            return false;
        }

        auto& series_cache = series_cache_opt.value();
        auto& episodes_cache = episodes_cache_opt.value();
        std::scoped_lock lock(m_cache_mutex);
        auto cache = tvdb_api::TVDB_Cache{std::move(series_cache), std::move(episodes_cache)};
        m_cache = std::move(cache);
        m_is_info_cached = true;
    }

    // write cache to series folder
    const auto series_cache_path = fs::absolute(m_path / SERIES_CACHE_FN);
    const auto episodes_cache_path = fs::absolute(m_path / EPISODES_CACHE_FN);
    if (!util::write_document_to_file(series_cache_path.string().c_str(), series_doc)) {
        push_error("Failed to write series cache file");
        return false;
    }

    if (!util::write_document_to_file(episodes_cache_path.string().c_str(), episodes_doc)) {
        push_error("Failed to write episodes cache file");
        return false;
    }

    return true;
}

bool AppFolder::load_cache_from_file() {
    auto busy_lock = BusyLock(m_is_busy_mutex, m_is_busy, m_global_busy_count);

    // load cache from series folder
    // NOTE: We expect that this may not load since the cache hasn't been downloaded from api
    const fs::path series_cache_fn = m_path / SERIES_CACHE_FN;
    const fs::path episodes_cache_fn = m_path / EPISODES_CACHE_FN;
    auto series_res = util::load_document_from_file(series_cache_fn.string().c_str());
    if (series_res.code != util::DocumentLoadCode::OK) {
        return false;
    }

    auto episodes_res = util::load_document_from_file(episodes_cache_fn.string().c_str());
    if (episodes_res.code != util::DocumentLoadCode::OK) {
        return false;
    }

    // attempt to read in json data
    auto& series_doc = series_res.doc;
    auto& episodes_doc = episodes_res.doc;
    
    auto series_cache_opt = tvdb_api::load_series_info(series_doc);
    if (!series_cache_opt) {
        push_error("Failed to validate series data");
        return false;
    }

    auto episodes_cache_opt = tvdb_api::load_series_episodes_info(episodes_doc);
    if (!episodes_cache_opt) {
        push_error("Failed to validate episodes data");
        return false;
    }

    auto& series_cache = series_cache_opt.value();
    auto& episodes_cache = episodes_cache_opt.value();
    {
        auto lock = std::scoped_lock(m_cache_mutex);
        auto cache = tvdb_api::TVDB_Cache{std::move(series_cache), std::move(episodes_cache)};
        m_cache = std::move(cache);
        m_is_info_cached = true;
    }
    return true;
}

// update folder diff after cache has been loaded
bool AppFolder::update_state_from_cache() {
    if (!m_is_info_cached && !load_cache_from_file()) {
        return false;
    }

    auto busy_lock = BusyLock(m_is_busy_mutex, m_is_busy, m_global_busy_count);

    auto intents = get_directory_file_intents(m_path, m_cfg, m_cache);
    auto new_state = std::make_unique<AppFolderState>();
    for (auto& intent: intents) {
        new_state->AddIntent(std::move(intent));
    }

    auto& counts = new_state->GetActionCount();
    auto& conflict_table = new_state->GetConflicts();

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
    return true;
}

bool AppFolder::load_bookmarks_from_file() {
    auto busy_lock = BusyLock(m_is_busy_mutex, m_is_busy, m_global_busy_count);

    const fs::path bookmarks_fn = m_path / BOOKMARKS_FN;
    auto res = util::load_document_from_file(bookmarks_fn.string().c_str());
    if (res.code != util::DocumentLoadCode::OK) {
        return false;
    }

    auto& doc = res.doc;
    auto bookmarks_opt = load_app_folder_bookmarks(doc);
    if (!bookmarks_opt) {
        push_error(bookmarks_opt.error());
        return false;
    }

    auto bookmarks = bookmarks_opt.value();
    {
        auto lock = std::scoped_lock(m_bookmarks_mutex);
        m_bookmarks = std::move(bookmarks);
    }
    return true;
}

bool AppFolder::save_bookmarks_to_file() {
    auto busy_lock = BusyLock(m_is_busy_mutex, m_is_busy, m_global_busy_count);

    const fs::path bookmarks_fn = m_path / BOOKMARKS_FN;

    std::string json_str;
    {
        auto lock = std::scoped_lock(m_bookmarks_mutex);
        json_str = json_stringify_app_folder_bookmarks(m_bookmarks);
    }

    std::ofstream file(bookmarks_fn);
    if (!file.is_open()) {
        push_error("Failed to save bookmarks");
        return false;
    }
    file << json_str << std::endl;
    return true;    
}

int AppFolder::execute_actions() {
    auto busy_lock = BusyLock(m_is_busy_mutex, m_is_busy, m_global_busy_count);

    auto lock = std::scoped_lock(m_state_mutex);
    auto& intents = m_state->GetIntents();
    int total_errors = 0;

    // Remove deletes first to remove filepath conflicts where possible
    for (auto& [_, file_state]: intents) {
        auto& intent = file_state.GetIntent();
        if (intent.action == FileIntent::Action::DELETE) {
            try {
                execute_file_intent(m_path, intent);
            } catch (std::exception& e) {
                push_error(e.what());
                total_errors++;
            }
        }
    }

    // Execute changes after files have been removed
    for (auto& [_, file_state]: intents) {
        auto& intent = file_state.GetIntent();
        if (intent.action != FileIntent::Action::DELETE) {
            try {
                execute_file_intent(m_path, intent);
            } catch (std::exception& e) {
                push_error(e.what());
                total_errors++;
            }
        }
    }

    return total_errors;
}

// execute the shell command to open the folder or file
void AppFolder::open_folder(const std::string& path) {
    auto filepath = m_path / path;
    auto parent_dir = filepath.remove_filename();
    auto parent_dir_str = parent_dir.string();
    os_dep::open_folder(parent_dir_str);
}

void AppFolder::open_file(const std::string& path) {
    auto filepath = m_path / path;
    auto filepath_str = filepath.string();
    os_dep::open_file(filepath_str);
}

}
