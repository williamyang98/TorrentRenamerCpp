#pragma once

struct GLFWwindow;

// Uses the template pattern to help construct a generic imgui app
class ImguiSkeleton 
{
public:
    // Before creating the window
    // Can be used to set the GLFW hints
    virtual void BeforeGLFWInit();
    // Create a new GLFWwindow instance
    virtual GLFWwindow* Create_GLFW_Window();
    virtual void AfterGLFWInit();
    // Occurs after Imgui::CreateContext
    virtual void AfterImguiContextInit();
    virtual void Render() = 0;
    // On shutdown of app
    virtual void AfterShutdown();
};

int RenderImguiSkeleton(ImguiSkeleton* runner);