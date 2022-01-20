#include <cpr/cpr.h>
#include <fstream>
#include <string>

#include <rapidjson/reader.h>
#include <rapidjson/writer.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/schema.h>

#include <vector>
#include <optional>

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "tvdb_api.h"
#include "tvdb_api_schema.h"

namespace tvdb_api 
{

// #define BASE_URL "https://api.thetvdb.com/"
#define BASE_URL "http://api.thetvdb.com/"

void validate_response(const rapidjson::Document &doc, rapidjson::SchemaDocument &schema_doc) {
    rapidjson::SchemaValidator validator(schema_doc);
    if (doc.Accept(validator)) {
        return;
    }
    spdlog::error("Api response doesn't match schema");

    rapidjson::StringBuffer sb;
    validator.GetInvalidDocumentPointer().StringifyUriFragment(sb);
    spdlog::error(fmt::format("document pointer: {}", sb.GetString()));
    spdlog::error(fmt::format("error-type: {}", validator.GetInvalidSchemaKeyword()));
    sb.Clear();

    validator.GetInvalidSchemaPointer().StringifyUriFragment(sb);
    spdlog::error(fmt::format("schema pointer: {}", sb.GetString()));
    sb.Clear();

    throw std::runtime_error("tvdb api response failed to match schema");
}

cpr::Header create_token_header(const char *token) {
    return cpr::Header{{"Authorization", "Bearer " + std::string(token)}};
}

std::optional<std::string> login(const char *apikey, const char *userkey, const char *username) {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    writer.StartObject();
    writer.Key("apikey");
    writer.String(apikey);
    writer.Key("userkey");
    writer.String(userkey);
    writer.Key("username");
    writer.String(username);
    writer.EndObject();

    auto r = cpr::Post(
        cpr::Url(BASE_URL "login"),
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body(sb.GetString())
    );

    if (r.status_code != 200) {
        return {};
    }

    rapidjson::Document doc;
    doc.Parse(r.text.c_str());
    return doc["token"].GetString();
}

bool refresh_token(const char *token) {
    auto r = cpr::Get(
        cpr::Url(BASE_URL "refresh_token"),
        create_token_header(token)
    );

    return (r.status_code == 200);
}

std::optional<rapidjson::Document> search_series(const char *name, const char *token) {
    auto r = cpr::Get(
        cpr::Url(BASE_URL "search/series"),
        create_token_header(token),
        cpr::Parameters{{"name", name}}
    );

    if (r.status_code != 200) {
        return {};
    }

    rapidjson::Document doc;
    doc.Parse(r.text.c_str());
    doc.Swap(doc["data"]);
    validate_response(doc, SEARCH_DATA_SCHEMA);
    return doc;
}

std::optional<rapidjson::Document> get_series(sid_t id, const char *token) {
    auto r = cpr::Get(
        cpr::Url(BASE_URL "series/" + std::to_string(id)),
        create_token_header(token)
    );

    if (r.status_code != 200) {
        return {};
    }

    rapidjson::Document doc;
    doc.Parse(r.text.c_str());
    doc.Swap(doc["data"]);
    validate_response(doc, SERIES_DATA_SCHEMA);
    return doc;
}

std::optional<rapidjson::Document> get_series_episodes(sid_t id, const char *token) {
    auto get_page = [id, token](int page) {
        auto r = cpr::Get(
            cpr::Url(BASE_URL "series/" + std::to_string(id) + "/episodes"),
            create_token_header(token),
            cpr::Parameters{{"page", std::to_string(page)}}
        );
        return r;
    };

    auto r = get_page(1);
    if (r.status_code != 200) {
        return {};
    }

    rapidjson::Document combined_doc;
    combined_doc.SetArray();

    auto add_episodes = [&combined_doc](rapidjson::Document &doc) {
        auto episodes_data = doc["data"].GetArray();
        for (auto &ep_data: episodes_data) {
            rapidjson::Value ep_copy;
            ep_copy.CopyFrom(ep_data, combined_doc.GetAllocator()) ;
            combined_doc.PushBack(ep_copy, combined_doc.GetAllocator());
        }
    };

    rapidjson::Document doc;
    doc.Parse(r.text.c_str());
    add_episodes(doc);
    
    auto &links = doc["links"];
    if (!(links["next"].IsInt() && links["last"].IsInt())) {
        return combined_doc;
    }

    int next_page = links["next"].GetInt();
    int last_page = links["last"].GetInt();

    for (int i = next_page; i <= last_page; i++) {
        auto r0 = get_page(i);
        if (r0.status_code != 200) {
            spdlog::critical("Request failed in middle of page loading for episodes data");
            throw std::runtime_error("Request failed in middle of page loading for episodes data");
        }

        rapidjson::Document doc0;
        doc0.Parse(r0.text.c_str());
        add_episodes(doc0);
    }
    
    validate_response(combined_doc, EPISODES_DATA_SCHEMA);
    return combined_doc;
}

};