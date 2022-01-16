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

#include "tvdb_api.h"
#include "renamer.h"
#include "file_loading.h"

namespace fs = std::filesystem;

void scan_directory(const fs::path &subdir, app::app_schema_t &schema);

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cout << "Enter directory to scan" << std::endl;
        return 1;
    }

    auto schema_file = app::load_schema_from_file("schema.json");
    auto cred_doc = app::load_document_from_file("credentials.json");

    auto &validator = schema_file.at(std::string(SCHEME_CRED_KEY));
    app::validate_document(cred_doc, validator, "Credentials");
    app::write_json_to_stream(cred_doc, std::cout);

    // Instrumentor::Get().BeginSession("Renaming");

    // const char *fn = "D:/TV Shows/Air Crash Investigation (Mayday) (2003)/";
    const fs::path root(argv[1]);

    if (argc == 2) {
        for (auto &subdir: fs::directory_iterator(root)) {
            if (!subdir.is_directory()) {
                continue;
            }
            scan_directory(subdir, schema_file);
        }
    } else {
        scan_directory(root, schema_file);
    }

    // Instrumentor::Get().EndSession();

    return 0;
}

void scan_directory(const fs::path &subdir, app::app_schema_t &schema) {
    std::cout << "Scanning directory: " << subdir << std::endl;

    rapidjson::Document series_info, episodes_info;

    // load from cache
    if (0) {
        const fs::path series_cache_fn = subdir / "series.json";
        series_info = std::move(app::load_document_from_file(series_cache_fn.string().c_str()));

        const fs::path episodes_cache_fn = subdir / "episodes.json";
        episodes_info = std::move(app::load_document_from_file(episodes_cache_fn.string().c_str()));
    
    // load from db
    } else {
        auto cred_doc = app::load_document_from_file("credentials.json");
        app::validate_document(cred_doc, schema.at(std::string(SCHEME_CRED_KEY)), "Valiating credentials");

        const fs::path series_cache_fn = subdir / "series.json";
        series_info = std::move(app::load_document_from_file(series_cache_fn.string().c_str()));

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

            token = res.value();
        }

        auto series_cache   = app::load_series_info(series_info);
        uint32_t id = series_cache.id;

        // auto series_info_doc = tvdb_api::get_series(id, token.c_str());
        // if (!series_info_doc) {
        //     std::cerr << "Failed to get series info from tvdb" << std::endl;
        //     exit(1);
        // }
        // series_info = std::move(series_info_doc.value());

        auto episodes_info_doc = tvdb_api::get_series_episodes(id, token.c_str());
        if (!episodes_info_doc) {
            std::cerr << "Failed to get episodes info from tvdb" << std::endl;
            exit(1);
        }

        episodes_info = std::move(episodes_info_doc.value());
    }

    app::validate_document(series_info, schema.at(std::string(SCHEME_SERIES_KEY)), "Validating series");
    app::validate_document(episodes_info, schema.at(std::string(SCHEME_EPISODES_KEY)), "Validating episodes");

    auto series_cache   = app::load_series_info(series_info);
    auto episodes_cache = app::load_series_episodes_info(episodes_info);

    app::TVDB_Cache tvdb_cache { series_cache, episodes_cache };

    app::RenamingConfig cfg;
    cfg.blacklist_extensions.push_back("nfo");
    cfg.blacklist_extensions.push_back("ext");

    cfg.special_folders.push_back("Extras");

    auto actions = app::scan_directory(subdir, tvdb_cache, cfg);
    
    std::cout << "completed=" << actions.completed.size() << std::endl;
    std::cout << "ignores=" << actions.ignores.size() << std::endl;
    std::cout << "deletes=" << actions.deletes.size() << std::endl;
    std::cout << "conflicts=" << actions.conflicts.size() << std::endl;
    std::cout << "pending=" << actions.pending.size() << std::endl;

    for (const auto &e: actions.completed) {
        std::cout << FGRN("[C] ") << e << std::endl;
    }

    for (const auto &e: actions.ignores) {
        std::cout << FMAG("[I] ") << e << std::endl;
    }

    for (const auto &e: actions.pending) {
        std::cout << FCYN("[P] ") << e.target << " ==> " << e.dest << std::endl;
    }

    for (const auto &e: actions.deletes) {
        std::cout << FYEL("[D] ") << e.filename << std::endl;
    }

    for (const auto &e: actions.conflicts) {
        auto &dest = e.first;
        auto &targets = e.second;
        std::cout << FRED("[?] (" << targets.size() << ") ") << dest << std::endl;
        for (auto &t: targets) {
            std::cout << "\t\t" << t.target << std::endl;
        }
    }
}
