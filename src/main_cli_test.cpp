#include <iostream>
#include <string>
#include <filesystem>
#include <optional>

#include "app/app_credentials.h"
#include "app/app_config.h"
#include "app/app_folder_state.h"
#include "app/file_intents.h"
#include "tvdb_api/tvdb_api.h"
#include "tvdb_api/tvdb_json.h"
#include "util/file_loading.h"
#include "util/console_colours.h"

namespace fs = std::filesystem;

std::optional<rapidjson::Document> assert_document_load(util::DocumentLoadResult res, const char* message);
std::string fetch_api_token();
std::optional<tvdb_api::TVDB_Cache> load_cache_from_directory(fs::path root);
std::optional<tvdb_api::TVDB_Cache> load_cache_from_api(fs::path root, const std::string& token);
void scan_directory(const fs::path &subdir, const tvdb_api::TVDB_Cache& tvdb_cache, const app::FilterRules& cfg);

// A headless scanner that goes through a directory of TV series 
int main(int argc, char** argv) {
    // Argument parser
    if (argc <= 1) {
        std::cout << "Usage: " << argv[0] << "(directory_path) [--use-api]" << std::endl;
        return 1;
    }

    const auto root = fs::path(argv[1]);
    bool is_load_api = false;
    if (argc >= 3) {
        const auto* flag = argv[2];
        if (strncmp(flag, "--use-api", 10) == 0) {
            is_load_api = true;
            std::cout << "Using tvdb api" << std::endl;
        }
    }

    // Load filter rules from application configuration file
    auto app_config_opt = app::load_app_config_from_filepath("res/app_config.json");
    if (!app_config_opt) {
        std::cerr << app_config_opt.error() << std::endl;
        exit(1);
    }
    auto& app_config = app_config_opt.value();
    auto filter_rules = app::FilterRules();
    filter_rules.blacklist_extensions   = std::move(app_config.blacklist_extensions);
    filter_rules.whitelist_files        = std::move(app_config.whitelist_filenames);
    filter_rules.whitelist_folders      = std::move(app_config.whitelist_folders);
    filter_rules.whitelist_tags         = std::move(app_config.whitelist_tags);

    if (!is_load_api) {
        // series and episodes data is from local cache
        for (auto& subdir: fs::directory_iterator(root)) {
            if (!subdir.is_directory()) {
                continue;
            }
            const auto cache_opt = load_cache_from_directory(subdir);
            if (cache_opt) {
                auto& cache = cache_opt.value();
                scan_directory(subdir, cache, filter_rules);
            }
        }
    } else {
        // episodes data is from api
        // series data is from local cache
        const auto token = fetch_api_token();
        for (auto &subdir: fs::directory_iterator(root)) {
            if (!subdir.is_directory()) {
                continue;
            }
            const auto cache_opt = load_cache_from_api(subdir, token);
            if (cache_opt) {
                auto& cache = cache_opt.value();
                scan_directory(subdir, cache, filter_rules);
            }
        }
    }

    return 0;
}

std::optional<rapidjson::Document> assert_document_load(util::DocumentLoadResult res, const char* message) {
    if (res.code != util::DocumentLoadCode::OK) {
        std::cerr << "Failed to load document (" << message << ")" << std::endl;
        return {};
    }
    return std::move(res.doc);
}

void scan_directory(const fs::path &subdir, const tvdb_api::TVDB_Cache& tvdb_cache, const app::FilterRules& cfg) {
    std::cout << "Scanning directory: " << subdir << std::endl;

    auto folder = app::AppFolderState();
    auto intents = app::get_directory_file_intents(subdir, cfg, tvdb_cache);
    for (auto& intent: intents) {
        folder.AddIntent(std::move(intent));
    }

    auto &counts = folder.GetActionCount();
    auto &conflicts = folder.GetConflicts();
    
    std::cout 
        << "completed=" << counts.completes << '\n'
        << "renames=" << counts.renames << '\n'
        << "deletes=" << counts.deletes << '\n'
        << "ignores=" << counts.ignores << '\n'
        << "whitelist=" << counts.whitelists << '\n'
        << "conflicts=" << conflicts.size() << std::endl;

    for (const auto &[key, intent]: folder.GetIntents()) {
        if (intent.GetAction() != app::FileIntent::Action::COMPLETE) continue;
        std::cout << FGRN("[C] ") << intent.GetSrc() << std::endl;
    }

    for (const auto &[key, intent]: folder.GetIntents()) {
        if (intent.GetAction() != app::FileIntent::Action::RENAME) continue;
        std::cout << FCYN("[R] ") << intent.GetSrc() << " ==> " << intent.GetDest() << std::endl;
    }

    for (const auto &[key, intent]: folder.GetIntents()) {
        if (intent.GetAction() != app::FileIntent::Action::DELETE) continue;
        std::cout << FYEL("[D] ") << intent.GetSrc() << std::endl;
    }

    for (const auto &[key, intent]: folder.GetIntents()) {
        if (intent.GetAction() != app::FileIntent::Action::IGNORE) continue;
        std::cout << FMAG("[I] ") << intent.GetSrc() << std::endl;
    }

    for (const auto &[key, intent]: folder.GetIntents()) {
        if (intent.GetAction() != app::FileIntent::Action::WHITELIST) continue;
        std::cout << FWHT("[W] ") << intent.GetSrc() << std::endl;
    }

    for (const auto &[dest, keys]: conflicts) {
        std::cout << FRED("[!] (" << keys.size() << ") ") << dest << std::endl;
        for (auto &key: keys) {
            auto intent = folder.GetIntents().at(key);
            std::cout << "\t\t" << intent.GetSrc() << std::endl;
        }
    }
}

std::string fetch_api_token() {
    auto credentials_opt = app::load_credentials_from_filepath("res/credentials");
    if (!credentials_opt) {
        std::cerr << "Failed to load credentials: " << credentials_opt.error() << std::endl;
        exit(1);
    }

    auto& credentials = credentials_opt.value();
    auto token_opt = tvdb_api::login(
        credentials.api_key.c_str(),
        credentials.user_key.c_str(),
        credentials.username.c_str()
    );

    if (!token_opt) {
        std::cerr << "Failed to login" << std::endl;
        exit(1);
    }
    auto& token = token_opt.value();

    std::cout 
        << "apikey=" << credentials.api_key << "\n"
        << "userkey=" << credentials.user_key << "\n"
        << "username=" << credentials.username << "\n"
        << "token=" << token << std::endl;

    return token;
}

std::optional<tvdb_api::TVDB_Cache> load_cache_from_directory(fs::path root) {
    // load from cache
    const fs::path series_cache_fn = root / "series.json";
    const fs::path episodes_cache_fn = root / "episodes.json";

    auto series_doc_opt = assert_document_load(
        util::load_document_from_file(series_cache_fn.string().c_str()),
        "Loading series file");
    if (!series_doc_opt) {
        return {};
    }
    auto& series_doc = series_doc_opt.value();

    auto episodes_doc_opt = assert_document_load(
        util::load_document_from_file(episodes_cache_fn.string().c_str()),
        "Loading episodes file");
    if (!episodes_doc_opt) {
        return {};
    }
    auto& episodes_doc = episodes_doc_opt.value();

    auto series_opt = tvdb_api::load_series_info(series_doc);
    if (!series_opt) {
        std::cerr << series_opt.error() << std::endl;
        return {};
    }
    auto& series_info = series_opt.value();

    auto episodes_opt = tvdb_api::load_series_episodes_info(episodes_doc);
    if (!episodes_opt) {
        std::cerr << episodes_opt.error() << std::endl;
        return {};
    }
    auto& episodes_info = episodes_opt.value();

    auto tvdb_cache = tvdb_api::TVDB_Cache{
        std::move(series_info), 
        std::move(episodes_info)
    };

    return std::move(tvdb_cache);
}

std::optional<tvdb_api::TVDB_Cache> load_cache_from_api(fs::path root, const std::string& token) {
    // NOTE: We rely on the series info being cached already 
    const fs::path series_cache_fn = root / "series.json";
    auto series_doc_opt = assert_document_load(
        util::load_document_from_file(series_cache_fn.string().c_str()),
        "Loading series file");
    if (!series_doc_opt) {
        return {};
    }
    auto& series_doc = series_doc_opt.value();

    auto series_opt = tvdb_api::load_series_info(series_doc);
    if (!series_opt) {
        std::cerr << series_opt.error() << std::endl;
        return {};
    }
    auto& series_info = series_opt.value();

    // Load episodes data from api
    const uint32_t id = series_info.id;
    auto episodes_doc_opt = tvdb_api::get_series_episodes(id, token.c_str());
    if (!episodes_doc_opt) {
        std::cerr << "Failed to get episodes info from tvdb" << std::endl;
        return {};
    }

    auto& episodes_doc = episodes_doc_opt.value();
    auto episodes_opt = tvdb_api::load_series_episodes_info(episodes_doc);
    if (!episodes_opt) {
        std::cerr << episodes_opt.error() << std::endl;
        return {};
    }
    auto& episodes_info = episodes_opt.value();

    auto tvdb_cache = tvdb_api::TVDB_Cache{
        std::move(series_info), 
        std::move(episodes_info)
    };

    return std::move(tvdb_cache);
}
