#pragma once

// Managed the state of the folder and its files
// - Allow for reverting of changes
// - Check for conflicts
// - Keep track of files
// The AppFolderState and AppFileState are really a tree data structure
// - AppFileState are editable leaf nodes
// - AppFolderState keeps track of the entire tree's state

#include <string>
#include <filesystem>
#include <list>
#include <map>
#include <unordered_map>

#include "file_intents.h"
#include "app_file_state.h"

namespace app
{

class AppFolderState 
{
public:
    // NOTE: Use std::map so that iteration is alphabetical
    using FileTable = std::map<std::string, AppFileState>;
    using ConflictTable = std::map<std::string, std::list<std::string>>;
    using UpcomingRenames = std::unordered_map<std::string, int>;

    struct ActionCount {
        int deletes = 0; 
        int renames = 0; 
        int ignores = 0;
        int completes = 0;
        int whitelists = 0;
    };
private:
    FileTable intents;
    ConflictTable conflicts;
    UpcomingRenames upcoming_rename_counts;
    ActionCount action_counts;
    bool is_conflict_table_dirty;
public:
    AppFolderState();
    ~AppFolderState();
    // NOTE: intent object is now invalid since managed file intent transfers ownership
    void AddIntent(FileIntent&& file_intent);
    FileTable& GetIntents() { return intents; }
    ConflictTable& GetConflicts();
    ActionCount& GetActionCount() { 
        UpdateConflictTable();
        return action_counts; 
    }

    // NOTE: Cannot change location of folder state since the file state takes it via reference
    AppFolderState(const AppFolderState&) = delete;
    AppFolderState(AppFolderState&&) = delete;
    AppFolderState& operator=(const AppFolderState&) = delete;
    AppFolderState& operator=(AppFolderState&&) = delete;
private:
    void UpdateConflictTable();
    void UpdateActionCount(FileIntent::Action action, int delta);
    friend AppFileState;
};

}
