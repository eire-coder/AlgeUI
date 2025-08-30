#pragma once

#include <string>
#include <functional>

#include "vulkan/vulkan.h"

// Forward-declare GLFWwindow to avoid including the GLFW header here
struct GLFWwindow;

// Platform-specific includes and definitions for Windows
#ifdef WL_PLATFORM_WINDOWS
	// These definitions prevent the heavy Windows.h header from including unnecessary APIs
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace AlgeUI {

	struct WindowSpecification
	{
		std::string Title = "AlgeUI";
		uint32_t Width = 1600;
		uint32_t Height = 900;
	};

	class Window
	{
	public:
		Window(const WindowSpecification& spec);
		~Window();

		void PollEvents();
		bool ShouldClose();

		VkResult CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface);
		void SetIcon(const unsigned char* data, int len);

		GLFWwindow* GetNativeWindow() const { return m_WindowHandle; }
		uint32_t GetWidth() const { return m_Data.Width; }
		uint32_t GetHeight() const { return m_Data.Height; }
		void GetFramebufferSize(int* width, int* height);

	private:
		void Init();
		void Shutdown();

	private:
		GLFWwindow* m_WindowHandle = nullptr;

		struct WindowData
		{
			std::string Title;
			uint32_t Width;
			uint32_t Height;
		};

		WindowData m_Data;

#ifdef WL_PLATFORM_WINDOWS
		HWND m_NativeHandle = nullptr; // HWND handle
		// Static callback requires a static pointer to the original procedure
		inline static WNDPROC s_OriginalWndProc = nullptr;
		static LRESULT CALLBACK Win32WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
	};

}