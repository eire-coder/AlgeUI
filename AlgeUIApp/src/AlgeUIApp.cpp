#include "AlgeUI/Application.h"
#include "AlgeUI/EntryPoint.h"

#include "imgui.h"

class ExampleLayer : public AlgeUI::Layer
{
public:
	virtual void OnUIRender() override
	{
		ImGui::Begin("Hello from AlgeUI!");
		ImGui::Button("My Button");
		ImGui::End();
	}
};

AlgeUI::Application* AlgeUI::CreateApplication(int argc, char** argv)
{
	AlgeUI::ApplicationSpecification spec;
	spec.Name = "AlgeUI Example Application";
	spec.Width = 1280;
	spec.Height = 720;

	AlgeUI::Application* app = new AlgeUI::Application(spec);

	app->PushLayer<ExampleLayer>();

	app->SetMenubarCallback([app]()
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit"))
				{
					app->Close();
				}
				ImGui::EndMenu();
			}
		});

	return app;
}