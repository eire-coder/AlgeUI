#pragma once

#include "vulkan/vulkan.h"

// Forward-declare from GLFW
struct GLFWwindow;

namespace AlgeUI {

	class VulkanContext
	{
	public:
		VulkanContext(GLFWwindow* windowHandle);
		~VulkanContext();

		static VkInstance GetInstance() { return s_Instance; }
		static VkDevice GetDevice() { return s_Device; }
		static VkPhysicalDevice GetPhysicalDevice() { return s_PhysicalDevice; }
		static VkQueue GetGraphicsQueue() { return s_GraphicsQueue; }
		static uint32_t GetQueueFamily() { return s_QueueFamily; }

	private:
		void SetupVulkan(GLFWwindow* windowHandle);
		void CleanupVulkan();

	private:
		// All Vulkan handles are now static members of the context.
		// This maintains the ability to access them globally within the library
		// via VulkanContext::Get...() instead of Application::Get...()
		inline static VkInstance s_Instance = VK_NULL_HANDLE;
		inline static VkDevice s_Device = VK_NULL_HANDLE;
		inline static VkPhysicalDevice s_PhysicalDevice = VK_NULL_HANDLE;
		inline static VkQueue s_GraphicsQueue = VK_NULL_HANDLE;
		inline static VkDebugReportCallbackEXT s_DebugReport = VK_NULL_HANDLE;

		inline static uint32_t s_QueueFamily = (uint32_t)-1;
	};

}