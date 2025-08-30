#pragma once

#include "Layer.h"
#include "Window.h"
#include "Image.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "imgui.h"
#include "vulkan/vulkan.h"
#include <imgui_internal.h>

void check_vk_result(VkResult err);

// Forward-declare the context
namespace AlgeUI { class VulkanContext; }

namespace AlgeUI {

	struct ApplicationSpecification
	{
		std::string Name = "AlgeUI App";
		uint32_t Width = 1600;
		uint32_t Height = 900;
	};

	struct TitleBarControlBox
	{
		ImRect Minimize;
		ImRect Maximize;
		ImRect Close;
	};

	class Application
	{
	public:
		Application(const ApplicationSpecification& applicationSpecification = ApplicationSpecification());
		~Application();

		static Application& Get();
		void Run();

		void SetMenubarCallback(const std::function<void()>& menubarCallback) { m_MenubarCallback = menubarCallback; }

		static bool IsTitleBarHovered() { return s_TitleBarHovered; }

		template<typename T>
		void PushLayer()
		{
			static_assert(std::is_base_of<Layer, T>::value, "Pushed type is not subclass of Layer!");
			m_LayerStack.emplace_back(std::make_shared<T>())->OnAttach();
		}

		void PushLayer(const std::shared_ptr<Layer>& layer) { m_LayerStack.emplace_back(layer); layer->OnAttach(); }

		void Close();

		float GetTime();
		GLFWwindow* GetWindowHandle() const { return m_Window->GetNativeWindow(); }

		// DEPRECATED - These will be removed later. Use VulkanContext::Get...() instead.
		static VkInstance GetInstance();
		static VkPhysicalDevice GetPhysicalDevice();
		static VkDevice GetDevice();

		static VkCommandBuffer GetCommandBuffer(bool begin);
		static void FlushCommandBuffer(VkCommandBuffer commandBuffer);
		static void SubmitResourceFree(std::function<void()>&& func);

		static const TitleBarControlBox& GetControlBox() { return s_ControlBox; }

	private:
		void Init();
		void Shutdown();

	private:
		ApplicationSpecification m_Specification;
		bool m_Running = false;

		// The application now OWNS these objects
		std::unique_ptr<Window> m_Window;
		std::unique_ptr<VulkanContext> m_VulkanContext;
		std::shared_ptr<Image> m_AppIcon; // Add this for the title bar icon

		float m_TimeStep = 0.0f;
		float m_FrameTime = 0.0f;
		float m_LastFrameTime = 0.0f;

		std::vector<std::shared_ptr<Layer>> m_LayerStack;
		std::function<void()> m_MenubarCallback;

		inline static bool s_TitleBarHovered = false;
		inline static TitleBarControlBox s_ControlBox;
	};

	// Implemented by CLIENT
	Application* CreateApplication(int argc, char** argv);
}