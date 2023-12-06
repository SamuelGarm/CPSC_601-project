#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/compatibility.hpp> // lerp
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/random.hpp"
#include "glm/gtx/string_cast.hpp"

#include "panel.h"
#include "Shader.h"
#include "GLHandles.h"
#include "ShaderProgram.h"
#include "Camera.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "cube.h"
#include "VoxelGrid.h"
#include <map>

//simulation variables
#define NUMBER_OF_STARTING_AGENTS 5
#define SOIL_X_LENGTH 100
#define SOIL_Y_LENGTH 100
#define SOIL_Z_LENGTH 100

//camera variables
bool leftMouseButtonPressed = false;

float mouseX = 0;
float mouseY = 0;
float clickX = 0;
float clickY = 0;

bool firstMouse = true;
float lastX;
float lastY;
int scrollAcc = 0;

Camera camera;

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (action == GLFW_PRESS) {
    if (key == GLFW_KEY_P)
      panel::showPanel = !panel::showPanel;
		if (key == GLFW_KEY_W)
			camera.fspeed += 1.0f;
		if (key == GLFW_KEY_S)
			camera.fspeed += -1.0f;
		if (key == GLFW_KEY_A)
			camera.hspeed += -1.0f;
		if (key == GLFW_KEY_D)
			camera.hspeed += 1.0f;
  } 
  else if (action == GLFW_RELEASE)
  {
		if (key == GLFW_KEY_W)
			camera.fspeed -= 1.0f;
		if (key == GLFW_KEY_S)
			camera.fspeed -= -1.0f;
		if (key == GLFW_KEY_A)
			camera.hspeed -= -1.0f;
		if (key == GLFW_KEY_D)
			camera.hspeed -= 1.0f;
  }
}
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			clickX = mouseX;
			clickY = mouseY;
			leftMouseButtonPressed = true;
		}
		else if (action == GLFW_RELEASE) {
			leftMouseButtonPressed = false;
			firstMouse = true;
		}
	}
}
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
	mouseX = xpos;
	mouseY = ypos;
	if (leftMouseButtonPressed) {
		if (firstMouse)
		{
			lastX = clickX;
			lastY = clickY;
			firstMouse = false;
		}
		float dx = (xpos - lastX) * 0.1;
		float dy = (lastY - ypos) * 0.1;

		lastX = xpos;
		lastY = ypos;

		camera.panHorizontal(dx);
		camera.panVertical(dy);
	}
}
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	scrollAcc += yoffset;

	camera.cameraSpeed = 1-0.79*(1-std::exp(0.11 * scrollAcc));
}
void errorCallback(int error, const char* description) {
  std::cout << "GLFW error: " << error << " - " << description << "\n";
  throw std::runtime_error("Failed to create GLFW window.");
}

struct Agent {
	enum State {SEARCHING, RETURNING};
	State state = State::SEARCHING;
	glm::ivec3 direction = glm::ivec3(0,-1,0);
  glm::ivec3 position = glm::ivec3(0); //integer representation of position in the matrix 
};

struct PheremoneVoxel {
	//stores how many agents of each type are in a single 'agent voxel'
	float wanderPheremone = 0;
	float foodFoundPheremone = 0;
};

struct SoilVoxel {
	float nutrient = 0;
	bool isSoil = true; //if the soil is actually there or if it is now 'root'
};

struct soilRenderData {
	glm::mat4 transform = glm::mat4(1);
	float nutrient = 0;
};

struct agentRenderData {
	glm::mat4 transform = glm::mat4(1);
};

struct pheremoneRenderData {
	glm::mat4 transform = glm::mat4(1);
	glm::vec3 color = glm::vec3(0);
};

void generateSoil(VoxelGrid<SoilVoxel>& soil) {
	const int numberOfSources = 5;
	//generate the soil with reasonable nutrient distribution
	//generate n nutrient source points
	std::vector<glm::vec3> sources;
	for (int i = 0; i < numberOfSources; i++) {
		sources.push_back(glm::vec3(glm::linearRand<int>(0, SOIL_X_LENGTH), glm::linearRand<int>(0, SOIL_Y_LENGTH), glm::linearRand<int>(0, SOIL_Z_LENGTH)));
	}
	for (int x = 0; x < SOIL_X_LENGTH; x++) {
		for (int y = 0; y < SOIL_Y_LENGTH; y++) {
			for (int z = 0; z < SOIL_Z_LENGTH; z++) {
				glm::vec3 soilPoint(x, y, z);
				//find the closest source and use a linear falloff
				float shortestDistance = glm::distance(soilPoint, sources[0]);
				for (int i = 1; i < numberOfSources; i++) {
					float distance = glm::distance(soilPoint, sources[i]);
					if (distance < shortestDistance)
						shortestDistance = distance;
				}
				//calculate the nutrient value based on a falloff
				soil.at(x, y, z).nutrient = 1;//50.f / (shortestDistance + 50.f);
			}
		}
	}

	//generate a hold for the 'nest'
	for (int x = (SOIL_X_LENGTH / 2) - 2; x <= (SOIL_X_LENGTH / 2) + 2; x++) {
		for (int z = (SOIL_Z_LENGTH / 2) - 2; z <= (SOIL_Z_LENGTH / 2) + 2; z++) {
			soil.at(x, SOIL_Y_LENGTH-1, z).isSoil = false;
			soil.at(x, SOIL_Y_LENGTH - 1, z).nutrient = 0;
		}
	}
}

void stepSimulation(VoxelGrid<SoilVoxel>& soil, VoxelGrid<PheremoneVoxel>& pheromones, std::vector<Agent>& agents) {
	for (Agent& agent : agents) {
		//create the coordinate frame
		glm::ivec3 front = agent.direction;
		glm::ivec3 right = agent.direction.x == 0 ? glm::ivec3(1, 0, 0) : glm::ivec3(0, 1, 0);
		glm::ivec3 up = glm::cross(glm::vec3(front), glm::vec3(right));

		//std::cout << "\n-----------------------------------------------------------------------\n";
		//std::cout << "Agent position: " << glm::to_string(agent.position) << '\n';
		//std::cout << "Agent orientation:\n\tFront: " << glm::to_string(front) << "\n\tRight: " << glm::to_string(right) << "\n\tUp: " << glm::to_string(up) << '\n';

		//patterns are specified as coordinates relative to the agents local frame <Heading, Right, Up> (They should be symetrical)
		//pattern 1 is a search of the 3x3 grid directly in front
		std::vector<glm::ivec3> pattern1 = { {1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

		//set the pattern you want to use
		std::vector<glm::ivec3> pattern = pattern1;

		float nutrientWeight = 1;
		float foodPheremoneWeight = 1;
		float wanderPheremoneWeight = 1;


		//calculate how the agent decides to move based on its state
		if(agent.state == agent.SEARCHING) {
			//std::cout << "Agent state is: SEARCHING\n";
			//calculate the influinces from each point in the pattern
			std::vector<std::pair<glm::vec3, float>> weights;
			for (glm::ivec3 offset : pattern) {
				glm::ivec3 searchLoc = agent.position + (front * offset[0] + right * offset[1] + up * offset[2]); //the location in the agent grid
				glm::ivec3 soilLoc = searchLoc / 3; //the location in the soil grid
				if (searchLoc.x < 0 || searchLoc.x > SOIL_X_LENGTH * 3 - 1
					|| searchLoc.y < 0 || searchLoc.y > SOIL_Y_LENGTH * 3 - 1
					|| searchLoc.z < 0 || searchLoc.z > SOIL_Z_LENGTH * 3 - 1)
					continue;
				float nutrient = soil.at(soilLoc.x, soilLoc.y, soilLoc.z).nutrient;
				float pheremone = pheromones.at(searchLoc.x, searchLoc.y, searchLoc.z).foodFoundPheremone;
				float weight = nutrient * nutrientWeight + pheremone * foodPheremoneWeight;
				if (weight == 0)
					continue;
				weights.push_back(std::pair<glm::ivec3, float>(searchLoc, weight));
				//std::cout << "Adding weight\n\tPos: " << glm::to_string(searchLoc) << " \n\tWeight: " << weight << '\n';
			}

			//go through all the weights that have been calculated and choose a direction
			std::pair<glm::ivec3, float> best(glm::ivec3(0), -1);
			for (auto& e : weights) {
				if (e.second > best.second)
					best = e;
			}
			//std::cout << " Best weight is\n\tPos: " << glm::to_string(best.first) << " \n\tWeight: " << best.second << '\n';
			while (best.second == -1 || best.first.x < 0 || best.first.x > SOIL_X_LENGTH * 3 - 1
															|| best.first.y < 0 || best.first.y > SOIL_Y_LENGTH * 3 - 1
															|| best.first.z < 0 || best.first.z > SOIL_Z_LENGTH * 3 - 1) {
				//if there is no best then choose a random direction
				int randInd = glm::linearRand<int>(0, pattern.size()-1);
				best.first = pattern[randInd] + agent.position;
				best.second = 0;
				//std::cout << "Random position choosen: " << glm::to_string(pattern[randInd]) << '\n';
			}
			glm::ivec3 direction = best.first - agent.position;
			agent.direction = direction;
		}
		else if (agent.state == agent.RETURNING) {
			//std::cout << "Agent state is: RETURNING\n";
			//calculate the influinces from each point in the pattern
			std::vector<std::pair<glm::vec3, float>> weights;
			for (glm::ivec3 offset : pattern) {
				glm::ivec3 searchLoc = agent.position + (front * offset[0] + right * offset[1] + up * offset[2]); //the location in the agent grid
				glm::ivec3 soilLoc = searchLoc / 3; //the location in the soil grid

				if (searchLoc.x < 0 || searchLoc.x > SOIL_X_LENGTH * 3 - 1
					|| searchLoc.y < 0 || searchLoc.y > SOIL_Y_LENGTH * 3 - 1
					|| searchLoc.z < 0 || searchLoc.z > SOIL_Z_LENGTH * 3 - 1)
					continue;

				float pheremone = pheromones.at(searchLoc.x, searchLoc.y, searchLoc.z).wanderPheremone;
				float weight = pheremone * wanderPheremoneWeight;
				if (weight == 0)
					continue;
				weights.push_back(std::pair<glm::ivec3, float>(searchLoc, weight));
				//std::cout << "Adding weight\n\tPos: " << glm::to_string(searchLoc) << " \n\tWeight: " << weight << '\n';
			}

			//go through all the weights that have been calculated and choose a direction
			std::pair<glm::ivec3, float> best(glm::ivec3(0), -1);
			for (auto& e : weights) {
				if (e.second > best.second)
					best = e;
			}
			//std::cout << " Best weight is\n\tPos: " << glm::to_string(best.first) << " \n\tWeight: " << best.second << '\n';
			while (best.second == -1 || best.first.x < 0 || best.first.x > SOIL_X_LENGTH * 3 - 1
				|| best.first.y < 0 || best.first.y > SOIL_Y_LENGTH * 3 - 1
				|| best.first.z < 0 || best.first.z > SOIL_Z_LENGTH * 3 - 1) {
				//if there is no best then choose a random direction
				int randInd = glm::linearRand<int>(0, pattern.size()-1);
				best.first = pattern[randInd] + agent.position;
				best.second = 0;
				//std::cout << "Random position choosen: " << glm::to_string(pattern[randInd]) << '\n';
			}
			glm::ivec3 direction = best.first - agent.position;
			agent.direction = direction;
		}
	}

	//update position step
	for (Agent& agent : agents) {
		//loop over each agent and move it
		//if the agent encounters a soil voxel eat some nutrient and make the agent want to follow the return pheromones
		glm::ivec3 nextPos = agent.position + agent.direction;
		glm::ivec3 nextSoilPos = nextPos / 3;
		SoilVoxel& soilVox = soil.at(nextSoilPos.x, nextSoilPos.y, nextSoilPos.z);

		if (agent.state == agent.SEARCHING) {
			//if it is going to collide
			if (soilVox.isSoil) {
				//std::cout << "collision detected\n";
				soilVox.nutrient -= 1;
				if (soilVox.nutrient <= 0) {
					soilVox.isSoil = false;
					soilVox.nutrient = 0;
					//std::cout << "Soil depleted, removing\n";
				}
				agent.state = agent.RETURNING;
				agent.direction *= -1;
			}
		}
		else if (agent.state == agent.RETURNING) {
			if(soilVox.isSoil)
				agent.direction *= -1;
			//detect if the agent is in the nest region
			glm::vec3 smallValues = glm::vec3(((SOIL_X_LENGTH * 3) / 2) - 2, (SOIL_Y_LENGTH * 3) - 2, ((SOIL_Z_LENGTH * 3) / 2) - 2);
			glm::vec3 largeValues = glm::vec3(((SOIL_X_LENGTH * 3) / 2) + 2, (SOIL_Y_LENGTH * 3), ((SOIL_Z_LENGTH * 3) / 2) + 2);
			if (agent.position.x >= smallValues.x && agent.position.x <= largeValues.x &&
				agent.position.y >= smallValues.y && agent.position.y <= largeValues.y &&
				agent.position.z >= smallValues.z && agent.position.z <= largeValues.z)
				agent.state = agent.SEARCHING;
		}

		if(agent.state == agent.SEARCHING)
			pheromones.at(agent.position.x, agent.position.y, agent.position.z).wanderPheremone += 1;
		else if (agent.state == agent.RETURNING)
			pheromones.at(agent.position.x, agent.position.y, agent.position.z).foodFoundPheremone += 1;
		
		agent.position += agent.direction;

	}

	//evaporate pheremones
	/*
	for (int x = 0; x < SOIL_X_LENGTH * 3; x++) {
		for (int y = 0; y < SOIL_Y_LENGTH * 3; y++) {
			for (int z = 0; z < SOIL_Z_LENGTH * 3; z++) {
				float a = 0.82;
				pheromones.at(x, y, z).foodFoundPheremone -= log(a * pheromones.at(x, y, z).foodFoundPheremone + 1);
				pheromones.at(x, y, z).wanderPheremone -= log(a * pheromones.at(x, y, z).wanderPheremone + 1);
			}
		}
	}*/
}

void loadSoilGrid(VoxelGrid<SoilVoxel>& soil, std::vector<soilRenderData>& instancedVoxelData, bool isSoilCond = true) {
	instancedVoxelData.clear();
	//set up simulation (columns sweep the x, rows sweep the y, layers sweep the z
	for (int x = 0; x < SOIL_X_LENGTH; x++) {
		for (int y = 0; y < SOIL_Y_LENGTH; y++) {
			for (int z = 0; z < SOIL_Z_LENGTH; z++) {
				if (soil.at(x, y, z).isSoil != isSoilCond) {
					continue;
				}
				char neighbours = 0;
				if (x > 0 && soil.at(x - 1, y, z).isSoil == isSoilCond) neighbours++;
				if (x < SOIL_X_LENGTH - 1 && soil.at(x + 1, y, z).isSoil == isSoilCond) neighbours++;
				if (y > 0 && soil.at(x, y-1, z).isSoil == isSoilCond) neighbours++;
				if (y < SOIL_Y_LENGTH  - 1 && soil.at(x, y+1, z).isSoil == isSoilCond) neighbours++;
				if (z > 0 && soil.at(x, y, z-1).isSoil == isSoilCond) neighbours++;
				if (z < SOIL_Z_LENGTH - 1 && soil.at(x, y, z+1).isSoil == isSoilCond) neighbours++;
				if (neighbours == 6) continue;
				soilRenderData data;
				data.transform = glm::translate(glm::mat4(1), glm::vec3(x, y, z));
				data.nutrient = soil.at(x, y, z).nutrient;
				instancedVoxelData.push_back(data);
			}
		}
	}
}

void loadAgentGrid(const std::vector<Agent>& agents, std::vector<agentRenderData>& instancedAgentData) {
	instancedAgentData.clear();
	std::map<int, bool> occupied;
	for (const Agent& agent : agents) {
		int index = agent.position.x + agent.position.y * SOIL_X_LENGTH * 3 + agent.position.z * SOIL_X_LENGTH * 3 * SOIL_Y_LENGTH * 3;
		if (occupied[index])
			continue;
		occupied[index] = true;
		glm::vec3 position = glm::vec3(agent.position.x, agent.position.y, agent.position.z);
		agentRenderData data;
		data.transform = glm::translate(glm::mat4(1), (position - glm::vec3(1)) / 3.f) * glm::scale(glm::mat4(1), glm::vec3(1 / 3.f));
		instancedAgentData.push_back(data);
	}
}

void loadPheremoneGrid(VoxelGrid<PheremoneVoxel>& pheremones, std::vector<pheremoneRenderData>& instancedPheremoneData) {
	instancedPheremoneData.clear();
	float maxWander = 0;
	float maxFood = 0;
	//set up simulation (columns sweep the x, rows sweep the y, layers sweep the z
	for (int x = 0; x < SOIL_X_LENGTH * 3; x++) {
		for (int y = 0; y < SOIL_Y_LENGTH * 3; y++) {
			for (int z = 0; z < SOIL_Z_LENGTH * 3; z++) {
				glm::vec3 position = glm::vec3(x, y, z);
				PheremoneVoxel voxel = pheremones.at(x, y, z);
				if (voxel.foodFoundPheremone == 0 && voxel.wanderPheremone == 0) {
					continue;
				}
				maxWander = std::max(maxWander, voxel.wanderPheremone);
				maxFood = std::max(maxFood, voxel.foodFoundPheremone);
				pheremoneRenderData data;
				data.transform = glm::translate(glm::mat4(1), (position - glm::vec3(1)) / 3.f) * glm::scale(glm::mat4(1), glm::vec3(1 / 3.f));
				data.color = (voxel.foodFoundPheremone) * glm::vec3(1, 0, 0) + (voxel.wanderPheremone) * glm::vec3(0, 1, 0);
				instancedPheremoneData.push_back(data);
			}
		}
	}
	for (auto& e : instancedPheremoneData) {
		e.color.r /= maxFood;
		e.color.g /= maxWander;
	}
}

//
// program entry point
//
int main(void) {
  //set up glfw error handling
  glfwSetErrorCallback(errorCallback);

  if (!glfwInit())
  {
    throw std::runtime_error("Failed to create GLFW window.");
  }

  //
  // setup window (OpenGL context)
  //
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // needed for mac?
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);

  GLFWwindow* window = glfwCreateWindow(1000, 1000, "Growing to computers near you!", NULL, NULL);
  if (window == nullptr) {
    std::cout << "WINDOW failed to create GLFW window\n";
    throw std::runtime_error("Failed to create GLFW window.");
  }
  glfwMakeContextCurrent(window);
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    std::cout << "WINDOW glewInit error: " << glewGetErrorString(err) << '\n';
    throw std::runtime_error("Failed to initialize GLEW");
  }

  // Standard ImGui/GLFW middleware
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330 core");

  //
  // setup simulation
  //

  //  Setup callbacks
  glfwSetKeyCallback(window, keyCallback);
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
  glfwSetScrollCallback(window, scrollCallback);
  glfwSetCursorPosCallback(window, cursorPosCallback);




	//Rendering variables
	std::vector<soilRenderData> instancedVoxelData;
	std::vector<agentRenderData> instancedAgentData;
	std::vector<pheremoneRenderData> instancedPheremoneData;

	//simulation state variables
	std::vector<Agent> agents;
	VoxelGrid<PheremoneVoxel> pheremones(SOIL_X_LENGTH * 3, SOIL_Y_LENGTH * 3, SOIL_Z_LENGTH * 3);
	VoxelGrid<SoilVoxel> soil(SOIL_X_LENGTH, SOIL_Y_LENGTH, SOIL_Z_LENGTH);

	/*
	* Setup openGL structures for rendering voxel terrain
	* There are 2 buffers, one for the vertex geometry and the other storing data such as position that is applied to each voxel instance
	*/
	ShaderProgram voxelShader("shaders/voxel.vert", "shaders/voxel.frag");
  GLuint voxels_vertexArray; //stores the state of how to render and interpert that buffer data
  GLuint voxels_vertexBuffer; //stores the vertex data in a buffer on the GPU
  GLuint voxels_instanceTransformBuffer; //stores data that is used for each instance of voxels
	glGenVertexArrays(1, &voxels_vertexArray);
	glBindVertexArray(voxels_vertexArray);
	glGenBuffers(1, &voxels_vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, voxels_vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_data), cube_data, GL_STATIC_DRAW);

	//set up vertex data for the cube
	glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)0);
  
	glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float)*3));
  
	//set up instancing buffers buffers
  glGenBuffers(1, &voxels_instanceTransformBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, voxels_instanceTransformBuffer);
  //set up instanced transformation matricies (I hate that I need to pass a 4x4 matrix...)
	glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4) + sizeof(float), (void*)(0 * sizeof(glm::vec4)));
	glVertexAttribDivisor(2, 1);
  
	glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4) + sizeof(float), (void*)(1 * sizeof(glm::vec4)));
	glVertexAttribDivisor(3, 1);

	glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4) + sizeof(float), (void*)(2 * sizeof(glm::vec4)));
	glVertexAttribDivisor(4, 1);

	glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4) + sizeof(float), (void*)(3 * sizeof(glm::vec4)));
	glVertexAttribDivisor(5, 1);

	//nutrient
	glEnableVertexAttribArray(6);
	glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4) + sizeof(float), (void*)(4 * sizeof(glm::vec4)));
	glVertexAttribDivisor(6, 1);

	/*
	* Set up openGL structures for the boids
	*/
	ShaderProgram boidShader("shaders/boid.vert", "shaders/boid.frag");
	GLuint agents_vertexArray; //stores the state of how to render and interpert that buffer data
	GLuint agents_vertexBuffer; //stores the vertex data in a buffer on the GPU
	GLuint agents_instanceTransformBuffer; //stores data that is used for each instance of voxels
	glGenVertexArrays(1, &agents_vertexArray); 
	glBindVertexArray(agents_vertexArray);
	glGenBuffers(1, &agents_vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, agents_vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_data), cube_data, GL_STATIC_DRAW);

	//set up vertex data for the cube
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)0);
	
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));

	//set up instancing buffers buffers
	glGenBuffers(1, &agents_instanceTransformBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, agents_instanceTransformBuffer);
	//set up instanced transformation matricies (I hate that I need to pass a 4x4 matrix...)
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4), (void*)(0 * sizeof(glm::vec4)));
	glVertexAttribDivisor(2, 1);

	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4), (void*)(1 * sizeof(glm::vec4)));
	glVertexAttribDivisor(3, 1);

	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4), (void*)(2 * sizeof(glm::vec4)));
	glVertexAttribDivisor(4, 1);

	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4), (void*)(3 * sizeof(glm::vec4)));
	glVertexAttribDivisor(5, 1);

	/*
* Setup openGL structures for rendering pheremones
* There are 2 buffers, one for the vertex geometry and the other storing data such as position that is applied to each voxel instance
*/
	ShaderProgram pheremonesShader("shaders/pheromone.vert", "shaders/pheromone.frag");
	GLuint pheremones_vertexArray; //stores the state of how to render and interpert that buffer data
	GLuint pheremones_vertexBuffer; //stores the vertex data in a buffer on the GPU
	GLuint pheremones_instanceTransformBuffer; //stores data that is used for each instance of pheremones
	glGenVertexArrays(1, &pheremones_vertexArray);
	glBindVertexArray(pheremones_vertexArray);
	glGenBuffers(1, &pheremones_vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, pheremones_vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_data), cube_data, GL_STATIC_DRAW);

	//set up vertex data for the cube
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));

	glGenBuffers(1, &pheremones_instanceTransformBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, pheremones_instanceTransformBuffer);
	//set up instanced transformation matricies (I hate that I need to pass a 4x4 matrix...)
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4) + sizeof(glm::vec3), (void*)(0 * sizeof(glm::vec4)));
	glVertexAttribDivisor(2, 1);

	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4) + sizeof(glm::vec3), (void*)(1 * sizeof(glm::vec4)));
	glVertexAttribDivisor(3, 1);

	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4) + sizeof(glm::vec3), (void*)(2 * sizeof(glm::vec4)));
	glVertexAttribDivisor(4, 1);

	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4) + sizeof(glm::vec3), (void*)(3 * sizeof(glm::vec4)));
	glVertexAttribDivisor(5, 1);

	//color
	glEnableVertexAttribArray(6);
	glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4) + sizeof(glm::vec3), (void*)(4 * sizeof(glm::vec4)));
	glVertexAttribDivisor(6, 1);

	generateSoil(soil);

	loadSoilGrid(soil, instancedVoxelData, panel::renderSoil == 1);
	glBindVertexArray(voxels_vertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, voxels_instanceTransformBuffer);
	glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::mat4) + sizeof(float))* instancedVoxelData.size(), instancedVoxelData.data(), GL_DYNAMIC_DRAW);

	for (int i = 0; i < 30; i++) {
		Agent a = Agent();
		a.state = a.SEARCHING;
		a.position = glm::ivec3((SOIL_X_LENGTH * 3) / 2, SOIL_Y_LENGTH * 3 - 1, (SOIL_Z_LENGTH * 3) / 2);
		agents.push_back(a);
	}

	loadAgentGrid(agents, instancedAgentData);

	//buffer agent data
	glBindVertexArray(agents_vertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, agents_instanceTransformBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4)* instancedAgentData.size(), instancedAgentData.data(), GL_DYNAMIC_DRAW);

  using namespace std::chrono;

  double accumulator = 0.0; // The accumulator for the remaining time

  auto previous_time = steady_clock::now(); // The time of the previous update
	double frameTime = 1.f;
  //
  // main loop
  //
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
		if(!io.WantCaptureMouse && !io.WantCaptureKeyboard)
			camera.update(frameTime);

		if (panel::stepModel) {
			accumulator = 1;
		}

		//simulation loop that uses discrete steps
		if (panel::playModel || panel::stepModel) {
			auto current_time = steady_clock::now(); // Get the current time
			auto elapsed_time = duration_cast<duration<double>>(current_time - previous_time);
			previous_time = current_time;
			accumulator += elapsed_time.count();
			if (accumulator > panel::stepTime) {
				accumulator = 0;
				stepSimulation(soil, pheremones, agents);
				std::cout << "step\n";

				//buffer soil data
				if (panel::renderGround) {
					loadSoilGrid(soil, instancedVoxelData, panel::renderSoil == 1);
					glBindVertexArray(voxels_vertexArray);
					glBindBuffer(GL_ARRAY_BUFFER, voxels_instanceTransformBuffer);
					glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::mat4) + sizeof(float)) * instancedVoxelData.size(), instancedVoxelData.data(), GL_DYNAMIC_DRAW);
				}
				if (panel::renderAgents) {
					//buffer agent data
					loadAgentGrid(agents, instancedAgentData);
					glBindVertexArray(agents_vertexArray);
					glBindBuffer(GL_ARRAY_BUFFER, agents_instanceTransformBuffer);
					glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * instancedAgentData.size(), instancedAgentData.data(), GL_DYNAMIC_DRAW);
				}
				if (panel::renderPheremones) {
					//buffer pheremone data
					loadPheremoneGrid(pheremones, instancedPheremoneData);
					glBindVertexArray(pheremones_vertexArray);
					glBindBuffer(GL_ARRAY_BUFFER, pheremones_instanceTransformBuffer);
					glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::mat4) + sizeof(glm::vec3)) * instancedPheremoneData.size(), instancedPheremoneData.data(), GL_DYNAMIC_DRAW);
				}
			}
		}
		else {
			accumulator = 0;
			previous_time = steady_clock::now();
		}

		

    //
    // render
    //
    //camera
		glm::mat4 V = camera.getView();
		
    glEnable(GL_DEPTH_TEST);
    auto color = panel::clear_color;
    glClearColor(color.x, color.y, color.z, color.z);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		
		//render the boids
		if (panel::renderAgents) {
			boidShader.use();
			glBindVertexArray(agents_vertexArray);
			glBindBuffer(GL_ARRAY_BUFFER, agents_instanceTransformBuffer);
			GLuint cameraUniform = glGetUniformLocation(GLuint(boidShader), "cameraMat");
			glUniformMatrix4fv(cameraUniform, 1, GL_FALSE, glm::value_ptr(glm::perspective(45.f, 1.f, 0.1f, 500.f) * V));

			glDrawArraysInstanced(GL_TRIANGLES, 0, 36, instancedAgentData.size());
		}

		if (panel::renderGround) {
			//render the voxels
			voxelShader.use();
			glBindVertexArray(voxels_vertexArray);
			glBindBuffer(GL_ARRAY_BUFFER, voxels_instanceTransformBuffer);
			GLuint cameraUniform = glGetUniformLocation(GLuint(voxelShader), "cameraMat");
			glUniformMatrix4fv(cameraUniform, 1, GL_FALSE, glm::value_ptr(glm::perspective(45.f, 1.f, 0.1f, 500.f) * V));

			GLuint thresholdUniform = glGetUniformLocation(GLuint(voxelShader), "nutrientThreshold");
			glUniform1f(thresholdUniform, panel::nutrientThreshold);

			glDrawArraysInstanced(GL_TRIANGLES, 0, 36, SOIL_X_LENGTH * SOIL_Y_LENGTH * SOIL_Z_LENGTH);
		}

		
		if (panel::renderPheremones) {
			//render the pheromones
			pheremonesShader.use();
			glBindVertexArray(pheremones_vertexArray);
			glBindBuffer(GL_ARRAY_BUFFER, pheremones_instanceTransformBuffer);
			GLuint cameraUniform = glGetUniformLocation(GLuint(pheremonesShader), "cameraMat");
			glUniformMatrix4fv(cameraUniform, 1, GL_FALSE, glm::value_ptr(glm::perspective(45.f, 1.f, 0.1f, 500.f) * V));

			glDrawArraysInstanced(GL_TRIANGLES, 0, 36, instancedPheremoneData.size());
		}

    panel::updateMenu();
    // Swap buffers and poll events
    glfwSwapBuffers(window);

		//update the time it took to compute this iteration of the loop
		auto end = std::chrono::steady_clock::now();
		std::chrono::duration<double> diff = end - previous_time;
		frameTime = diff.count();
  }
  glfwDestroyWindow(window);
  glfwTerminate();
  return EXIT_SUCCESS;
}
