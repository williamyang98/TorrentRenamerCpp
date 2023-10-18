#pragma once

#include <string>
#include <map>

namespace app 
{

class AppFolderBookmarks 
{
public:
    struct Flags {
        bool is_starred = false; 
        bool is_read = false;
        bool is_unread = false;

        bool GetIsAnyFlagged() const {
            return is_starred || is_read || is_unread;
        }
    };
public:
    std::map<std::string, Flags> m_flags; 
    bool m_is_dirty = false;
    Flags& GetFlags(const std::string& key) {
        auto res = m_flags.find(key);
        if (res == m_flags.end()) {
            res = m_flags.insert({ key, Flags {} }).first;
        }
        return res->second;
    }
};

};
