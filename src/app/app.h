#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <list>
#include <memory>
#include <atomic>
#include <mutex>
#include <functional>

#include "file_intents.h"
#include "util/ctpl_stl.h"

namespace app 
{

// NOTE: foward declare
class AppFolder;


// Main app object which contains all folders
class App 
{
public:
    std::filesystem::path m_root;
    FilterRules m_cfg;
    std::string m_token;
    std::string m_credentials_filepath;

    std::list<std::shared_ptr<AppFolder>> m_folders;
    std::shared_ptr<AppFolder> m_current_folder;

    std::vector<std::string> m_app_errors;
    std::mutex m_app_errors_mutex;
    std::list<std::string> m_app_warnings;
    std::mutex m_app_warnings_mutex;
private:
    ctpl::thread_pool m_thread_pool;
    std::atomic<int> m_global_busy_count;
public:
    App(const char* config_filepath);
    void authenticate();
    void refresh_folders();
    int get_folder_busy_count() { return m_global_busy_count; }
    void queue_async_call(std::function<void (int)> call);
    void queue_app_error(const std::string& error);
    void queue_app_warning(const std::string& warning);
};

};
