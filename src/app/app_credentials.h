#pragma once
#include <string>
#include <optional>
#include "util/expected.hpp"

namespace app
{

struct AppCredentials {
    std::string api_key;
    std::string user_key;
    std::string username;
    std::optional<std::string> token;
};

tl::expected<AppCredentials, std::string> load_credentials_from_filepath(const char* filename);

};