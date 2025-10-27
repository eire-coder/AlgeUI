#include "AlgeUI/Application.h"

#include "VulkanContext.h"
#include "AlgeUI/ImGuiRenderer.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <stdio.h>
#include <stdlib.h>
#include <glm/glm.hpp>
#include <iostream>
#include <GLFW/glfw3.h>

#include "ImGui/Roboto-Regular.embed"
#include "AlgeUIIcon.embed"
#include "stb_image.h"
#include "AlgeUI/Input.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

static AlgeUI::Application* s_Instance = nullptr;

// Gideon theme: apply modern dark + red accent palette
static void ApplyGideonDarkRedTheme(ImGuiStyle& style)
{
    ImVec4 bg            = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    ImVec4 bgAlt         = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    ImVec4 panel         = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
    ImVec4 text          = ImVec4(0.92f, 0.92f, 0.95f, 1.0f);
    ImVec4 textDisabled  = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
    ImVec4 accent        = ImVec4(0.90f, 0.21f, 0.27f, 1.0f); // Gideon Red
    ImVec4 accentHover   = ImVec4(1.00f, 0.30f, 0.36f, 1.0f);
    ImVec4 accentActive  = ImVec4(0.78f, 0.16f, 0.22f, 1.0f);
    ImVec4 highlight     = ImVec4(0.22f, 0.22f, 0.26f, 1.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                 = text;
    colors[ImGuiCol_TextDisabled]         = textDisabled;
    colors[ImGuiCol_WindowBg]             = bg;
    colors[ImGuiCol_ChildBg]              = bgAlt;
    colors[ImGuiCol_PopupBg]              = panel;
    colors[ImGuiCol_Border]               = ImVec4(0, 0, 0, 0.40f);
    colors[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0.00f);

    colors[ImGuiCol_FrameBg]              = panel;
    colors[ImGuiCol_FrameBgHovered]       = highlight;
    colors[ImGuiCol_FrameBgActive]        = highlight;

    colors[ImGuiCol_TitleBg]              = bg;
    colors[ImGuiCol_TitleBgActive]        = bg;
    colors[ImGuiCol_TitleBgCollapsed]     = bg;

    colors[ImGuiCol_MenuBarBg]            = bgAlt;

    colors[ImGuiCol_ScrollbarBg]          = bg;
    colors[ImGuiCol_ScrollbarGrab]        = highlight;
    colors[ImGuiCol_ScrollbarGrabHovered] = highlight;
    colors[ImGuiCol_ScrollbarGrabActive]  = highlight;

    colors[ImGuiCol_CheckMark]            = accent;
    colors[ImGuiCol_SliderGrab]           = accent;
    colors[ImGuiCol_SliderGrabActive]     = accentActive;

    colors[ImGuiCol_Button]               = highlight;
    colors[ImGuiCol_ButtonHovered]        = accentHover;
    colors[ImGuiCol_ButtonActive]         = accentActive;

    colors[ImGuiCol_Header]               = highlight;
    colors[ImGuiCol_HeaderHovered]        = accentHover;
    colors[ImGuiCol_HeaderActive]         = accentActive;

    colors[ImGuiCol_Separator]            = highlight;
    colors[ImGuiCol_SeparatorHovered]     = accentHover;
    colors[ImGuiCol_SeparatorActive]      = accentActive;

    colors[ImGuiCol_ResizeGrip]           = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ResizeGripHovered]    = accentHover;
    colors[ImGuiCol_ResizeGripActive]     = accentActive;

    colors[ImGuiCol_Tab]                  = highlight;
    colors[ImGuiCol_TabHovered]           = accentHover;
    colors[ImGuiCol_TabActive]            = accentActive;
    colors[ImGuiCol_TabUnfocused]         = highlight;
    colors[ImGuiCol_TabUnfocusedActive]   = accent;

    colors[ImGuiCol_DockingPreview]       = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    colors[ImGuiCol_DockingEmptyBg]       = ImVec4(bg.x, bg.y, bg.z, 1.0f);

    colors[ImGuiCol_NavHighlight]         = accent;
    colors[ImGuiCol_NavWindowingHighlight]= ImVec4(1, 1, 1, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]    = ImVec4(0, 0, 0, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0, 0, 0, 0.35f);

    style.WindowPadding      = ImVec2(8, 8);
    style.FramePadding       = ImVec2(10, 6);
    style.ItemSpacing        = ImVec2(10, 8);
    style.ScrollbarSize      = 14.0f;
    style.GrabMinSize        = 10.0f;

    style.WindowBorderSize   = 1.0f;
    style.ChildBorderSize    = 1.0f;
    style.PopupBorderSize    = 1.0f;
    style.FrameBorderSize    = 1.0f;

    style.WindowRounding     = 6.0f;
    style.ChildRounding      = 6.0f;
    style.FrameRounding      = 6.0f;
    style.PopupRounding      = 6.0f;
    style.ScrollbarRounding  = 6.0f;
    style.GrabRounding       = 6.0f;
    style.TabRounding        = 6.0f;
}

void check_vk_result(VkResult err) {
    if (err == 0) return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0) abort();
}

namespace AlgeUI {

    Application::Application(const ApplicationSpecification& specification) : m_Specification(specification) {
        s_Instance = this;
        Init();
    }

    Application::~Application() {
        Shutdown();
        s_Instance = nullptr;
    }

    Application& Application::Get() {
        return *s_Instance;
    }

    void Application::Init() {
        WindowSpecification windowSpec;
        windowSpec.Title = m_Specification.Name;
        windowSpec.Width = m_Specification.Width;
        windowSpec.Height = m_Specification.Height;
        windowSpec.CustomTitleBar = m_Specification.CustomTitleBar;
        m_Window = std::make_unique<Window>(windowSpec);

        m_Window->SetIcon(g_AlgeUIIcon, g_AlgeUIIcon_len);

        m_VulkanContext = std::make_unique<VulkanContext>(m_Window->GetNativeWindow());

        // Setup Dear ImGui + Vulkan backends via renderer module
        ImGuiRenderer::Init(m_Window.get());

        // Theme
        ImGuiStyle& style = ImGui::GetStyle();
        ApplyGideonDarkRedTheme(style);
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Load default font & Upload Fonts (Roboto embedded)
        {
            ImGuiIO& io = ImGui::GetIO();
            ImFontConfig fontConfig; fontConfig.FontDataOwnedByAtlas = false;
            ImFont* robotoFont = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 20.0f, &fontConfig);
            io.FontDefault = robotoFont;

            // Upload fonts via renderer's window data
            ImGui_ImplVulkanH_Window* wd = ImGuiRenderer::GetMainWindowData();
            VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
            VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;
            VkResult err = vkResetCommandPool(VulkanContext::GetDevice(), command_pool, 0); check_vk_result(err);
            VkCommandBufferBeginInfo begin_info{}; begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(command_buffer, &begin_info); check_vk_result(err);
            ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
            VkSubmitInfo end_info{}; end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; end_info.commandBufferCount = 1; end_info.pCommandBuffers = &command_buffer;
            err = vkEndCommandBuffer(command_buffer); check_vk_result(err);
            err = vkQueueSubmit(VulkanContext::GetGraphicsQueue(), 1, &end_info, VK_NULL_HANDLE); check_vk_result(err);
            err = vkDeviceWaitIdle(VulkanContext::GetDevice()); check_vk_result(err);
            ImGui_ImplVulkan_DestroyFontUploadObjects();
        }

        // Load icon texture for title bar rendering
        {
            int width, height, channels;
            stbi_uc* pixels = stbi_load_from_memory(g_AlgeUIIcon, g_AlgeUIIcon_len, &width, &height, &channels, 4);
            if (pixels) {
                m_AppIcon = std::make_shared<Image>(width, height, ImageFormat::RGBA, pixels);
                stbi_image_free(pixels);
            }
        }
    }

    void Application::Shutdown() {
        for (auto& layer : m_LayerStack)
            layer->OnDetach();
        m_LayerStack.clear();

        m_AppIcon.reset();

        vkDeviceWaitIdle(VulkanContext::GetDevice());

        ImGuiRenderer::Shutdown();
    }

    void Application::Tick()
    {
        m_Window->PollEvents();

        // Rebuild swapchain if requested
        ImGuiRenderer::RebuildSwapChainIfNeeded(m_Window.get());

        for (auto& layer : m_LayerStack)
            layer->OnUpdate(m_TimeStep);

        ImGuiRenderer::NewFrame(m_Window.get());

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        ImGui::Begin("MainDockspaceContainer", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        const float titleBarHeight = 48.0f;

        if (m_Specification.CustomTitleBar)
        {
            const ImU32 bgCol      = ImGui::GetColorU32(ImGuiCol_TitleBgActive);
            const ImU32 accentCol  = ImGui::GetColorU32(ImVec4(0.90f, 0.21f, 0.27f, 1.0f));
            const ImU32 hoverBgCol = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered]);
            const ImU32 lineCol    = accentCol;

            // Title bar background and accents
            ImGui::GetWindowDrawList()->AddRectFilled(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + titleBarHeight), bgCol);
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(viewport->Pos.x, viewport->Pos.y), ImVec2(viewport->Pos.x + 3.0f, viewport->Pos.y + titleBarHeight), accentCol);
            ImGui::GetWindowDrawList()->AddLine(ImVec2(viewport->Pos.x, viewport->Pos.y + titleBarHeight - 1.0f), ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + titleBarHeight - 1.0f), lineCol, 1.0f);

            float verticalCenter = (titleBarHeight - ImGui::GetFrameHeight()) * 0.5f;
            ImGui::SetCursorPos(ImVec2(ImGui::GetStyle().WindowPadding.x + 8.0f, verticalCenter));
            if (m_AppIcon) {
                ImGui::Image(m_AppIcon->GetDescriptorSet(), ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
                ImGui::SameLine();
            }
            ImGui::SetCursorPosY(verticalCenter);
            if (m_TitleBarContentCallback) {
                m_TitleBarContentCallback();
            }
            float leftContentEndX = ImGui::GetCursorPosX();
            const float buttonSize = 20.0f;
            const float buttonAreaWidth = (buttonSize + ImGui::GetStyle().ItemSpacing.x) * 3;
            float rightContentStartX = ImGui::GetWindowWidth() - buttonAreaWidth;
            float availableWidthForTitle = rightContentStartX - leftContentEndX;
            ImVec2 titleSize = ImGui::CalcTextSize(m_Specification.Name.c_str());
            if (titleSize.x < availableWidthForTitle) {
                ImGui::SetCursorPosX(leftContentEndX + (availableWidthForTitle - titleSize.x) * 0.5f);
                ImGui::SetCursorPosY((titleBarHeight - titleSize.y) * 0.5f);
                ImGui::Text("%s", m_Specification.Name.c_str());
            }
            ImGui::SameLine(rightContentStartX);
            ImGui::SetCursorPosY((titleBarHeight - buttonSize) * 0.5f);
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // Minimize button
            ImGui::InvisibleButton("##minimize", ImVec2(buttonSize, buttonSize));
            s_ControlBox.Minimize = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
            if (ImGui::IsItemClicked()) glfwIconifyWindow(m_Window->GetNativeWindow());
            drawList->AddRectFilled(s_ControlBox.Minimize.Min, s_ControlBox.Minimize.Max, ImGui::IsItemHovered() ? hoverBgCol : 0);
            drawList->AddLine(ImVec2(s_ControlBox.Minimize.Min.x + 5, s_ControlBox.Minimize.GetCenter().y), ImVec2(s_ControlBox.Minimize.Max.x - 5, s_ControlBox.Minimize.GetCenter().y), ImGui::GetColorU32(ImGuiCol_Text), 1.f);
            ImGui::SameLine();

            // Maximize/Restore button
            ImGui::InvisibleButton("##maximize", ImVec2(buttonSize, buttonSize));
            s_ControlBox.Maximize = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
            if (ImGui::IsItemClicked()) { if (glfwGetWindowAttrib(m_Window->GetNativeWindow(), GLFW_MAXIMIZED)) glfwRestoreWindow(m_Window->GetNativeWindow()); else glfwMaximizeWindow(m_Window->GetNativeWindow()); }
            drawList->AddRectFilled(s_ControlBox.Maximize.Min, s_ControlBox.Maximize.Max, ImGui::IsItemHovered() ? hoverBgCol : 0);
            if (glfwGetWindowAttrib(m_Window->GetNativeWindow(), GLFW_MAXIMIZED)) { ImVec2 p = s_ControlBox.Maximize.Min; drawList->AddRect(ImVec2(p.x + 7, p.y + 5), ImVec2(p.x + 15, p.y + 13), ImGui::GetColorU32(ImGuiCol_Text), 0, 0, 1.f); drawList->AddRectFilled(ImVec2(p.x + 5, p.y + 7), ImVec2(p.x + 13, p.y + 15), bgCol); drawList->AddRect(ImVec2(p.x + 5, p.y + 7), ImVec2(p.x + 13, p.y + 15), ImGui::GetColorU32(ImGuiCol_Text), 0, 0, 1.f); }
            else { ImVec2 p = s_ControlBox.Maximize.Min; drawList->AddRect(ImVec2(p.x + 5, p.y + 5), ImVec2(p.x + 15, p.y + 15), ImGui::GetColorU32(ImGuiCol_Text), 0, 0, 1.f); }
            ImGui::SameLine();

            // Close button
            ImGui::InvisibleButton("##close", ImVec2(buttonSize, buttonSize));
            s_ControlBox.Close = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
            if (ImGui::IsItemClicked()) Close();
            drawList->AddRectFilled(s_ControlBox.Close.Min, s_ControlBox.Close.Max, ImGui::IsItemHovered() ? accentCol : 0);
            ImVec2 p = s_ControlBox.Close.Min; drawList->AddLine(ImVec2(p.x + 5, p.y + 5), ImVec2(p.x + 15, p.y + 15), ImGui::GetColorU32(ImGuiCol_Text), 1.f); drawList->AddLine(ImVec2(p.x + 15, p.y + 5), ImVec2(p.x + 5, p.y + 15), ImGui::GetColorU32(ImGuiCol_Text), 1.f);

            ImVec2 titleBarMin = viewport->Pos;
            ImVec2 titleBarMax = ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + titleBarHeight);
            if (ImGui::IsMouseHoveringRect(titleBarMin, titleBarMax) &&
                ImGui::IsMouseClicked(0) &&
                !ImGui::IsAnyItemHovered() &&
                !ImGui::IsAnyItemActive())
            {
                ImGui::ClearActiveID();
                ReleaseCapture();
                PostMessage(glfwGetWin32Window(GetWindowHandle()), WM_SYSCOMMAND, SC_MOVE | 2, 0);
            }
            s_TitleBarHovered = ImGui::IsMouseHoveringRect(titleBarMin, titleBarMax) && !ImGui::IsAnyItemHovered();
        }

        float contentAreaY = m_Specification.CustomTitleBar ? titleBarHeight : 0;
        ImGui::SetCursorPos(ImVec2(0, contentAreaY));

        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGui::DockSpace(ImGui::GetID("VulkanAppDockspace"), ImVec2(0.0f, ImGui::GetContentRegionAvail().y), dockspace_flags);

        for (auto& layer : m_LayerStack)
            layer->OnUIRender();

        ImGui::End();

        // Render & present via renderer module (may request swapchain rebuild)
        if (ImGuiRenderer::Render())
            return; // skip frame if swapchain needs rebuild

        float time = GetTime(); m_FrameTime = time - m_LastFrameTime; m_TimeStep = glm::min<float>(m_FrameTime, 0.0333f); m_LastFrameTime = time;
        if (m_Window->ShouldClose()) m_Running = false;
    }

    void Application::Run() {
        m_Running = true;
        while (m_Running) {
            Tick();
        }
    }

    void Application::Close() {
        m_Running = false;
    }

    float Application::GetTime() {
        return (float)glfwGetTime();
    }

    VkInstance Application::GetInstance() { return VulkanContext::GetInstance(); }
    VkPhysicalDevice Application::GetPhysicalDevice() { return VulkanContext::GetPhysicalDevice(); }
    VkDevice Application::GetDevice() { return VulkanContext::GetDevice(); }

    VkCommandBuffer Application::GetCommandBuffer(bool begin) { return ImGuiRenderer::GetCommandBuffer(begin); }
    void Application::FlushCommandBuffer(VkCommandBuffer commandBuffer) { ImGuiRenderer::FlushCommandBuffer(commandBuffer); }
    void Application::SubmitResourceFree(std::function<void()>&& func) { ImGuiRenderer::SubmitResourceFree(std::move(func)); }
}