#pragma once

#include <string>
#include "file_intents.h"

namespace app 
{

class AppFolderState;

// NOTE: Refer to AppFolderState.h for description of class
//       This class is really the leaf node of a tree data structure
class AppFileState 
{
private:
    AppFolderState& m_folder;
    FileIntent m_intent;
    std::string m_old_dest;
public:
    // NOTE: Transfer ownership of intent to managed file intent to avoid unnecessary copying
    AppFileState(AppFolderState& folder, FileIntent&& intent);
    ~AppFileState();

    // NOTE: We 
    FileIntent& GetIntent() { return m_intent; }
    const std::string& GetSrc() const { return m_intent.src; }
    const std::string& GetDest() const { return m_intent.dest; }
    FileIntent::Action GetAction() const { return m_intent.action; }
    bool GetIsActive() const { return m_intent.is_active; }
    bool GetIsConflict() const { return m_intent.is_conflict; }
    // NOTE: Imgui textedit takes in the buffer
    std::string& GetDest() { return m_intent.dest; }

    void SetAction(FileIntent::Action new_action);
    void SetIsActive(bool new_is_active);
    // NOTE: Imgui returns a boolean to indicate changes to the buffer so we know when to call this
    void OnDestChange();
private:
    void SetIsConflict(bool new_is_conflict) { m_intent.is_conflict = new_is_conflict; }
    friend class AppFolderState;
};

};