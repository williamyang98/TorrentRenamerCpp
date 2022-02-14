#pragma once

void ImGuiSetupCustomConfig(void) {
    ImGuiStyle& style = ImGui::GetStyle();

    // border size
    style.WindowBorderSize      = 1.0f;
    style.ChildBorderSize       = 1.0f;
    style.PopupBorderSize       = 1.0f;
    style.FrameBorderSize       = 1.0f;
    style.TabBorderSize         = 1.0f;

    // rounding properties
    style.WindowRounding        = 4.0f;
    style.ChildRounding         = 4.0f;
    style.FrameRounding         = 4.0f;
    style.PopupRounding         = 4.0f;
    style.ScrollbarRounding     = 12.0f;
    style.GrabRounding          = 4.0f;
    style.LogSliderDeadzone     = 4.0f;
    style.TabRounding           = 4.0f;

    // theme color
    /*
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg]               = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.09f, 0.09f, 0.09f, 0.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.07f, 0.07f, 0.07f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.28f, 0.28f, 0.28f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.78f, 0.60f, 0.25f, 0.64f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.80f, 0.46f, 0.17f, 0.64f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.48f, 0.40f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.08f, 0.08f, 0.08f, 0.51f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.09f, 0.09f, 0.09f, 0.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.98f, 0.55f, 0.26f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.88f, 0.60f, 0.24f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.98f, 0.64f, 0.26f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.98f, 0.67f, 0.26f, 0.40f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.98f, 0.71f, 0.26f, 0.75f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.98f, 0.61f, 0.06f, 0.75f);
    colors[ImGuiCol_Header]                 = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.98f, 0.77f, 0.19f, 0.42f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.98f, 0.64f, 0.26f, 0.42f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.75f, 0.51f, 0.10f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.75f, 0.49f, 0.10f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.98f, 0.67f, 0.26f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.98f, 0.71f, 0.26f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.98f, 0.69f, 0.26f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.58f, 0.43f, 0.18f, 0.78f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.98f, 0.69f, 0.26f, 0.78f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.78f, 0.54f, 0.23f, 0.98f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.42f, 0.33f, 0.14f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.98f, 0.69f, 0.26f, 0.70f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.07f, 0.07f, 0.07f, 0.47f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.98f, 0.71f, 0.26f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 0.73f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.98f, 0.76f, 0.26f, 1.00f);
    */
}
