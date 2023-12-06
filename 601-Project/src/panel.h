#pragma once

#include <iosfwd>
#include <string>

#include "imgui/imgui.h"

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

// reset
extern bool resetView;

void updateMenu();

} // namespace panel
