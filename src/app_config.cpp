#include <rapidjson/document.h>
#include <rapidjson/schema.h>

#include <spdlog/spdlog.h>
#include <fmt/core.h>

#include "app.h"
#include "file_loading.h"

namespace app {

const char *APP_CONFIG_SCHEMA = 
R"({
    'title': 'app config',
    'descripton': 'Config file for gui app',
    'type': 'object',
    'properties': {
        'credentials_file': {
            'type': 'string'
        },
        'schema_file': {
            'type': 'string'
        },
        'whitelist_folders': {
            'type': 'array',
            'items': {
                'type': 'string'
            }
        },
        'whitelist_filenames': {
            'type': 'array',
            'items': {
                'type': 'string'
            }
        },
        'blacklist_extensions': {
            'type': 'array',
            'items': {
                'type': 'string'
            }
        },
        'whitelist_tags': {
            'type': 'array',
            'items': {
                'type': 'string'
            }
        }
    },
    'required': ['credentials_file', 'schema_file']
})";

rapidjson::SchemaDocument APP_SCHEMA_DOC = []() {
    rapidjson::Document schema_doc;
    schema_doc.Parse(APP_CONFIG_SCHEMA);
    return rapidjson::SchemaDocument(schema_doc);
} ();

AppConfig load_app_config_from_filepath(const char *filename) {
    auto load_result = load_document_from_file(filename);
    if (load_result.code != DocumentLoadCode::OK) {
        auto err = fmt::format("Failed to load app config json from: {}", filename);
        spdlog::critical(err);
        throw std::runtime_error(err);
    }

    auto doc = std::move(load_result.doc);
    if (!validate_document(doc, APP_SCHEMA_DOC)) {
        auto err = fmt::format("App config file ({}) has invalid format", filename); 
        spdlog::critical(err);
        throw std::runtime_error(err);
    }

    auto load_string_list = [](rapidjson::Document &doc, const char *key) {
        std::vector<std::string> vec;
        if (!doc.HasMember(key)) {
            return vec;
        }
        const auto &v = doc[key].GetArray();
        for (auto &e: v) {
            vec.push_back(e.GetString());
        }
        return vec;
    };

    AppConfig cfg;
    cfg.credentials_filepath = doc["credentials_file"].GetString();
    cfg.schema_filepath = doc["schema_file"].GetString();
    cfg.blacklist_extensions = load_string_list(doc, "blacklist_extensions");
    cfg.whitelist_filenames = load_string_list(doc, "whitelist_filenames");
    cfg.whitelist_folders = load_string_list(doc, "whitelist_folders");
    cfg.whitelist_tags = load_string_list(doc, "whitelist_tags");
    return cfg;
}

};