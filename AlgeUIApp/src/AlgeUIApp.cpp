#include "AlgeUI/Application.h"
#include "AlgeUI/EntryPoint.h"
#include "imgui.h"

class ExampleLayer : public AlgeUI::Layer {
public:
    virtual void OnUIRender() override {
        ImGui::Begin("Hello from AlgeUI!"); ImGui::End();
        if (ImGui::BeginPopup("FilePopup")) {
            if (ImGui::MenuItem("Exit")) { AlgeUI::Application::Get().Close(); }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("HelpPopup")) {
            if (ImGui::MenuItem("About")) { /* ... */ }
            ImGui::EndPopup();
        }
    }
};

AlgeUI::Application* AlgeUI::CreateApplication(int argc, char** argv) {
    AlgeUI::ApplicationSpecification spec;
    spec.Name = "Gideon Engine";
    spec.CustomTitleBar = true;
    AlgeUI::Application* app = new AlgeUI::Application(spec);
    app->PushLayer<ExampleLayer>();

    app->SetTitleBarContentCallback([app]() {
        ImGui::SameLine(0.0f, 5.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.35f));

        if (ImGui::Button("File")) { ImGui::OpenPopup("FilePopup"); }

        // --- THE LAYOUT FIX IS HERE ---
        ImGui::SameLine(0.0f, 5.0f); // Add spacing between File and Help

        if (ImGui::Button("Help")) { ImGui::OpenPopup("HelpPopup"); }

        ImGui::PopStyleColor(3);
        });

    return app;
}