#include "app.h"

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

#include "app_credentials_schema.h"
#include "app_config.h"
#include "app_folder.h"

#include "tvdb_api/tvdb_api.h"
#include "tvdb_api/tvdb_json.h"

#include "util/file_loading.h"

namespace app 
{

namespace fs = std::filesystem;

App::App(const char *config_filepath)
{
    const int num_threads = 1000;
m_thread_pool.resize(num_threads);

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
    auto cred_res = util::load_document_from_file(cred_fp);
    if (cred_res.code != util::DocumentLoadCode::OK) {
        auto err = "Failed to load credentials file";
        spdlog::error(err);
        queue_app_error(err);
        return;
    }

    auto &cred_doc = cred_res.doc;

    if (!util::validate_document(cred_doc, CREDENTIALS_SCHEMA)) {
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
            /* queue_app_error("Failed to login"); */
            queue_app_warning("Failed to login");
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
    auto lock = std::scoped_lock(m_app_errors_mutex);
    m_app_errors.push_back(error);
}

void App::queue_app_warning(const std::string &warning) {
    auto lock = std::scoped_lock(m_app_warnings_mutex);
    m_app_warnings.push_back(warning);
}

};
