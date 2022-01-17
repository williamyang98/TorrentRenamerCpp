#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <filesystem>
#include <list>

namespace app
{

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

// file table
typedef std::map<std::string, FileIntent> FileTable;
// conflict table
typedef std::map<std::string, std::list<std::string>> ConflictTable;
// table for tracking upcoming additions
typedef std::unordered_map<std::string, int> UpcomingCounts;

// stores all pending diffs made to a series folder
// also contains logic for determining which file intents produce conflicts
class FolderDiff 
{
public:
    struct ActionCount {
        int deletes = 0; 
        int renames = 0; 
        int ignores = 0;
        int completes = 0;
        int whitelists = 0;
    };
public:
    FileTable intents;
    ConflictTable conflicts;
    UpcomingCounts upcoming_counts;
    ActionCount action_counts;
    bool is_conflict_table_dirty;
public:
    FolderDiff() : intents(), conflicts(), upcoming_counts(), is_conflict_table_dirty(false) {}
    void AddIntent(FileIntent &intent);
    void OnIntentUpdate(FileIntent &intent, FileIntent::Action new_action);
    void OnIntentUpdate(FileIntent &intent, bool new_is_active);
    void UpdateConflictTable();
private:
    void UpdateActionCount(FileIntent::Action action, int delta);
};

}