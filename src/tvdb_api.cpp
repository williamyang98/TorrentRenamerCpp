#include <cpr/cpr.h>
#include <iostream>
#include <fstream>
#include <string>

#include <rapidjson/reader.h>
#include <rapidjson/writer.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/schema.h>

#include <vector>
#include <optional>

#include "tvdb_api.h"

namespace tvdb_api 
{

// #define BASE_URL "https://api.thetvdb.com/"
#define BASE_URL "http://api.thetvdb.com/"

void print_response(cpr::Response &r);

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

    print_response(r);
    return (r.status_code == 200);
}

std::vector<SeriesSearchResult> search_series(const char *name, const char *token) {
    auto r = cpr::Get(
        cpr::Url(BASE_URL "search/series"),
        create_token_header(token),
        cpr::Parameters{{"name", name}}
    );

    auto series = std::vector<SeriesSearchResult>();

    if (r.status_code != 200) {
        return series;
    }

    // print_response(r);
    rapidjson::Document doc;
    doc.Parse(r.text.c_str());

    auto data = doc["data"].GetArray();
    series.reserve(data.Size());

    for (auto &s: data) {
        SeriesSearchResult o;
        o.name = s["seriesName"].GetString();
        o.date = s["firstAired"].GetString();
        o.id = s["id"].GetUint();
        series.push_back(o);
    }
    
    return series;
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
        auto &episodes_data = doc["data"].GetArray();
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
            std::cerr << "Request failed in middle of page loading" << std::endl;
            exit(1);
        }

        rapidjson::Document doc0;
        doc0.Parse(r0.text.c_str());
        add_episodes(doc0);
    }
    
    return combined_doc;
}

void print_response(cpr::Response &r) {
    std::cout << "status_code: " << r.status_code << std::endl;
    // for (auto &e: r.header) {
    //     std::cout << "header[" << e.first << "]: " << e.second << std::endl;
    // }
    std::cout << "text: " << r.text.c_str() << std::endl;
}

};