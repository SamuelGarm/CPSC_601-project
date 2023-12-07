#include "panel.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <array>
#include <string>

namespace panel {

// default values
bool showPanel = false;
ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

// animation
bool playModel = false;
bool resetModel = false;
bool stepModel = false;

float nutrientThreshold = 0;

int renderSoil = 1;
float stepTime = 0.5;


bool renderGround = true;
bool renderAgents = true;
bool renderPheremones = false;

// reset
bool resetView = false;

std::string state = "idle";

void updateMenu() {
  using namespace ImGui;

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  if (showPanel && Begin("panel", &showPanel, ImGuiWindowFlags_MenuBar)) {
    if (BeginMenuBar()) {
      if (BeginMenu("File")) {
        if (MenuItem("Close", "(P)")) {
          showPanel = false;
        }
        // add more if you would like...
        ImGui::EndMenu();
      }
      EndMenuBar();
    }

    Spacing();
    if (CollapsingHeader("Background Color")) { // Clear
      ColorEdit3("Clear color", (float *)&clear_color);
    }

    Spacing();
    if (CollapsingHeader("visualization")) {
			SliderFloat("Nutrient Threshold", &nutrientThreshold, 0, 1);
    }

    Spacing();
    Separator();
		Text(state.c_str());
		Checkbox("Run", &playModel);
			if (playModel)
				state = "running";
			else
				state = "idle";
    
    resetModel = Button("Reset Model");
    stepModel = Button("Step");

		Spacing();
		RadioButton("soil", &renderSoil, 1); ImGui::SameLine();
		RadioButton("root", &renderSoil, 0);

		Spacing();
		Checkbox("Render soil", &renderGround);
		Checkbox("Render agents", &renderAgents);
		Checkbox("Render pheremones", &renderPheremones);

		Spacing();
		DragFloat("Step time", &stepTime, 0.01, 0, 5);

    Spacing();
    Separator();
    resetView = Button("Reset view");

    Spacing();
    Separator();
    Text("Application average %.3f ms/frame (%.1f FPS)",
         1000.0f / GetIO().Framerate, GetIO().Framerate);

    End();
  }
  ImGui::Render();	// Render the ImGui window
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // Some middleware thing
}

} // namespace panel
