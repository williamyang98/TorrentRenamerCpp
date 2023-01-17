#include "app.h"

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <functional>
#include <memory>

#include <spdlog/spdlog.h>
#include <fmt/core.h>

#include "app_config.h"
#include "app_credentials.h"
#include "app_folder.h"

#include "tvdb_api/tvdb_api.h"
#include "tvdb_api/tvdb_json.h"

#include "util/file_loading.h"

namespace app 
{

namespace fs = std::filesystem;

App::App(const char* config_filepath)
{
    const int num_threads = 1000;
    m_thread_pool.resize(num_threads);

    m_current_folder = nullptr;
    m_global_busy_count = 0;

    auto cfg_opt = load_app_config_from_filepath(config_filepath);
    if (!cfg_opt) {
        queue_app_error(cfg_opt.error());
        return;
    }

    auto& cfg = cfg_opt.value();
    // setup our renaming config
    for (auto& v: cfg.blacklist_extensions) {
        m_cfg.blacklist_extensions.push_back(v);
    }
    for (auto& v: cfg.whitelist_folders) {
        m_cfg.whitelist_folders.push_back(v);
    }
    for (auto& v: cfg.whitelist_filenames) {
        m_cfg.whitelist_files.push_back(v);
    }
    for (auto& v: cfg.whitelist_tags) {
        m_cfg.whitelist_tags.push_back(v);
    }

    m_credentials_filepath = cfg.credentials_filepath;
    authenticate();
}

// get a new token which can be used for a few hours
void App::authenticate() {
    auto filepath = m_credentials_filepath.c_str();
    
    auto credentials_opt = load_credentials_from_filepath(filepath);
    if (!credentials_opt) {
        queue_app_error(credentials_opt.error());
        return;
    }

    auto& credentials = credentials_opt.value();
    auto token_opt = tvdb_api::login(
        credentials.api_key.c_str(), 
        credentials.user_key.c_str(),
        credentials.username.c_str()
    );

    if (!token_opt) {
        queue_app_warning(token_opt.error());
        return;
    }

    auto& token = token_opt.value();
    m_token = std::move(token);
}

// create folder objects for each folder in the root directory
void App::refresh_folders() {
    if (m_global_busy_count > 0) {
        return;
    }

    m_folders.clear();
    m_current_folder = nullptr;

    // NOTE: An I/O error will throw an exception
    try {
        for (auto& subdir: fs::directory_iterator(m_root)) {
            if (!subdir.is_directory()) {
                continue;
            }
            auto folder = std::make_shared<AppFolder>(subdir, m_cfg, m_global_busy_count);
            m_folders.push_back(folder);
        }
    } catch (std::exception& e) {
        queue_app_error(e.what());
    }
}

// Asynchronously run a callable in our thread pool to prevent blocking the UI thread
void App::queue_async_call(std::function<void (int)> call) {
    m_thread_pool.push([call, this](int pid) {
        // NOTE: We may perform IO here in the thread pool which can raise exceptions
        try {
            call(pid);
        } catch (std::exception& e) {
            queue_app_error(e.what());
        }
    });
}

// mutex protected addition of error
void App::queue_app_error(const std::string& error) {
    auto lock = std::scoped_lock(m_app_errors_mutex);
    m_app_errors.push_back(error);
}

void App::queue_app_warning(const std::string& warning) {
    auto lock = std::scoped_lock(m_app_warnings_mutex);
    m_app_warnings.push_back(warning);
}

};
