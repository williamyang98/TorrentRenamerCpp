#pragma once

// wrap a list of raw file intents with management data structures
// these include:
// - keeping track of conflicting files
// - counts for each file action

#include <string>
#include <filesystem>
#include <list>
#include <map>
#include <unordered_map>

#include "file_intent.h"

namespace app
{

class ManagedFileIntent;

// use maps so when we iterate over them in the ui the order is maintained alphabetically
// file table
typedef std::map<std::string, ManagedFileIntent> FileTable;
// conflict table (store keys into our file table)
typedef std::map<std::string, std::list<std::string>> ConflictTable;
// table for tracking upcoming additions
typedef std::unordered_map<std::string, int> UpcomingCounts;

// store the management data structures for an entire folder
class ManagedFolder 
{
public:
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
    UpcomingCounts upcoming_counts;
    ActionCount action_counts;
    bool is_conflict_table_dirty;
public:
    ManagedFolder() : intents(), conflicts(), upcoming_counts(), is_conflict_table_dirty(false) {}
    void AddIntent(FileIntent &intent);
    inline FileTable &GetIntents() { return intents; }
    ConflictTable &GetConflicts();
    inline ActionCount &GetActionCount() { 
        UpdateConflictTable();
        return action_counts; 
    }

    // disable copy and move semantics which break the folder to file intent relationship
    ManagedFolder(const ManagedFolder&) = delete;
    ManagedFolder(ManagedFolder&&) = delete;
    ManagedFolder& operator=(const ManagedFolder&) = delete;
    ManagedFolder& operator=(ManagedFolder&&) = delete;
public:
    friend class ManagedFileIntent;
private:
    void UpdateConflictTable();
    void UpdateActionCount(FileIntent::Action action, int delta);
};

// wrapper for FileIntent which updates the parent folder data structure
class ManagedFileIntent 
{
private:
    ManagedFolder &m_folder;
    FileIntent m_intent;
    std::string m_old_dest;
public:
    // transfer ownership of intent to managed file intent
    ManagedFileIntent(ManagedFolder &folder, FileIntent &intent)
    : m_folder(folder) {
        m_intent = std::move(intent);
        m_old_dest = m_intent.dest;
    }

    FileIntent& GetUnmanagedIntent() { return m_intent; }
    void SetAction(FileIntent::Action new_action);
    inline FileIntent::Action GetAction() const { return m_intent.action; }
    void SetIsActive(bool new_is_active);
    inline bool GetIsActive() const { return m_intent.is_active; }
    inline const std::string &GetSrc() const { return m_intent.src; }
    // difficult to setup a hook for detecting changes on std::string
    inline std::string &GetDest() { return m_intent.dest; }
    inline const std::string &GetDest() const { return m_intent.dest; }
    void OnDestChange();

    inline void SetIsConflict(bool new_is_conflict) { m_intent.is_conflict = new_is_conflict; }
    inline bool GetIsConflict() const { return m_intent.is_conflict; }
};


}
