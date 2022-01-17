#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <filesystem>
#include <list>

#include "file_loading.h"

namespace app
{

struct FileIntent {
    enum Action {
        RENAME      = 1<<0,
        COMPLETE    = 1<<1,
        IGNORE      = 1<<2,
        DELETE      = 1<<3,
    };

    std::string src;
    std::string dest;
    Action action;
    bool is_conflict;
    bool is_active;
};

// file table
typedef std::map<std::string, FileIntent> FileTable;
// conflict table
typedef std::map<std::string, std::list<std::string>> ConflictTable;
// table for tracking upcoming additions
typedef std::unordered_map<std::string, int> UpcomingCounts;

// stores all pending diffs made to a series folder
class SeriesState 
{
public:
    struct ActionCount {
        int deletes = 0; 
        int renames = 0; 
        int ignores = 0;
        int completes = 0;
    } action_counts;
public:
    FileTable intents;
    ConflictTable conflicts;
    UpcomingCounts upcoming_counts;
    bool is_conflict_table_dirty;

    SeriesState() : intents(), conflicts(), upcoming_counts(), is_conflict_table_dirty(false) {}
    void AddIntent(FileIntent &intent);
    void OnIntentUpdate(FileIntent &intent, FileIntent::Action new_action);
    void OnIntentUpdate(FileIntent &intent, bool new_is_active);
    void UpdateConflictTable();

private:
    void UpdateActionCount(FileIntent::Action action, int delta);
};

// config for renaming
struct RenamingConfig {
    std::vector<std::string> blacklist_extensions;
    std::vector<std::string> special_folders;
};

SeriesState scan_directory(
    const std::filesystem::path &root, 
    const TVDB_Cache &cache,
    const RenamingConfig &cfg);

};