#pragma once

#include <unordered_map>
#include <string>
#include <optional>

namespace tvdb_api {

// stores info about series
struct SeriesInfo {
    uint32_t id;
    std::string name;
    std::string air_date;
    std::string status;
    std::optional<std::string> overview = std::nullopt;
};

// stores info about an episode 
struct EpisodeInfo {
    uint32_t id = 0;
    int season = 0;
    int episode = 0;
    std::string air_date;
    std::string name;
    std::optional<std::string> overview = std::nullopt;
};

// key for unordered map for episode lookup
struct EpisodeKey {
    int season;
    int episode;

    const int GetHash() const {
        return (season * (1 << 16)) + episode;
    }

    bool operator ==(const EpisodeKey& rhs) const {
        return GetHash() == rhs.GetHash();
    }
};

// define our hash function here to pass to unordered_map
struct EpisodeKeyHasher {
    std::size_t operator()(const EpisodeKey& k) const noexcept {
        return k.GetHash();
    }
};

typedef std::unordered_map<EpisodeKey, EpisodeInfo, EpisodeKeyHasher> EpisodesMap;

// a cache that stores combined series and episodes info
struct TVDB_Cache {
    SeriesInfo series;
    EpisodesMap episodes;
};

}