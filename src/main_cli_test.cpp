#include <iostream>
#include <fstream>
#include <string>

#include <rapidjson/reader.h>
#include <rapidjson/writer.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/schema.h>
#include <rapidjson/prettywriter.h>

#include <filesystem>

#define PROFILE_ENABLED 0

#include "Instrumentor.h"
#include "console_colours.h"

#include "app_credentials_schema.h"
#include "tvdb_api.h"
#include "scanner.h"
#include "file_loading.h"

namespace fs = std::filesystem;

void scan_directory(const fs::path &subdir);
void assert_validation(bool is_valid, const char *message);
rapidjson::Document assert_document_load(app::DocumentLoadResult res, const char *message);

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cout << "Enter directory to scan" << std::endl;
        return 1;
    }

    auto cred_doc = assert_document_load(
        app::load_document_from_file("credentials.json"),
        "Loading credentials file");

    assert_validation(
        app::validate_document(cred_doc, app::CREDENTIALS_SCHEMA), 
        "Credentials file");

    app::write_json_to_stream(cred_doc, std::cout);

    // Instrumentor::Get().BeginSession("Renaming");

    // const char *fn = "D:/TV Shows/Air Crash Investigation (Mayday) (2003)/";
    const fs::path root(argv[1]);

    if (argc == 2) {
        for (auto &subdir: fs::directory_iterator(root)) {
            if (!subdir.is_directory()) {
                continue;
            }
            scan_directory(subdir);
        }
    } else {
        scan_directory(root);
    }

    // Instrumentor::Get().EndSession();

    return 0;
}

void assert_validation(bool is_valid, const char *message) {
    if (is_valid) { 
        return;
    }
    std::cerr << "Failed validation (" << message << ")" << std::endl;
    exit(1);
}

rapidjson::Document assert_document_load(app::DocumentLoadResult res, const char *message) {
    if (res.code != app::DocumentLoadCode::OK) {
        std::cerr << "Failed to load document (" << message << ")" << std::endl;
        exit(1);
    }
    return std::move(res.doc);
}

void scan_directory(const fs::path &subdir) {
    std::cout << "Scanning directory: " << subdir << std::endl;

    rapidjson::Document series_info, episodes_info;

    // load from cache
    if (0) {
        const fs::path series_cache_fn = subdir / "series.json";
        series_info = assert_document_load(
            app::load_document_from_file(series_cache_fn.string().c_str()),
            "Loading series file");

        const fs::path episodes_cache_fn = subdir / "episodes.json";
        episodes_info = assert_document_load(
            app::load_document_from_file(episodes_cache_fn.string().c_str()),
            "Loading episodes file");
    
    // load from db
    } else {
        auto cred_doc = assert_document_load(
            app::load_document_from_file("credentials.json"),
            "Loading credentials file");

        assert_validation(
            app::validate_document(cred_doc, app::CREDENTIALS_SCHEMA),
            "Validating credentials file");

        const fs::path series_cache_fn = subdir / "series.json";
        series_info = assert_document_load(
            app::load_document_from_file(series_cache_fn.string().c_str()),
            "Loading series file");

        std::string token;
        {
            auto res = tvdb_api::login(
                cred_doc["credentials"]["apikey"].GetString(), 
                cred_doc["credentials"]["userkey"].GetString(),
                cred_doc["credentials"]["username"].GetString());
            
            if (!res) {
                std::cerr << "Failed to login" << std::endl;
                exit(1);
            }

            token = std::move(res.value());
        }

        auto series_cache   = app::load_series_info(series_info);
        uint32_t id = series_cache.id;

        auto episodes_info_doc = tvdb_api::get_series_episodes(id, token.c_str());
        if (!episodes_info_doc) {
            std::cerr << "Failed to get episodes info from tvdb" << std::endl;
            exit(1);
        }

        episodes_info = std::move(episodes_info_doc.value());
    }

    auto series_cache   = app::load_series_info(series_info);
    auto episodes_cache = app::load_series_episodes_info(episodes_info);

    app::TVDB_Cache tvdb_cache { series_cache, episodes_cache };

    app::RenamingConfig cfg;
    cfg.blacklist_extensions.push_back("nfo");
    cfg.blacklist_extensions.push_back("ext");

    cfg.whitelist_folders.push_back("Extras");

    auto folder = app::ManagedFolder();
    for (auto intent: app::scan_directory(subdir, tvdb_cache, cfg)) {
        folder.AddIntent(intent);
    }

    auto &counts = folder.GetActionCount();
    auto &conflicts = folder.GetConflicts();
    
    std::cout << "completed=" << counts.completes << std::endl;
    std::cout << "ignores=" << counts.ignores << std::endl;
    std::cout << "deletes=" << counts.deletes << std::endl;
    std::cout << "conflicts=" << conflicts.size() << std::endl;
    std::cout << "pending=" << counts.renames << std::endl;

    for (const auto &[key, intent]: folder.GetIntents()) {
        if (intent.GetAction() != app::FileIntent::Action::COMPLETE) continue;
        std::cout << FGRN("[C] ") << intent.GetSrc() << std::endl;
    }

    for (const auto &[key, intent]: folder.GetIntents()) {
        if (intent.GetAction() != app::FileIntent::Action::IGNORE) continue;
        std::cout << FMAG("[I] ") << intent.GetSrc() << std::endl;
    }

    for (const auto &[key, intent]: folder.GetIntents()) {
        if (intent.GetAction() != app::FileIntent::Action::RENAME) continue;
        std::cout << FCYN("[P] ") << intent.GetSrc() << " ==> " << intent.GetDest() << std::endl;
    }

    for (const auto &[key, intent]: folder.GetIntents()) {
        if (intent.GetAction() != app::FileIntent::Action::DELETE) continue;
        std::cout << FYEL("[D] ") << intent.GetSrc() << std::endl;
    }

    for (const auto &[dest, keys]: conflicts) {
        std::cout << FRED("[?] (" << keys.size() << ") ") << dest << std::endl;
        for (auto &key: keys) {
            auto intent = folder.GetIntents().at(key);
            std::cout << "\t\t" << intent.GetSrc() << std::endl;
        }
    }
}
