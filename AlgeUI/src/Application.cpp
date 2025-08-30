#include "AlgeUI/Application.h"
#include "VulkanContext.h"

//
// Adapted from Dear ImGui Vulkan example
//

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include <stdio.h>
#include <stdlib.h>
#include <glm/glm.hpp>
#include <iostream>

#include <GLFW/glfw3.h>

// Emedded font
#include "ImGui/Roboto-Regular.embed"

// Embed the icon
#include "AlgeUIIcon.embed"
#include "stb_image.h"


#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// Global Vulkan variables that are specific to the swapchain/window
// These will be moved to a dedicated Swapchain class next.
static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static int                      g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;

// Per-frame-in-flight resources
static std::vector<std::vector<VkCommandBuffer>> s_AllocatedCommandBuffers;
static std::vector<std::vector<std::function<void()>>> s_ResourceFreeQueue;
static uint32_t s_CurrentFrameIndex = 0;

static AlgeUI::Application* s_Instance = nullptr;

// We will keep these functions here for now, as they will be moved into the SwapChain class.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height);
static void CleanupVulkanWindow();
static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data);
static void FramePresent(ImGui_ImplVulkanH_Window* wd);


void check_vk_result(VkResult err)
{
	if (err == 0) return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0) abort();
}

namespace AlgeUI {

	Application::Application(const ApplicationSpecification& specification)
		: m_Specification(specification)
	{
		s_Instance = this;
		Init();
	}

	Application::~Application()
	{
		Shutdown();
		s_Instance = nullptr;
	}

	Application& Application::Get()
	{
		return *s_Instance;
	}

	void Application::Init()
	{
		// 1. Create the window using a correctly populated WindowSpecification
		WindowSpecification windowSpec;
		windowSpec.Title = m_Specification.Name;
		windowSpec.Width = m_Specification.Width;
		windowSpec.Height = m_Specification.Height;
		m_Window = std::make_unique<Window>(windowSpec);

		// Set the native window icon
		m_Window->SetIcon(g_AlgeUIIcon, g_AlgeUIIcon_len);

		// 2. Create the Vulkan context, which needs the window handle
		m_VulkanContext = std::make_unique<VulkanContext>(m_Window->GetNativeWindow());

		// 3. Create the Vulkan window surface
		VkSurfaceKHR surface;
		check_vk_result(m_Window->CreateVulkanSurface(VulkanContext::GetInstance(), &surface));

		// Create Descriptor Pool
		{
			VkDescriptorPoolSize pool_sizes[] =
			{
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
			VkDescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
			pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
			pool_info.pPoolSizes = pool_sizes;
			VkResult err = vkCreateDescriptorPool(VulkanContext::GetDevice(), &pool_info, nullptr, &g_DescriptorPool);
			check_vk_result(err);
		}


		// Create Framebuffers
		int w, h;
		m_Window->GetFramebufferSize(&w, &h);
		ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
		SetupVulkanWindow(wd, surface, w, h);

		s_AllocatedCommandBuffers.resize(wd->ImageCount);
		s_ResourceFreeQueue.resize(wd->ImageCount);

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForVulkan(m_Window->GetNativeWindow(), true);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = VulkanContext::GetInstance();
		init_info.PhysicalDevice = VulkanContext::GetPhysicalDevice();
		init_info.Device = VulkanContext::GetDevice();
		init_info.QueueFamily = VulkanContext::GetQueueFamily();
		init_info.Queue = VulkanContext::GetGraphicsQueue();
		init_info.PipelineCache = g_PipelineCache;
		init_info.DescriptorPool = g_DescriptorPool;
		init_info.Subpass = 0;
		init_info.MinImageCount = g_MinImageCount;
		init_info.ImageCount = wd->ImageCount;
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.Allocator = nullptr;
		init_info.CheckVkResultFn = check_vk_result;
		ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

		// Load icon for rendering in title bar
		{
			int width, height, channels;
			stbi_uc* pixels = stbi_load_from_memory(g_AlgeUIIcon, g_AlgeUIIcon_len, &width, &height, &channels, 4);
			if (pixels)
			{
				m_AppIcon = std::make_shared<Image>(width, height, ImageFormat::RGBA, pixels);
				stbi_image_free(pixels);
			}
		}

		// Load default font & Upload Fonts
		ImFontConfig fontConfig;
		fontConfig.FontDataOwnedByAtlas = false;
		ImFont* robotoFont = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 20.0f, &fontConfig);
		io.FontDefault = robotoFont;
		{
			VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
			VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;
			VkResult err = vkResetCommandPool(VulkanContext::GetDevice(), command_pool, 0);
			check_vk_result(err);
			VkCommandBufferBeginInfo begin_info = {};
			begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			err = vkBeginCommandBuffer(command_buffer, &begin_info);
			check_vk_result(err);
			ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
			VkSubmitInfo end_info = {};
			end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			end_info.commandBufferCount = 1;
			end_info.pCommandBuffers = &command_buffer;
			err = vkEndCommandBuffer(command_buffer);
			check_vk_result(err);
			err = vkQueueSubmit(VulkanContext::GetGraphicsQueue(), 1, &end_info, VK_NULL_HANDLE);
			check_vk_result(err);
			err = vkDeviceWaitIdle(VulkanContext::GetDevice());
			check_vk_result(err);
			ImGui_ImplVulkan_DestroyFontUploadObjects();
		}
	}

	void Application::Shutdown()
	{
		for (auto& layer : m_LayerStack)
			layer->OnDetach();
		m_LayerStack.clear();

		// Clear the icon pointer
		m_AppIcon.reset();

		vkDeviceWaitIdle(VulkanContext::GetDevice());

		for (auto& queue : s_ResourceFreeQueue)
		{
			for (auto& func : queue)
				func();
		}
		s_ResourceFreeQueue.clear();

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		CleanupVulkanWindow();
		vkDestroyDescriptorPool(VulkanContext::GetDevice(), g_DescriptorPool, nullptr);
	}

	void Application::Run()
	{
		m_Running = true;

		ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		ImGuiIO& io = ImGui::GetIO();

		while (!m_Window->ShouldClose() && m_Running)
		{
			m_Window->PollEvents();

			for (auto& layer : m_LayerStack)
				layer->OnUpdate(m_TimeStep);

			if (g_SwapChainRebuild)
			{
				int width, height;
				m_Window->GetFramebufferSize(&width, &height);
				if (width > 0 && height > 0)
				{
					ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
					ImGui_ImplVulkanH_CreateOrResizeWindow(VulkanContext::GetInstance(), VulkanContext::GetPhysicalDevice(), VulkanContext::GetDevice(), &g_MainWindowData, VulkanContext::GetQueueFamily(), nullptr, width, height, g_MinImageCount);
					g_MainWindowData.FrameIndex = 0;
					s_AllocatedCommandBuffers.clear();
					s_AllocatedCommandBuffers.resize(g_MainWindowData.ImageCount);
					g_SwapChainRebuild = false;
				}
			}

			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			{
				static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
				ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

				const ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImGui::SetNextWindowPos(viewport->WorkPos);
				ImGui::SetNextWindowSize(viewport->WorkSize);
				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
				window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
				window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

				if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
					window_flags |= ImGuiWindowFlags_NoBackground;

				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				ImGui::Begin("DockSpace Demo", nullptr, window_flags);
				ImGui::PopStyleVar();

				ImGui::PopStyleVar(2);

				const ImVec4& windowBgColor = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
				ImGui::PushStyleColor(ImGuiCol_ChildBg, windowBgColor);
				const float titleBarHeight = ImGui::GetFrameHeight() * 2.5f;
				ImGui::BeginChild("##TitleBar", ImVec2(0, titleBarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
				ImGui::PopStyleColor();

				float titlePaddingY = (titleBarHeight - ImGui::GetTextLineHeight()) * 0.5f;
				float iconSize = titleBarHeight - titlePaddingY * 1.5f;
				ImGui::SetCursorPos(ImVec2(10.0f, (titleBarHeight - iconSize) * 0.5f));

				if (m_AppIcon)
				{
					ImGui::Image(m_AppIcon->GetDescriptorSet(), ImVec2(iconSize, iconSize));
					ImGui::SameLine();
					ImGui::SetCursorPosY(titlePaddingY + (iconSize - ImGui::GetTextLineHeight()) * 0.5f);
				}
				ImGui::Text("%s", m_Specification.Name.c_str());

				const ImVec4 grayTextColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
				const ImVec4 whiteTextColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

				const float buttonHeight = ImGui::GetFrameHeight();
				const float buttonWidth = buttonHeight * 1.5f;
				const float buttonPaddingY = (titleBarHeight - buttonHeight) * 0.5f;
				const float horizontalPadding = 10.0f;
				const float buttonSpacing = 5.0f;

				ImDrawList* drawList = ImGui::GetWindowDrawList();

				ImGui::SameLine(ImGui::GetWindowWidth() - (buttonWidth * 3) - (buttonSpacing * 2) - horizontalPadding);
				ImGui::SetCursorPosY(buttonPaddingY);

				ImGui::InvisibleButton("##minimize", ImVec2(buttonWidth, buttonHeight));
				if (ImGui::IsItemClicked()) { 
					glfwIconifyWindow(m_Window->GetNativeWindow()); 
				}

				s_ControlBox.Minimize = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };

				ImVec2 btnMin = ImGui::GetItemRectMin();
				ImVec2 btnMax = ImGui::GetItemRectMax();
				ImVec2 textSize = ImGui::CalcTextSize(" _ ");
				ImU32 textColor = ImGui::GetColorU32(ImGui::IsItemHovered() ? whiteTextColor : grayTextColor);
				drawList->AddText(ImVec2(btnMin.x + (buttonWidth - textSize.x) * 0.5f, btnMin.y + (buttonHeight - textSize.y) * 0.5f - 2.0f), textColor, " _ ");

				ImGui::SameLine(0, buttonSpacing);
				ImGui::InvisibleButton("##maximize", ImVec2(buttonWidth, buttonHeight));
				if (ImGui::IsItemClicked()) {
					if (glfwGetWindowAttrib(m_Window->GetNativeWindow(), GLFW_MAXIMIZED))
						glfwRestoreWindow(m_Window->GetNativeWindow());
					else
						glfwMaximizeWindow(m_Window->GetNativeWindow());
				}

				s_ControlBox.Maximize = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };

				btnMin = ImGui::GetItemRectMin();
				btnMax = ImGui::GetItemRectMax();
				ImU32 iconColor = ImGui::GetColorU32(ImGui::IsItemHovered() ? whiteTextColor : grayTextColor);
				float iconHeight = buttonHeight * 0.5f;
				float iconWidth = iconHeight;
				float iconPosX = btnMin.x + (buttonWidth - iconWidth) * 0.5f;
				float iconPosY = btnMin.y + (buttonHeight - iconHeight) * 0.5f;

				if (glfwGetWindowAttrib(m_Window->GetNativeWindow(), GLFW_MAXIMIZED))
				{
					float restoreOffset = 2.0f;
					drawList->AddRect(
						ImVec2(iconPosX + restoreOffset, iconPosY - restoreOffset),
						ImVec2(iconPosX + iconWidth + restoreOffset, iconPosY + iconHeight - restoreOffset),
						iconColor, 0.0f, 0, 1.5f);
					drawList->AddRectFilled(btnMin, btnMax, ImGui::GetColorU32(clear_color));
					drawList->AddRect(
						ImVec2(iconPosX, iconPosY),
						ImVec2(iconPosX + iconWidth, iconPosY + iconHeight),
						iconColor, 0.0f, 0, 1.5f);
				}
				else
				{
					drawList->AddRect(
						ImVec2(iconPosX, iconPosY),
						ImVec2(iconPosX + iconWidth, iconPosY + iconHeight),
						iconColor, 0.0f, 0, 1.5f);
				}

				ImGui::SameLine(0, buttonSpacing);
				ImGui::InvisibleButton("##close", ImVec2(buttonWidth, buttonHeight));
				if (ImGui::IsItemClicked()) { Close(); }

				s_ControlBox.Close = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };

				btnMin = ImGui::GetItemRectMin();
				btnMax = ImGui::GetItemRectMax();
				textSize = ImGui::CalcTextSize(" X ");
				textColor = ImGui::GetColorU32(ImGui::IsItemHovered() ? whiteTextColor : grayTextColor);
				drawList->AddText(ImVec2(btnMin.x + (buttonWidth - textSize.x) * 0.5f, btnMin.y + (buttonHeight - textSize.y) * 0.5f), textColor, " X ");


				ImGui::EndChild();

				if (m_MenubarCallback)
				{
					if (ImGui::BeginMenuBar())
					{
						m_MenubarCallback();
						ImGui::EndMenuBar();
					}
				}

				ImGuiIO& io = ImGui::GetIO();
				if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
				{
					ImGuiID dockspace_id = ImGui::GetID("VulkanAppDockspace");
					ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
				}

				for (auto& layer : m_LayerStack)
					layer->OnUIRender();

				ImGui::End();
			}

			// Rendering
			ImGui::Render();
			ImDrawData* main_draw_data = ImGui::GetDrawData();
			const bool main_is_minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);
			wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
			wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
			wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
			wd->ClearValue.color.float32[3] = clear_color.w;
			if (!main_is_minimized)
				FrameRender(wd, main_draw_data);

			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}



			if (!main_is_minimized)
				FramePresent(wd);

			float time = GetTime();
			m_FrameTime = time - m_LastFrameTime;
			m_TimeStep = glm::min<float>(m_FrameTime, 0.0333f);
			m_LastFrameTime = time;
		}
	}

	void Application::Close()
	{
		m_Running = false;
	}

	float Application::GetTime()
	{
		return (float)glfwGetTime();
	}

	VkInstance Application::GetInstance() { return VulkanContext::GetInstance(); }
	VkPhysicalDevice Application::GetPhysicalDevice() { return VulkanContext::GetPhysicalDevice(); }
	VkDevice Application::GetDevice() { return VulkanContext::GetDevice(); }

	VkCommandBuffer Application::GetCommandBuffer(bool begin)
	{
		ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
		VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
		VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
		cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufAllocateInfo.commandPool = command_pool;
		cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufAllocateInfo.commandBufferCount = 1;
		VkCommandBuffer& command_buffer = s_AllocatedCommandBuffers[wd->FrameIndex].emplace_back();
		auto err = vkAllocateCommandBuffers(GetDevice(), &cmdBufAllocateInfo, &command_buffer);
		check_vk_result(err);

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(command_buffer, &begin_info);
		check_vk_result(err);

		return command_buffer;
	}

	void Application::FlushCommandBuffer(VkCommandBuffer commandBuffer)
	{
		const uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;

		VkSubmitInfo end_info = {};
		end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		end_info.commandBufferCount = 1;
		end_info.pCommandBuffers = &commandBuffer;
		auto err = vkEndCommandBuffer(commandBuffer);
		check_vk_result(err);

		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VkFence fence;
		err = vkCreateFence(GetDevice(), &fenceCreateInfo, nullptr, &fence);
		check_vk_result(err);

		err = vkQueueSubmit(VulkanContext::GetGraphicsQueue(), 1, &end_info, fence);
		check_vk_result(err);

		err = vkWaitForFences(GetDevice(), 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
		check_vk_result(err);

		vkDestroyFence(GetDevice(), fence, nullptr);
	}

	void Application::SubmitResourceFree(std::function<void()>&& func)
	{
		s_ResourceFreeQueue[s_CurrentFrameIndex].emplace_back(std::move(func));
	}
}

static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
	wd->Surface = surface;
	VkBool32 res;
	vkGetPhysicalDeviceSurfaceSupportKHR(AlgeUI::VulkanContext::GetPhysicalDevice(), AlgeUI::VulkanContext::GetQueueFamily(), wd->Surface, &res);
	if (res != VK_TRUE)
	{
		fprintf(stderr, "Error no WSI support on physical device 0\n");
		exit(-1);
	}
	const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
	const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(AlgeUI::VulkanContext::GetPhysicalDevice(), wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

#ifdef IMGUI_UNLIMITED_FRAME_RATE
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif

	wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(AlgeUI::VulkanContext::GetPhysicalDevice(), wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
	IM_ASSERT(g_MinImageCount >= 2);
	ImGui_ImplVulkanH_CreateOrResizeWindow(AlgeUI::VulkanContext::GetInstance(), AlgeUI::VulkanContext::GetPhysicalDevice(), AlgeUI::VulkanContext::GetDevice(), wd, AlgeUI::VulkanContext::GetQueueFamily(), nullptr, width, height, g_MinImageCount);
}

static void CleanupVulkanWindow()
{
	ImGui_ImplVulkanH_DestroyWindow(AlgeUI::VulkanContext::GetInstance(), AlgeUI::VulkanContext::GetDevice(), &g_MainWindowData, nullptr);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
	VkResult err;
	VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
	err = vkAcquireNextImageKHR(AlgeUI::VulkanContext::GetDevice(), wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		g_SwapChainRebuild = true;
		return;
	}
	check_vk_result(err);
	s_CurrentFrameIndex = (s_CurrentFrameIndex + 1) % g_MainWindowData.ImageCount;
	ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
	{
		err = vkWaitForFences(AlgeUI::VulkanContext::GetDevice(), 1, &fd->Fence, VK_TRUE, UINT64_MAX);
		check_vk_result(err);
		err = vkResetFences(AlgeUI::VulkanContext::GetDevice(), 1, &fd->Fence);
		check_vk_result(err);
	}
	{
		for (auto& func : s_ResourceFreeQueue[s_CurrentFrameIndex])
			func();
		s_ResourceFreeQueue[s_CurrentFrameIndex].clear();
	}
	{
		auto& allocatedCommandBuffers = s_AllocatedCommandBuffers[wd->FrameIndex];
		if (allocatedCommandBuffers.size() > 0)
		{
			vkFreeCommandBuffers(AlgeUI::VulkanContext::GetDevice(), fd->CommandPool, static_cast<uint32_t>(allocatedCommandBuffers.size()), allocatedCommandBuffers.data());
			allocatedCommandBuffers.clear();
		}
		err = vkResetCommandPool(AlgeUI::VulkanContext::GetDevice(), fd->CommandPool, 0);
		check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
		check_vk_result(err);
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = wd->RenderPass;
		info.framebuffer = fd->Framebuffer;
		info.renderArea.extent.width = wd->Width;
		info.renderArea.extent.height = wd->Height;
		info.clearValueCount = 1;
		info.pClearValues = &wd->ClearValue;
		vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}
	ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);
	vkCmdEndRenderPass(fd->CommandBuffer);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &image_acquired_semaphore;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &fd->CommandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &render_complete_semaphore;
		err = vkEndCommandBuffer(fd->CommandBuffer);
		check_vk_result(err);
		err = vkQueueSubmit(AlgeUI::VulkanContext::GetGraphicsQueue(), 1, &info, fd->Fence);
		check_vk_result(err);
	}
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd)
{
	if (g_SwapChainRebuild)
		return;
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &render_complete_semaphore;
	info.swapchainCount = 1;
	info.pSwapchains = &wd->Swapchain;
	info.pImageIndices = &wd->FrameIndex;
	VkResult err = vkQueuePresentKHR(AlgeUI::VulkanContext::GetGraphicsQueue(), &info);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		g_SwapChainRebuild = true;
		return;
	}
	check_vk_result(err);
	wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->ImageCount;
}