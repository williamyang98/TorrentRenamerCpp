#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

#include "app.h"
#include "tvdb_api.h"
#include "se_regex.h"
#include "renamer.h"
#include "file_loading.h"

#define EPISODES_CACHE_FN   "episodes.json"
#define SERIES_CACHE_FN     "series.json"

namespace app 
{

namespace fs = std::filesystem;

SeriesFolder::SeriesFolder(const fs::path &path, app_schema_t &schema, RenamingConfig &cfg) 
: m_path(path), m_schema(schema), m_cfg(cfg) {
    m_is_info_cached = false;
}

void SeriesFolder::refresh_cache(uint32_t id, const char *token) {
    auto series_opt = tvdb_api::get_series(id, token);
    if (!series_opt) {
        std::cerr << "Failed to get series info from tvdb" << std::endl;
        exit(1);
    }

    auto episodes_opt = tvdb_api::get_series_episodes(id, token);
    if (!episodes_opt) {
        std::cerr << "Failed to get episodes info from tvdb" << std::endl;
        exit(1);
    }

    rapidjson::Document series_doc = std::move(series_opt.value());
    rapidjson::Document episodes_doc = std::move(episodes_opt.value());

    validate_document(series_doc, m_schema.at(std::string(SCHEME_SERIES_KEY)), "Validating series");
    validate_document(episodes_doc, m_schema.at(std::string(SCHEME_EPISODES_KEY)), "Validating episodes");

    auto series_cache = load_series_info(series_doc);
    auto episodes_cache = load_series_episodes_info(episodes_doc);

    // update cache
    m_cache = { series_cache, episodes_cache };
    // m_cache.series = series_cache;
    // m_cache.episodes = episodes_cache;

    // TODO: write episodes data
    auto series_cache_path = fs::absolute(m_path / SERIES_CACHE_FN);
    write_document_to_file(series_cache_path.string().c_str(), series_doc);

    auto episodes_cache_path = fs::absolute(m_path / EPISODES_CACHE_FN);
    write_document_to_file(episodes_cache_path.string().c_str(), episodes_doc);

    m_is_info_cached = true;
}

void SeriesFolder::load_cache() {
    // TODO: detect if cache exists before loading 
    const fs::path series_cache_fn = m_path / SERIES_CACHE_FN;
    auto series_doc = load_document_from_file(series_cache_fn.string().c_str());

    const fs::path episodes_cache_fn = m_path / EPISODES_CACHE_FN;
    auto episodes_doc = load_document_from_file(episodes_cache_fn.string().c_str());

    validate_document(series_doc, m_schema.at(std::string(SCHEME_SERIES_KEY)), "Validating series");
    validate_document(episodes_doc, m_schema.at(std::string(SCHEME_EPISODES_KEY)), "Validating episodes");

    auto series_cache = load_series_info(series_doc);
    auto episodes_cache = load_series_episodes_info(episodes_doc);

    m_cache = { series_cache, episodes_cache };
    
    m_is_info_cached = true;
}

void SeriesFolder::update_state() {
    if (!m_is_info_cached) {
        load_cache();
    }
    m_state = scan_directory(m_path, m_cache, m_cfg);
}

App::App() {
    m_current_folder = -1;

    m_cfg.blacklist_extensions.push_back(".nfo");
    m_cfg.blacklist_extensions.push_back(".ext");
    m_cfg.special_folders.push_back("Extras");

    m_schema = load_schema_from_file("schema.json");

    authenticate();
}

void App::authenticate() {
    auto cred_doc = load_document_from_file("credentials.json");
    validate_document(cred_doc, m_schema.at(std::string(SCHEME_CRED_KEY)), "Valiating credentials");
    {
        auto res = tvdb_api::login(
            cred_doc["credentials"]["apikey"].GetString(), 
            cred_doc["credentials"]["userkey"].GetString(),
            cred_doc["credentials"]["username"].GetString());
        
        if (!res) {
            std::cerr << "Failed to login" << std::endl;
            exit(1);
        }

        m_token = res.value();
    }
}

void App::refresh_folders() {
    m_folders.clear();
    m_current_folder = -1;

    for (auto &subdir: fs::directory_iterator(m_root)) {
        if (!subdir.is_directory()) {
            continue;
        }
        m_folders.emplace_back(subdir, m_schema, m_cfg);
    }
}

void App::select_folder(int i) {
    m_current_folder = i;

    if (m_current_folder == -1) {
        return;
    }

    assert(m_current_folder >= 0 || m_current_folder < (int)m_folders.size());

    auto &folder = m_folders[m_current_folder];
    folder.update_state();
}

SeriesFolder* App::get_selected_folder() {
    if (m_current_folder == -1) {
        return nullptr;
    }

    assert(m_current_folder >= 0 || m_current_folder < (int)m_folders.size());
    auto &folder = m_folders[m_current_folder];
    return &folder;
}

};