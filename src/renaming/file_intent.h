#pragma once

#include <string>

namespace app {

// for each file, we can perform 4 different actions
// these are processed by renamer.cpp
struct FileIntent {
    enum Action {
        RENAME      = 1<<0,
        COMPLETE    = 1<<1,
        IGNORE      = 1<<2,
        DELETE      = 1<<3,
        WHITELIST   = 1<<4,
    };

    std::string src;
    std::string dest; // used by Action::RENAME to determine new location of file
    Action action;
    bool is_conflict; // used by other functions to check if action will conflict
    bool is_active;   // flag that can be used to disable/enable an action
};


};