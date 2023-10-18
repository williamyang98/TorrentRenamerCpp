#pragma once

#include <string>
#include "file_intents.h"

namespace app 
{

class AppFolderState;

class AppFileState 
{
private:
    AppFolderState& m_folder;
    FileIntent m_intent;
    std::string m_old_dest;
public:
    AppFileState(AppFolderState& folder, FileIntent&& intent);
    ~AppFileState();
    FileIntent& GetIntent() { return m_intent; }
    const std::string& GetSrc() const { return m_intent.src; }
    const std::string& GetDest() const { return m_intent.dest; }
    FileIntent::Action GetAction() const { return m_intent.action; }
    bool GetIsActive() const { return m_intent.is_active; }
    bool GetIsConflict() const { return m_intent.is_conflict; }
    void SetAction(FileIntent::Action new_action);
    void SetIsActive(bool new_is_active);

    // NOTE: Imgui takes in raw buffer and sets dirty flag on change
    std::string& GetDest() { return m_intent.dest; }
    void OnDestChange();
private:
    void SetIsConflict(bool new_is_conflict) { m_intent.is_conflict = new_is_conflict; }
    friend class AppFolderState;
};

};