#pragma once

#include <optional>
#include <vector>
#include <rapidjson/document.h>

namespace tvdb_api 
{

typedef uint32_t sid_t;

struct SeriesSearchResult {
    std::string name;
    std::string date;
    uint32_t id;
};

std::optional<std::string> login(const char *apikey, const char *userkey, const char *username);
bool refresh_token(const char *token);
std::vector<SeriesSearchResult> search_series(const char *name, const char *token);
std::optional<rapidjson::Document> get_series(sid_t id, const char *token);
std::optional<rapidjson::Document> get_series_episodes(sid_t id, const char *token);

};