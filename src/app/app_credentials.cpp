#include "app_credentials.h"
#include "app_schemas.h"
#include "util/file_loading.h"

#include <spdlog/spdlog.h>
#include <fmt/core.h>
#include <rapidjson/document.h>
#include <rapidjson/schema.h>

namespace app
{

tl::expected<AppCredentials, std::string> load_credentials_from_filepath(const char* filename)
{
    auto load_result = util::load_document_from_file(filename);
    if (load_result.code != util::DocumentLoadCode::OK) {
        auto err = fmt::format("Failed to load credentials json from: {}", filename);
        return tl::make_unexpected<std::string>(std::move(err));
    }

    auto& doc = load_result.doc;
    if (!util::validate_document(doc, CREDENTIALS_SCHEMA)) {
        auto err = fmt::format("Credentials file ({}) has invalid format", filename); 
        return tl::make_unexpected<std::string>(std::move(err));
    }

    auto credentials = AppCredentials();
    credentials.api_key = doc["credentials"]["apikey"].GetString();
    credentials.user_key = doc["credentials"]["userkey"].GetString();
    credentials.username = doc["credentials"]["username"].GetString();

    if (doc.HasMember("token")) {
        if (doc["token"].IsString()) {
            credentials.token = doc["token"].GetString();
        } else {
            credentials.token = std::nullopt; 
        }
    }

    return credentials;
}

};

