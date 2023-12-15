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

bool renderFood = true;
bool renderWander = true;
bool renderRoot = true;

glm::vec3 maxPheromoneClipBounds = glm::vec3(1);
clippingPlanes pheromoneClipping;
bool usePheromoneClipping = false;

glm::vec3 maxSoilClipBounds = glm::vec3(1);
clippingPlanes soilClipping;
bool useSoilClipping = false;

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
		if (CollapsingHeader("Soil Clipping")) {
			Checkbox("Use Clipping", &useSoilClipping);
			DragFloatRange2("X Clipping", &soilClipping.xClipMin, &soilClipping.xClipMax, 1, 0, maxSoilClipBounds.x, "%.0f");
			DragFloatRange2("Y Clipping", &soilClipping.yClipMin, &soilClipping.yClipMax, 1, 0, maxSoilClipBounds.y, "%.0f");
			DragFloatRange2("Z Clipping", &soilClipping.zClipMin, &soilClipping.zClipMax, 1, 0, maxSoilClipBounds.z, "%.0f");
		}

		Spacing();
		if (CollapsingHeader("Pheromone Clipping")) {
			Checkbox("Use Clipping", &usePheromoneClipping);
			DragFloatRange2("X Clipping", &pheromoneClipping.xClipMin, &pheromoneClipping.xClipMax, 1, 0, maxPheromoneClipBounds.x, "%.0f");
			DragFloatRange2("Y Clipping", &pheromoneClipping.yClipMin, &pheromoneClipping.yClipMax, 1, 0, maxPheromoneClipBounds.y, "%.0f");
			DragFloatRange2("Z Clipping", &pheromoneClipping.zClipMin, &pheromoneClipping.zClipMax, 1, 0, maxPheromoneClipBounds.z, "%.0f");
		}

		Spacing();
		Checkbox("Render soil", &renderGround);
		Checkbox("Render agents", &renderAgents);
		Checkbox("Render pheremones", &renderPheremones);
		Checkbox("Render food Pheromone", &renderFood);
		Checkbox("Render wander pheromone", &renderWander);
		Checkbox("Render root pheromone", &renderRoot);

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
