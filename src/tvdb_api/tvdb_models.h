#pragma once

#include <unordered_map>
#include <string>

namespace tvdb_api {

// stores info about series
struct SeriesInfo {
    uint32_t id;
    std::string name;
    std::string air_date;
    std::string status;
};

// stores info about an episode 
struct EpisodeInfo {
    uint32_t id;
    int season;
    int episode;
    std::string air_date;
    std::string name;

    EpisodeInfo() {
        id = 0;
        season = 0;
        episode = 0;
        air_date = "";
        name = "";
    }
};

// key for unordered map for episode lookup
struct EpisodeKey {
    const int season;
    const int episode;

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