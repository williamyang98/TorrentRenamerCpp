#include <fstream>
#include <string>
#include <vector>
#include <optional>

#include <cpr/cpr.h>
#include <rapidjson/writer.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/schema.h>
#include <rapidjson/error/en.h>

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "tvdb_api.h"
#include "util/file_loading.h"
#include "util/expected.hpp"

// NOTE: HTTPS requires extra work which I don't know how to do
// #define BASE_URL "https://api.thetvdb.com/"
#define BASE_URL "http://api.thetvdb.com/"
constexpr int HTTP_CODE_OK = 200;

cpr::Header create_token_header(const char* token) {
    return cpr::Header{{"Authorization", "Bearer " + std::string(token)}};
}

namespace tvdb_api 
{

tl::expected<std::string, std::string> login(const char* apikey, const char* userkey, const char* username) {
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

    if (r.status_code != HTTP_CODE_OK) {
        auto err = fmt::format("Got invalid http_code for url={}, http_code={}", r.url.c_str(), r.status_code);
        return tl::make_unexpected<std::string>(std::move(err));
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(r.text.c_str());
    if (ok.IsError()) {
        auto err = fmt::format("Got invalid JSON for url={}, code={}, offset={}", r.url.c_str(), ok.Code(), ok.Offset());
        return tl::make_unexpected<std::string>(std::move(err));
    }

    if (!doc.HasMember("token")) {
        auto err = fmt::format("Missing field 'token' in url={}", r.url.c_str());
        return tl::make_unexpected<std::string>(std::move(err));
    }

    if (!doc["token"].IsString()) {
        auto err = fmt::format("Field 'token' is not a string in url={}", r.url.c_str());
        return tl::make_unexpected<std::string>(std::move(err));
    }

    return doc["token"].GetString();
}

bool refresh_token(const char* token) {
    auto r = cpr::Get(
        cpr::Url(BASE_URL "refresh_token"),
        create_token_header(token)
    );

    return (r.status_code == HTTP_CODE_OK);
}

tl::expected<rapidjson::Document, std::string> search_series(const char* name, const char* token) {
    auto r = cpr::Get(
        cpr::Url(BASE_URL "search/series"),
        create_token_header(token),
        cpr::Parameters{{"name", name}}
    );

    if (r.status_code != HTTP_CODE_OK) {
        auto err = fmt::format("Got invalid http_code for url={}, http_code={}", r.url.c_str(), r.status_code);
        return tl::make_unexpected<std::string>(std::move(err));
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(r.text.c_str());
    if (ok.IsError()) {
        auto err = fmt::format("Got invalid JSON for url={}, code={}, offset={}", r.url.c_str(), ok.Code(), ok.Offset());
        return tl::make_unexpected<std::string>(std::move(err));
    }

    if (!doc.HasMember("data")) {
        auto err = fmt::format("Missing field 'data' in url={}", r.url.c_str());
        return tl::make_unexpected<std::string>(std::move(err));
    }

    doc.Swap(doc["data"]);
    return doc;
}

tl::expected<rapidjson::Document, std::string> get_series(sid_t id, const char* token) {
    auto r = cpr::Get(
        cpr::Url(BASE_URL "series/" + std::to_string(id)),
        create_token_header(token)
    );

    if (r.status_code != HTTP_CODE_OK) {
        auto err = fmt::format("Got invalid http_code for url={}, http_code={}", r.url.c_str(), r.status_code);
        return tl::make_unexpected<std::string>(std::move(err));
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(r.text.c_str());
    if (ok.IsError()) {
        auto err = fmt::format("Got invalid JSON for url={}, code={}, offset={}", r.url.c_str(), ok.Code(), ok.Offset());
        return tl::make_unexpected<std::string>(std::move(err));
    }

    if (!doc.HasMember("data")) {
        auto err = fmt::format("Missing field 'data' in url={}", r.url.c_str());
        return tl::make_unexpected<std::string>(std::move(err));
    }

    doc.Swap(doc["data"]);
    return doc;
}

tl::expected<rapidjson::Document, std::string> get_series_episodes(sid_t id, const char* token) {
    // Append additional pages into our super document
    rapidjson::Document combined_doc;
    combined_doc.SetArray();

    auto add_page = [id, token, &combined_doc](const int page) -> tl::expected<rapidjson::Document, std::string> {
        // Attempt to get page from url
        auto r = cpr::Get(
            cpr::Url(BASE_URL "series/" + std::to_string(id) + "/episodes"),
            create_token_header(token),
            cpr::Parameters{{"page", std::to_string(page)}}
        );

        if (r.status_code != HTTP_CODE_OK) {
            auto err = fmt::format("Got invalid http_code for url={}, page={}, http_code={}", r.url.c_str(), page, r.status_code);
            return tl::make_unexpected<std::string>(std::move(err));
        }

        // Check if the response was valid json
        rapidjson::Document doc;
        rapidjson::ParseResult ok = doc.Parse(r.text.c_str());
        if (ok.IsError()) {
            auto err = fmt::format("Got invalid JSON for url={}, page={}, code={}, offset={}", r.url.c_str(), page, ok.Code(), ok.Offset());
            return tl::make_unexpected<std::string>(std::move(err));
        }

        if (!doc.HasMember("data")) {
            auto err = fmt::format("Missing field 'data' in url={}, page={}", r.url.c_str(), page);
            return tl::make_unexpected<std::string>(std::move(err));
        }

        // Append the json data to the combined document
        auto episodes_data = doc["data"].GetArray();
        for (auto& ep_data: episodes_data) {
            rapidjson::Value ep_copy;
            ep_copy.CopyFrom(ep_data, combined_doc.GetAllocator()) ;
            combined_doc.PushBack(ep_copy, combined_doc.GetAllocator());
        }

        return doc;
    };

    // Need first page to get number of pages
    auto page_1_doc_opt = add_page(1);
    if (!page_1_doc_opt) {
        return tl::make_unexpected<std::string>(std::move(page_1_doc_opt.error()));
    }
    auto& page_1_doc = page_1_doc_opt.value();

    // NOTE: If we do not have links to the next and last page
    //       then we assume that this is the only page of episodes data
    if (!page_1_doc.HasMember("links")) {
        return combined_doc;
    }
    auto& links = page_1_doc["links"];
    if (!links.HasMember("next") || !links["next"].IsInt()) {
        return combined_doc;
    }
    if (!links.HasMember("last") || !links["last"].IsInt()) {
        return combined_doc;
    }
    const int next_page = links["next"].GetInt();
    const int last_page = links["last"].GetInt();

    // Iterate through the pagination list
    for (int i = next_page; i <= last_page; i++) {
        auto page_doc_opt = add_page(i);
        if (!page_doc_opt) {
            return tl::make_unexpected<std::string>(std::move(page_doc_opt.error()));
        }
    }

    return combined_doc;
}

};
