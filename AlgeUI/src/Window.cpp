#include "AlgeUI/Window.h"
#include "AlgeUI/Application.h" // Must include for the callback logic

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include "stb_image.h"

#ifdef WL_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace AlgeUI {

	static void glfw_error_callback(int error, const char* description)
	{
		fprintf(stderr, "GLFW Error %d: %s\n", error, description);
	}

#ifdef WL_PLATFORM_WINDOWS
	// This is our new, clean hit-test callback that integrates perfectly with GLFW.
	// It receives coordinates in CLIENT space, which is what we need for ImGui.
	static int HitTestCallback(GLFWwindow* window, int x, int y)
	{
		Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
		if (!app)
		{
			return -1; // Fallback to default if app isn't set yet
		}

		if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED))
		{
			// When maximized, we only care about the control box. Let the OS handle the rest.
			const auto& controlBox = app->GetControlBox();
			const ImVec2 mousePos = { (float)x, (float)y };
			if (controlBox.Minimize.Contains(mousePos) ||
				controlBox.Maximize.Contains(mousePos) ||
				controlBox.Close.Contains(mousePos))
			{
				return HTCLIENT; // Let ImGui handle clicks on buttons
			}
			return HTCAPTION; // The rest of the title bar is for dragging to restore
		}

		// --- BORDER CHECKS (HIGHEST PRIORITY) ---
		const LONG border_width = 8;
		int width, height;
		glfwGetWindowSize(window, &width, &height);

		bool on_left = x >= 0 && x < border_width;
		bool on_right = x < width && x >= width - border_width;
		bool on_top = y >= 0 && y < border_width;
		bool on_bottom = y < height && y >= height - border_width;

		if (on_top && on_left)    return HTTOPLEFT;
		if (on_top && on_right)   return HTTOPRIGHT;
		if (on_bottom && on_left) return HTBOTTOMLEFT;
		if (on_bottom && on_right)return HTBOTTOMRIGHT;
		if (on_top)               return HTTOP;
		if (on_bottom)            return HTBOTTOM;
		if (on_left)              return HTLEFT;
		if (on_right)             return HTRIGHT;

		// --- TITLE BAR CHECKS (LOWER PRIORITY) ---
		const auto& controlBox = app->GetControlBox();
		const ImVec2 mousePos = { (float)x, (float)y };
		const LONG caption_height = 75;
		if (mousePos.y >= 0 && mousePos.y < caption_height)
		{
			if (controlBox.Minimize.Contains(mousePos) ||
				controlBox.Maximize.Contains(mousePos) ||
				controlBox.Close.Contains(mousePos))
			{
				return HTCLIENT; // Let ImGui handle the click.
			}
			return HTCAPTION; // Drag the window.
		}

		return HTCLIENT; // It's the main content area.
	}
#endif

	Window::Window(const WindowSpecification& spec)
	{
		m_Data.Title = spec.Title;
		m_Data.Width = spec.Width;
		m_Data.Height = spec.Height;
		Init();
	}

	Window::~Window()
	{
		Shutdown();
	}

	void Window::Init()
	{
		glfwSetErrorCallback(glfw_error_callback);
		if (!glfwInit())
		{
			std::cerr << "Could not initialize GLFW!" << std::endl;
			return;
		}

		glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		m_WindowHandle = glfwCreateWindow(m_Data.Width, m_Data.Height, m_Data.Title.c_str(), NULL, NULL);

		// Store a pointer to the Application instance for the callback to use.
		glfwSetWindowUserPointer(m_WindowHandle, &Application::Get());
	}

	void Window::Shutdown()
	{
		glfwDestroyWindow(m_WindowHandle);
		glfwTerminate();
	}

	void Window::PollEvents()
	{
		glfwPollEvents();
	}

	bool Window::ShouldClose()
	{
		return glfwWindowShouldClose(m_WindowHandle);
	}

	VkResult Window::CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface)
	{
		return glfwCreateWindowSurface(instance, m_WindowHandle, nullptr, surface);
	}

	void Window::SetIcon(const unsigned char* data, int len)
	{
		int width, height, channels;
		stbi_uc* pixels = stbi_load_from_memory(data, len, &width, &height, &channels, 4);
		if (pixels)
		{
			GLFWimage image[1];
			image[0].width = width;
			image[0].height = height;
			image[0].pixels = pixels;
			glfwSetWindowIcon(m_WindowHandle, 1, image);
			stbi_image_free(pixels);
		}
	}

	void Window::GetFramebufferSize(int* width, int* height)
	{
		glfwGetFramebufferSize(m_WindowHandle, width, height);
	}
}