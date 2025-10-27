#include "AlgeUI/Window.h"
#include "AlgeUI/Application.h"
#include "imgui.h" // Required for checking ImGui's mouse state (io.WantCaptureMouse)

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include "stb_image.h"

#ifdef WL_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib") // Link against the DWM API library
#endif

namespace AlgeUI {

    static void glfw_error_callback(int error, const char* description)
    {
        fprintf(stderr, "GLFW Error %d: %s\n", error, description);
    }

#ifdef WL_PLATFORM_WINDOWS
    // This is the definitive, correct window procedure that will fix the dragging issue.
    LRESULT CALLBACK Window::Win32WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
            // This message is sent when the OS needs to calculate the window's client area.
            // Returning 0 here effectively removes the standard title bar and borders.
        case WM_NCCALCSIZE:
        {
            if (wParam == TRUE)
            {
                return 0;
            }
            break;
        }

        // This is the most important message for custom frames. It asks what part
        // of the window the cursor is over.
        case WM_NCHITTEST:
        {
            const LONG border_width = 8; // Resize handle thickness
            const LONG caption_height = 48; // Your custom title bar height
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);
            long x = GET_X_LPARAM(lParam);
            long y = GET_Y_LPARAM(lParam);

            // Check for resize handles when not maximized
            if (!IsZoomed(hwnd))
            {
                if (y >= windowRect.top && y < windowRect.top + border_width) return HTTOP;
                if (y < windowRect.bottom && y >= windowRect.bottom - border_width) return HTBOTTOM;
                if (x >= windowRect.left && x < windowRect.left + border_width) return HTLEFT;
                if (x < windowRect.right && x >= windowRect.right - border_width) return HTRIGHT;
                if (x >= windowRect.left && x < windowRect.left + border_width && y >= windowRect.top && y < windowRect.top + border_width) return HTTOPLEFT;
                if (x < windowRect.right && x >= windowRect.right - border_width && y >= windowRect.top && y < windowRect.top + border_width) return HTTOPRIGHT;
                if (x >= windowRect.left && x < windowRect.left + border_width && y < windowRect.bottom && y >= windowRect.bottom - border_width) return HTBOTTOMLEFT;
                if (x < windowRect.right && x >= windowRect.right - border_width && y < windowRect.bottom && y >= windowRect.bottom - border_width) return HTBOTTOMRIGHT;
            }

            // Check if the cursor is in our custom title bar area
            if (y >= windowRect.top && y < windowRect.top + caption_height)
            {
                // If the mouse is over an ImGui widget in the title bar, treat it as part of the client area.
                if (ImGui::GetIO().WantCaptureMouse)
                {
                    return HTCLIENT;
                }
                // Otherwise, this is our title bar. Tell Windows it's a caption.
                return HTCAPTION;
            }

            // Any other part of the window is the client area.
            return HTCLIENT;
        }
        }

        // For all other messages, and for default handling of the ones above,
        // pass them to the original window procedure that we subclassed. This is vital.
        return CallWindowProc(s_OriginalWndProc, hwnd, msg, wParam, lParam);
    }
#endif

    Window::Window(const WindowSpecification& spec)
    {
        m_Data.Title = spec.Title;
        m_Data.Width = spec.Width;
        m_Data.Height = spec.Height;
        Init(spec);
    }

    Window::~Window()
    {
        Shutdown();
    }

    void Window::Init(const WindowSpecification& spec)
    {
        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit())
        {
            std::cerr << "Could not initialize GLFW!" << std::endl;
            return;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        if (spec.CustomTitleBar)
        {
            // Create an undecorated window if we're using our custom title bar.
            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        }

        m_WindowHandle = glfwCreateWindow(m_Data.Width, m_Data.Height, m_Data.Title.c_str(), NULL, NULL);

        glfwSetWindowUserPointer(m_WindowHandle, &Application::Get());

#ifdef WL_PLATFORM_WINDOWS
        if (spec.CustomTitleBar)
        {
            m_NativeHandle = glfwGetWin32Window(m_WindowHandle);

            LONG_PTR style = GetWindowLongPtr(m_NativeHandle, GWL_STYLE);
            // Add styles for resizing and maximize, but NOT WS_CAPTION.
            style |= WS_MAXIMIZEBOX | WS_THICKFRAME;
            SetWindowLongPtr(m_NativeHandle, GWL_STYLE, style);

            SetWindowPos(m_NativeHandle, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

            // Subclass the window procedure.
            s_OriginalWndProc = (WNDPROC)SetWindowLongPtr(m_NativeHandle, GWLP_WNDPROC, (LONG_PTR)Win32WndProc);
        }
#endif
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