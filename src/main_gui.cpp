#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "app/app.h"

#include <imgui.h>
#include <GLFW/glfw3.h>
#include "gui/imgui_skeleton.h"
#include "gui/font_awesome_definitions.h"
#include "gui/imgui_config.h"
#include "gui/render_app.h"

// NOTE: This is required since we might want to open a file dialog using the win32 api
//       and those api calls require an event loop to be established
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <shobjidl.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#endif

// remove command window when in release
#if defined(_WIN32) && defined(NDEBUG)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

namespace fs = std::filesystem;

class Renderer: public ImguiSkeleton
{
private:
    app::App& app;
public:
    Renderer(app::App& _app): app(_app) {}
    virtual GLFWwindow* Create_GLFW_Window(void) {
        return glfwCreateWindow(
            1280, 720, 
            "TorrentRenamer", 
            NULL, NULL);
    }
    virtual void AfterImguiContextInit() {
        ImguiSkeleton::AfterImguiContextInit();
        auto& io = ImGui::GetIO();
        io.IniFilename =  "imgui.ini";
        io.Fonts->AddFontFromFileTTF("res/Roboto-Regular.ttf", 15.0f);
        {
            static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA };
            ImFontConfig icons_config;
            icons_config.MergeMode = true;
            icons_config.PixelSnapH = true;
            io.Fonts->AddFontFromFileTTF("res/font_awesome.ttf", 16.0f, &icons_config, icons_ranges);
        }
        ImGuiSetupCustomConfig();
    }

    virtual void Render() {
        app::gui::RenderApp(app);
    }
};

int main(int argc, const char** argv)
{
    auto logger = spdlog::basic_logger_mt("root", "logs.txt");
    spdlog::set_default_logger(logger);

    #if NDEBUG
    spdlog::set_level(spdlog::level::info);
    #else
    spdlog::set_level(spdlog::level::debug);
    #endif
    
    #if defined(_WIN32)
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    #endif
    
    const char* root_path = (argc > 1) ? argv[1] : NULL;
    app::App main_app("res/app_config.json");
    if (root_path == NULL) {
        main_app.queue_app_warning("Please specify a filepath as an argument when starting the application");
    } else {
        main_app.m_root = fs::path(root_path);
        main_app.refresh_folders();
    }
    auto renderer = Renderer(main_app);
    const int rv = RenderImguiSkeleton(&renderer);

    #if defined(_WIN32)
    CoUninitialize();
    #endif

    return rv;
}