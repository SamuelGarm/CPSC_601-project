#pragma once

#include <iosfwd>
#include <string>

#include "imgui/imgui.h"
#include "clippingPlanes.h"
#include "glm/glm.hpp"

namespace panel {

extern bool showPanel;
extern ImVec4 clear_color;

// animation
extern bool playModel;
extern bool resetModel;
extern bool stepModel;

extern float nutrientThreshold;

extern int renderSoil;
extern float stepTime;

extern bool renderGround;
extern bool renderAgents;
extern bool renderPheremones;

extern bool renderFood;
extern bool renderWander;
extern bool renderRoot;

extern glm::vec3 maxPheromoneClipBounds;
extern clippingPlanes pheromoneClipping;
extern bool usePheromoneClipping;

extern glm::vec3 maxSoilClipBounds;
extern clippingPlanes soilClipping;
extern bool useSoilClipping;

// reset
extern bool resetView;

void updateMenu();

} // namespace panel
