#include "app_config.h"
#include "app_schemas.h"
#include "util/file_loading.h"

#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <spdlog/spdlog.h>
#include <fmt/core.h>

namespace app 
{

static
std::vector<std::string> load_string_list(rapidjson::Document& doc, const char* key) {
    auto vec = std::vector<std::string>();
    if (!doc.HasMember(key)) {
        return vec;
    }

    const auto& arr = doc[key].GetArray();
    for (auto& entry: arr) {
        if (entry.IsString()) {
            vec.push_back(entry.GetString());
        }
    }
    return vec;
};

tl::expected<AppConfig, std::string> load_app_config_from_filepath(const char* filename) {
    auto load_result = util::load_document_from_file(filename);
    if (load_result.code != util::DocumentLoadCode::OK) {
        auto err = fmt::format("Failed to load app config json from: {}", filename);
        return tl::make_unexpected<std::string>(std::move(err));
    }

    auto& doc = load_result.doc;
    if (!util::validate_document(doc, APP_SCHEMA_DOC)) {
        auto err = fmt::format("App config file ({}) has invalid format", filename); 
        return tl::make_unexpected<std::string>(std::move(err));
    }

    AppConfig cfg;
    cfg.credentials_filepath = doc["credentials_file"].GetString();
    cfg.blacklist_extensions = load_string_list(doc, "blacklist_extensions");
    cfg.whitelist_filenames = load_string_list(doc, "whitelist_filenames");
    cfg.whitelist_folders = load_string_list(doc, "whitelist_folders");
    cfg.whitelist_tags = load_string_list(doc, "whitelist_tags");
    return cfg;
}

};
