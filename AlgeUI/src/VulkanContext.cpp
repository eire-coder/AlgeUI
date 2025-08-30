
#include "VulkanContext.h"
#include "AlgeUI/Application.h" // For check_vk_result

#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>

#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

namespace AlgeUI {

	// Forward-declare the debug report function
#ifdef IMGUI_VULKAN_DEBUG_REPORT
	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData);
#endif

	VulkanContext::VulkanContext(GLFWwindow* windowHandle)
	{
		SetupVulkan(windowHandle);
	}

	VulkanContext::~VulkanContext()
	{
		CleanupVulkan();
	}

	void VulkanContext::SetupVulkan(GLFWwindow* windowHandle)
	{
		VkResult err;

		// Get required extensions from GLFW
		uint32_t extensions_count = 0;
		const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);

		// Create Vulkan Instance
		{
			VkInstanceCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			create_info.enabledExtensionCount = extensions_count;
			create_info.ppEnabledExtensionNames = extensions;

#ifdef IMGUI_VULKAN_DEBUG_REPORT
			// Enabling validation layers
			const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
			create_info.enabledLayerCount = 1;
			create_info.ppEnabledLayerNames = layers;

			// Enable debug report extension
			std::vector<const char*> extensions_ext(extensions, extensions + extensions_count);
			extensions_ext.push_back("VK_EXT_debug_report");

			create_info.enabledExtensionCount = (uint32_t)extensions_ext.size();
			create_info.ppEnabledExtensionNames = extensions_ext.data();

			err = vkCreateInstance(&create_info, nullptr, &s_Instance);
			check_vk_result(err);

			// Setup the debug report callback
			auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(s_Instance, "vkCreateDebugReportCallbackEXT");
			VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
			debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
			debug_report_ci.pfnCallback = debug_report;
			err = vkCreateDebugReportCallbackEXT(s_Instance, &debug_report_ci, nullptr, &s_DebugReport);
			check_vk_result(err);
#else
			// Create Vulkan Instance without any debug feature
			err = vkCreateInstance(&create_info, nullptr, &s_Instance);
			check_vk_result(err);
#endif
		}

		// Select GPU
		{
			uint32_t gpu_count;
			vkEnumeratePhysicalDevices(s_Instance, &gpu_count, NULL);
			std::vector<VkPhysicalDevice> gpus(gpu_count);
			vkEnumeratePhysicalDevices(s_Instance, &gpu_count, gpus.data());

			int use_gpu = 0;
			for (int i = 0; i < (int)gpu_count; i++)
			{
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(gpus[i], &properties);
				if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					use_gpu = i;
					break;
				}
			}
			s_PhysicalDevice = gpus[use_gpu];
		}

		// Select graphics queue family
		{
			uint32_t count;
			vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &count, NULL);
			std::vector<VkQueueFamilyProperties> queues(count);
			vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &count, queues.data());
			for (uint32_t i = 0; i < count; i++)
			{
				if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					s_QueueFamily = i;
					break;
				}
			}
		}

		// Create Logical Device (with 1 queue)
		{
			const char* device_extensions[] = { "VK_KHR_swapchain" };
			const float queue_priority[] = { 1.0f };
			VkDeviceQueueCreateInfo queue_info[1] = {};
			queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_info[0].queueFamilyIndex = s_QueueFamily;
			queue_info[0].queueCount = 1;
			queue_info[0].pQueuePriorities = queue_priority;
			VkDeviceCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
			create_info.pQueueCreateInfos = queue_info;
			create_info.enabledExtensionCount = 1;
			create_info.ppEnabledExtensionNames = device_extensions;
			err = vkCreateDevice(s_PhysicalDevice, &create_info, nullptr, &s_Device);
			check_vk_result(err);
			vkGetDeviceQueue(s_Device, s_QueueFamily, 0, &s_GraphicsQueue);
		}
	}


	void VulkanContext::CleanupVulkan()
	{
#ifdef IMGUI_VULKAN_DEBUG_REPORT
		auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(s_Instance, "vkDestroyDebugReportCallbackEXT");
		vkDestroyDebugReportCallbackEXT(s_Instance, s_DebugReport, nullptr);
#endif

		vkDestroyDevice(s_Device, nullptr);
		vkDestroyInstance(s_Instance, nullptr);
	}


#ifdef IMGUI_VULKAN_DEBUG_REPORT
	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
	{
		(void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
		fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
		return VK_FALSE;
	}
#endif
}