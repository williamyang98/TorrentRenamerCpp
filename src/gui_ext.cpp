#include <string>

#include <imgui.h>
#include <fmt/format.h>

#include "gui_ext.h"

namespace app::gui 
{

void RenderSeriesList(App &main_app) {
    static ImGuiTextFilter search_filter;
    search_filter.Draw();

    ImGui::Separator();

    if (ImGui::BeginListBox("series_list_box", ImVec2(-1,-1))) {
        for (int i = 0; i < main_app.m_folders.size(); i++) {
            auto &folder = main_app.m_folders[i];
            auto folder_name = folder.m_path.filename().string();
            auto label = folder_name.c_str();

            if (!search_filter.PassFilter(label)) {
                continue;
            }

            auto is_selected = i == main_app.m_current_folder;
            if (ImGui::Selectable(label, is_selected)) {
                main_app.select_folder(i);
            }
        }
        ImGui::EndListBox();
    }
}

static void RenderConflicts(ConflictFiles &conflicts);

void RenderEpisodes(App &main_app, SeriesFolder &folder) {
    if (ImGui::Button("Scan for changes")) {
        folder.update_state();
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh from cache")) {
        folder.load_cache();
        folder.update_state();
    }

    ImGui::SameLine();
    if (ImGui::Button("Download and refresh from tvdb")) {
        folder.refresh_cache(folder.m_cache.series.id, main_app.m_token.c_str());
        folder.update_state();
    }

    // render the state tree
    auto &state = folder.m_state;

    if (ImGui::BeginTabBar("Status")) {
        auto tab_name = fmt::format("Completed {:d}", state.completed.size());
        if (ImGui::BeginTabItem(tab_name.c_str())) {
            if (ImGui::BeginListBox("Completed", ImVec2(-1,-1))) {
                for (const auto &e: state.completed) {
                    ImGui::Text(e.c_str());
                }
                ImGui::EndListBox();
            }

            ImGui::EndTabItem();
        }

        tab_name = fmt::format("Ignores {:d}", state.ignores.size());
        if (ImGui::BeginTabItem(tab_name.c_str())) {
            if (ImGui::BeginListBox("Ignore", ImVec2(-1,-1))) {
                for (const auto &e: state.ignores) {
                    ImGui::Text(e.c_str());
                }
                ImGui::EndListBox();
            }

            ImGui::EndTabItem();
        }

        tab_name = fmt::format("Pending {:d}", state.pending.size());
        if (ImGui::BeginTabItem(tab_name.c_str())) {
            ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
            if (ImGui::BeginTable("Pending Table", 3, flags)) {

                ImGui::TableSetupColumn("");
                ImGui::TableSetupColumn("Target");
                ImGui::TableSetupColumn("Destination");
                ImGui::TableHeadersRow();

                for (auto &e: state.pending) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("", &e.active);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text(e.target.c_str());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text(e.dest.c_str());
                }

                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }

        tab_name = fmt::format("Deletes {:d}", state.deletes.size());
        if (ImGui::BeginTabItem(tab_name.c_str())) {
            if (ImGui::BeginListBox("Deletes", ImVec2(-1,-1))) {
                for (auto &e: state.deletes) {
                    ImGui::Checkbox(e.filename.c_str(), &e.active);
                }
                ImGui::EndListBox();
            }
            ImGui::EndTabItem();
        }

        tab_name = fmt::format("Conflicts {:d}", state.conflicts.size());
        if (ImGui::BeginTabItem(tab_name.c_str())) {
            RenderConflicts(state.conflicts);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void RenderConflicts(ConflictFiles &conflicts) {
    ImGuiTableFlags flags = 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | 
        ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings |
        ImGuiTableFlags_SizingStretchProp;

    for (auto &[dest, targets]: conflicts) {
        auto tree_label = fmt::format("{} ({:d})", dest.c_str(), targets.size());
        bool is_open = ImGui::CollapsingHeader(tree_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        if (is_open && ImGui::BeginTable("Table", 3, flags)) {
            ImGui::TableSetupColumn("");
            ImGui::TableSetupColumn("Target");
            ImGui::TableSetupColumn("Destination");
            ImGui::TableHeadersRow();

            for (auto &e: targets) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Checkbox("", &e.active);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text(e.target.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::Text(e.dest.c_str());
            }

            ImGui::EndTable();
        }
    }
}

};
