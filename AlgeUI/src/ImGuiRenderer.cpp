#include "AlgeUI/ImGuiRenderer.h"
#include "AlgeUI/Window.h"
#include "VulkanContext.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <vector>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

namespace AlgeUI {

static VkPipelineCache        s_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool       s_DescriptorPool = VK_NULL_HANDLE;
static ImGui_ImplVulkanH_Window s_MainWindowData{};
static int                    s_MinImageCount = 2;
static bool                   s_SwapChainRebuild = false;

// Per-frame-in-flight resources
static std::vector<std::vector<VkCommandBuffer>> s_AllocatedCommandBuffers;
static std::vector<std::vector<std::function<void()>>> s_ResourceFreeQueue;
static uint32_t             s_CurrentFrameIndex = 0;

extern void check_vk_result(VkResult err);

static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
    wd->Surface = surface;
    VkBool32 res{};
    vkGetPhysicalDeviceSurfaceSupportKHR(VulkanContext::GetPhysicalDevice(), VulkanContext::GetQueueFamily(), wd->Surface, &res);
    if (res != VK_TRUE)
        IM_ASSERT(false && "Error no WSI support on physical device");

    const VkFormat requestSurfaceImageFormat[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8G8B8_UNORM
    };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(VulkanContext::GetPhysicalDevice(), wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

#ifdef IMGUI_UNLIMITED_FRAME_RATE
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif

    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(VulkanContext::GetPhysicalDevice(), wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
    IM_ASSERT(s_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(VulkanContext::GetInstance(), VulkanContext::GetPhysicalDevice(), VulkanContext::GetDevice(), wd, VulkanContext::GetQueueFamily(), nullptr, width, height, s_MinImageCount);
}

static void CleanupVulkanWindow()
{
    ImGui_ImplVulkanH_DestroyWindow(VulkanContext::GetInstance(), VulkanContext::GetDevice(), &s_MainWindowData, nullptr);
}

static bool FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
    VkResult err;
    VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

    err = vkAcquireNextImageKHR(VulkanContext::GetDevice(), wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        s_SwapChainRebuild = true;
        return true;
    }
    check_vk_result(err);

    s_CurrentFrameIndex = (s_CurrentFrameIndex + 1) % s_MainWindowData.ImageCount;
    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];

    // Wait/Reset fence
    err = vkWaitForFences(VulkanContext::GetDevice(), 1, &fd->Fence, VK_TRUE, UINT64_MAX);
    check_vk_result(err);
    err = vkResetFences(VulkanContext::GetDevice(), 1, &fd->Fence);
    check_vk_result(err);

    // Run and clear resource free queue for this frame
    for (auto& func : s_ResourceFreeQueue[s_CurrentFrameIndex]) func();
    s_ResourceFreeQueue[s_CurrentFrameIndex].clear();

    // Reset and begin command buffer
    auto& allocatedCommandBuffers = s_AllocatedCommandBuffers[wd->FrameIndex];
    if (!allocatedCommandBuffers.empty())
    {
        vkFreeCommandBuffers(VulkanContext::GetDevice(), fd->CommandPool, static_cast<uint32_t>(allocatedCommandBuffers.size()), allocatedCommandBuffers.data());
        allocatedCommandBuffers.clear();
    }
    err = vkResetCommandPool(VulkanContext::GetDevice(), fd->CommandPool, 0);
    check_vk_result(err);

    VkCommandBufferBeginInfo info{}; info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &info); check_vk_result(err);

    VkRenderPassBeginInfo rp{}; rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO; rp.renderPass = wd->RenderPass; rp.framebuffer = fd->Framebuffer; rp.renderArea.extent.width = wd->Width; rp.renderArea.extent.height = wd->Height; rp.clearValueCount = 1; rp.pClearValues = &wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &rp, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    vkCmdEndRenderPass(fd->CommandBuffer);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{}; submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &image_acquired_semaphore; submit.pWaitDstStageMask = &wait_stage; submit.commandBufferCount = 1; submit.pCommandBuffers = &fd->CommandBuffer; submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &render_complete_semaphore;
    err = vkEndCommandBuffer(fd->CommandBuffer); check_vk_result(err);
    err = vkQueueSubmit(VulkanContext::GetGraphicsQueue(), 1, &submit, fd->Fence); check_vk_result(err);

    return false;
}

static bool FramePresent(ImGui_ImplVulkanH_Window* wd)
{
    if (s_SwapChainRebuild) return true;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info{}; info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR; info.waitSemaphoreCount = 1; info.pWaitSemaphores = &render_complete_semaphore; info.swapchainCount = 1; info.pSwapchains = &wd->Swapchain; info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(VulkanContext::GetGraphicsQueue(), &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) { s_SwapChainRebuild = true; return true; }
    check_vk_result(err);
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->ImageCount;
    return false;
}

void ImGuiRenderer::Init(Window* window)
{
    // Create Vulkan surface from window
    VkSurfaceKHR surface{};
    check_vk_result(window->CreateVulkanSurface(VulkanContext::GetInstance(), &surface));

    // Create descriptor pool
    {
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info{}; pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; pool_info.maxSets = 1000 * (uint32_t)IM_ARRAYSIZE(pool_sizes); pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes); pool_info.pPoolSizes = pool_sizes;
        VkResult err = vkCreateDescriptorPool(VulkanContext::GetDevice(), &pool_info, nullptr, &s_DescriptorPool); check_vk_result(err);
    }

    int w=0, h=0; window->GetFramebufferSize(&w, &h);
    SetupVulkanWindow(&s_MainWindowData, surface, w, h);

    s_AllocatedCommandBuffers.resize(s_MainWindowData.ImageCount);
    s_ResourceFreeQueue.resize(s_MainWindowData.ImageCount);

    // ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window->GetNativeWindow(), true);
    ImGui_ImplVulkan_InitInfo init{};
    init.Instance = VulkanContext::GetInstance();
    init.PhysicalDevice = VulkanContext::GetPhysicalDevice();
    init.Device = VulkanContext::GetDevice();
    init.QueueFamily = VulkanContext::GetQueueFamily();
    init.Queue = VulkanContext::GetGraphicsQueue();
    init.PipelineCache = s_PipelineCache;
    init.DescriptorPool = s_DescriptorPool;
    init.Subpass = 0;
    init.MinImageCount = s_MinImageCount;
    init.ImageCount = s_MainWindowData.ImageCount;
    init.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init.Allocator = nullptr;
    init.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init, s_MainWindowData.RenderPass);

    // Clear color from style
    {
        const ImVec4 clear = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        s_MainWindowData.ClearValue.color.float32[0] = clear.x;
        s_MainWindowData.ClearValue.color.float32[1] = clear.y;
        s_MainWindowData.ClearValue.color.float32[2] = clear.z;
        s_MainWindowData.ClearValue.color.float32[3] = clear.w;
    }

    // Upload fonts
    {
        VkCommandPool command_pool = s_MainWindowData.Frames[s_MainWindowData.FrameIndex].CommandPool;
        VkCommandBuffer command_buffer = s_MainWindowData.Frames[s_MainWindowData.FrameIndex].CommandBuffer;
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
}

void ImGuiRenderer::Shutdown()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow();
    vkDestroyDescriptorPool(VulkanContext::GetDevice(), s_DescriptorPool, nullptr);
}

void ImGuiRenderer::NewFrame(Window* /*window*/)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

bool ImGuiRenderer::Render()
{
    ImGui::Render();
    ImDrawData* main_draw_data = ImGui::GetDrawData();
    const bool minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);
    if (minimized)
        return false;

    if (FrameRender(&s_MainWindowData, main_draw_data)) return true;

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    if (FramePresent(&s_MainWindowData)) return true;
    return false;
}

bool ImGuiRenderer::IsSwapChainRebuilding()
{
    return s_SwapChainRebuild;
}

void ImGuiRenderer::RebuildSwapChainIfNeeded(Window* window)
{
    if (!s_SwapChainRebuild) return;
    int width=0, height=0; window->GetFramebufferSize(&width, &height);
    if (width > 0 && height > 0)
    {
        ImGui_ImplVulkanH_CreateOrResizeWindow(VulkanContext::GetInstance(), VulkanContext::GetPhysicalDevice(), VulkanContext::GetDevice(), &s_MainWindowData, VulkanContext::GetQueueFamily(), nullptr, width, height, s_MinImageCount);
        s_SwapChainRebuild = false;
    }
}

VkCommandBuffer ImGuiRenderer::GetCommandBuffer(bool begin)
{
    ImGui_ImplVulkanH_Window* wd = &s_MainWindowData;
    VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;

    VkCommandBufferAllocateInfo alloc{}; alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; alloc.commandPool = command_pool; alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; alloc.commandBufferCount = 1;
    VkCommandBuffer& cmd = s_AllocatedCommandBuffers[wd->FrameIndex].emplace_back();
    auto err = vkAllocateCommandBuffers(VulkanContext::GetDevice(), &alloc, &cmd); check_vk_result(err);

    if (begin)
    {
        VkCommandBufferBeginInfo begin_info{}; begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(cmd, &begin_info); check_vk_result(err);
    }

    return cmd;
}

void ImGuiRenderer::FlushCommandBuffer(VkCommandBuffer cmd)
{
    const uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000ULL;

    VkSubmitInfo submit{}; submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submit.commandBufferCount = 1; submit.pCommandBuffers = &cmd;
    auto err = vkEndCommandBuffer(cmd); check_vk_result(err);

    VkFenceCreateInfo fenceInfo{}; fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; VkFence fence{};
    err = vkCreateFence(VulkanContext::GetDevice(), &fenceInfo, nullptr, &fence); check_vk_result(err);

    err = vkQueueSubmit(VulkanContext::GetGraphicsQueue(), 1, &submit, fence); check_vk_result(err);
    err = vkWaitForFences(VulkanContext::GetDevice(), 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT); check_vk_result(err);

    vkDestroyFence(VulkanContext::GetDevice(), fence, nullptr);
}

void ImGuiRenderer::SubmitResourceFree(std::function<void()>&& func)
{
    s_ResourceFreeQueue[s_CurrentFrameIndex].emplace_back(std::move(func));
}

ImGui_ImplVulkanH_Window* ImGuiRenderer::GetMainWindowData()
{
    return &s_MainWindowData;
}

} // namespace AlgeUI
