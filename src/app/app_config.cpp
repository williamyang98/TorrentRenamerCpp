#include "app_config.h"
#include "app_schemas.h"
#include "util/file_loading.h"

#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <spdlog/spdlog.h>
#include <fmt/core.h>

namespace app 
{

AppConfig load_app_config_from_filepath(const char* filename) {
    auto load_result = util::load_document_from_file(filename);
    if (load_result.code != util::DocumentLoadCode::OK) {
        auto err = fmt::format("Failed to load app config json from: {}", filename);
        spdlog::critical(err);
        throw std::runtime_error(err);
    }

    auto& doc = load_result.doc;
    if (!util::validate_document(doc, APP_SCHEMA_DOC)) {
        auto err = fmt::format("App config file ({}) has invalid format", filename); 
        spdlog::critical(err);
        throw std::runtime_error(err);
    }

    auto load_string_list = [](rapidjson::Document& doc, const char* key) {
        std::vector<std::string> vec;
        if (!doc.HasMember(key)) {
            return vec;
        }
        const auto& v = doc[key].GetArray();
        for (auto& e: v) {
            vec.push_back(e.GetString());
        }
        return vec;
    };

    AppConfig cfg;
    cfg.credentials_filepath = doc["credentials_file"].GetString();
    cfg.blacklist_extensions = load_string_list(doc, "blacklist_extensions");
    cfg.whitelist_filenames = load_string_list(doc, "whitelist_filenames");
    cfg.whitelist_folders = load_string_list(doc, "whitelist_folders");
    cfg.whitelist_tags = load_string_list(doc, "whitelist_tags");
    return cfg;
}

};
