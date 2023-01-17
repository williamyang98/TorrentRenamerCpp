#pragma once

#include <string>
#include <optional>
#include <vector>
#include <rapidjson/document.h>
#include "util/expected.hpp"

namespace tvdb_api 
{

using sid_t = uint32_t;

// Unexpected value is a string containing the error message
// NOTE: These api calls do not perform any JSON schema validation
//       That is expected to be done through calls in tvdb_json.h

// Returns an access token
tl::expected<std::string, std::string> login(const char* apikey, const char* userkey, const char* username);
bool refresh_token(const char* token);

tl::expected<rapidjson::Document, std::string> search_series(const char* name, const char* token);
tl::expected<rapidjson::Document, std::string> get_series(sid_t id, const char* token);
tl::expected<rapidjson::Document, std::string> get_series_episodes(sid_t id, const char* token);

};