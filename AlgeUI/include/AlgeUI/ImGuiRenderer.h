#pragma once

#include <vulkan/vulkan.h>
#include <functional>

struct ImGui_ImplVulkanH_Window; // forward from imgui_impl_vulkan.h

namespace AlgeUI {
class Window;

class ImGuiRenderer {
public:
    // Initialize ImGui + Vulkan backend and create swapchain for the provided window
    static void Init(Window* window);
    static void Shutdown();

    // Per-frame
    static void NewFrame(Window* window);
    // Returns true if swapchain was rebuilt and caller should skip the frame
    static bool Render();

    // Resize/rebuild helpers
    static bool IsSwapChainRebuilding();
    static void RebuildSwapChainIfNeeded(Window* window);

    // Helpers for transient command buffer usage + cleanup queues
    static VkCommandBuffer GetCommandBuffer(bool begin);
    static void FlushCommandBuffer(VkCommandBuffer cmd);
    static void SubmitResourceFree(std::function<void()>&& func);

    static ImGui_ImplVulkanH_Window* GetMainWindowData();
};

}
