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
	camera.cameraSpeed = yoffset;
}
void errorCallback(int error, const char* description) {
  std::cout << "GLFW error: " << error << " - " << description << "\n";
  throw std::runtime_error("Failed to create GLFW window.");
}

struct Agent {
	enum State {SEARCHING, RETURNING};
	State state = State::SEARCHING;
	glm::ivec3 direction = glm::ivec3(0);
  glm::ivec3 position; //integer representation of position in the matrix 
};

struct AgentVoxel {
	//stores how many agents of each type are in a single 'agent voxel'
	float pheromoneA = 0;
	float pheromoneB = 0;
};

struct SoilVoxel {
	float nutrient = 1;
	bool isSoil = true; //if the soil is actually there or if it is now 'root'
};

struct soilRenderData {
	glm::mat4 transform = glm::mat4(1);
	float nutrient = 1;
};

struct agentRenderData {
	glm::mat4 transform = glm::mat4(1);
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
				soil.at(x, y, z).nutrient = 50.f / (shortestDistance + 50.f);
			}
		}
	}
}

void stepSimulation(VoxelGrid<SoilVoxel>& soil, VoxelGrid<AgentVoxel>& pheromones, std::vector<Agent>& agents) {
	for (Agent& agent : agents) {
		if(agent.state == agent.SEARCHING) {
			//patterns are specified as coordinates relative to the agents local frame <Heading, Right, Up> (They should be symetrical)
			//pattern 1 is a search of the 3x3 grid directly in front
			std::vector<glm::ivec3> pattern1 = { {1,0,0}, {1,0,1}, {1,0,-1}, {1,1,0}, {1,-1,0}, {1,1,1}, {1,1,-1}, {1,-1,1}, {1,-1,-1} };

			//set the pattern you want to use
			std::vector<glm::ivec3>& pattern = pattern1;
			float nutrientWeight = 1;
			float pheremoneWeight = 1;

			//create the coordinate frame
			glm::ivec3 front = agent.direction;
			glm::ivec3 right = agent.direction.x == 0 ? glm::ivec3(1, 0, 0) : glm::ivec3(0, 1, 0);
			glm::ivec3 up = glm::cross(front, right);

			//calculate the influinces from each point in the pattern
			std::vector<std::pair<glm::vec3, float>> weights;
			for (glm::ivec3 offset : pattern) {
				glm::ivec3 searchLoc = front * offset[0] + right + offset[1] + up * offset[2]; //the location in the agent grid
				glm::ivec3 soilLoc = floor(searchLoc / 3); //the location in the soil grid
				
				float nutrient = soil.at(soilLoc.x, soilLoc.y, soilLoc.z).nutrient;
				float pheremone = pheromones.at(searchLoc.x, searchLoc.y, searchLoc.z).pheromoneB;
				float weight = nutrient * nutrientWeight + pheremone * pheremoneWeight;
				weights.push_back(std::pair<glm::ivec3, float>(searchLoc, weight));
			}

			//go through all the weights that have been calculated and choose a direction
			std::pair<glm::ivec3, float> best(glm::ivec3(0), 0);
			for (auto& e : weights) {
				if (e.second > best.second)
					best = e;
			}
			if (best.second == 0) {
				//if there is no best then choose a random direction
				glm::vec3 dir = front + glm::linearRand<int>(-1, 1) * right + glm::linearRand<int>(-1, 1) * up;
				best.first = dir;
			}
			glm::ivec3 direction = best.first - agent.position;
			agent.direction = direction;
		}
		else if (agent.state == agent.RETURNING) {

		}
	}
	for (Agent& agent : agents) {
		//loop over each agent and move it
		//if the agent encounters a soil voxel eat some nutrient and make the agent want to follow the return pheromones
		glm::ivec3 nextPos = agent.position + agent.direction;
		glm::ivec3 nextSoilPos = floor(nextPos / 3);
		SoilVoxel soilVox = soil.at(nextSoilPos.x, nextSoilPos.y, nextSoilPos.z);
		if (soilVox.isSoil) {
			soilVox.nutrient -= 1;
			if (soilVox.nutrient < 0)
				soilVox.isSoil = false;
			agent.state = agent.RETURNING;
			agent.direction *= -1; //turn it around to increase the chance of it sucessfully finding the path back
		}
		else {
			agent.position = nextPos;
			if(agent.state == agent.SEARCHING)
				pheromones.at(agent.position.x, agent.position.y, agent.position.z).pheromoneA += 1;
			else if (agent.state == agent.RETURNING)
				pheromones.at(agent.position.x, agent.position.y, agent.position.z).pheromoneB += 1;
		}
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
	soilRenderData* instancedVoxelData = new soilRenderData[SOIL_X_LENGTH * SOIL_Y_LENGTH * SOIL_Z_LENGTH];
	std::vector<agentRenderData> instancedAgentData;

	//simulation state variables
	std::vector<Agent> agents;
	VoxelGrid<AgentVoxel> pheremones(SOIL_X_LENGTH * 3, SOIL_Y_LENGTH * 3, SOIL_Z_LENGTH * 3);
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

	glEnableVertexAttribArray(6);
	glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4) + sizeof(float), (void*)(4 * sizeof(glm::vec4)));
	glVertexAttribDivisor(6, 1);

	/*
	* Set up openGL structures for the boids
	*/
	ShaderProgram boidShader("shaders/boid.vert", "shaders/boid.frag");
	GLuint boids_vertexArray; //stores the state of how to render and interpert that buffer data
	GLuint boids_vertexBuffer; //stores the vertex data in a buffer on the GPU
	GLuint boids_instanceTransformBuffer; //stores data that is used for each instance of voxels
	glGenVertexArrays(1, &boids_vertexArray); 
	glBindVertexArray(boids_vertexArray);
	glGenBuffers(1, &boids_vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, boids_vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_data), cube_data, GL_STATIC_DRAW);

	//set up vertex data for the cube
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)0);
	
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));

	//set up instancing buffers buffers
	glGenBuffers(1, &boids_instanceTransformBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, boids_instanceTransformBuffer);
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

	generateSoil(soil);

  //set up simulation (columns sweep the x, rows sweep the y, layers sweep the z
  for (int col = 0; col < SOIL_X_LENGTH; col++) {
		for (int row = 0; row < SOIL_Y_LENGTH; row++) {
			for (int lay = 0; lay < SOIL_Z_LENGTH; lay++) {
				int index = row + col * SOIL_Y_LENGTH + lay * SOIL_X_LENGTH * SOIL_Y_LENGTH;
				instancedVoxelData[index].transform = glm::translate(glm::mat4(1), glm::vec3(col, row, lay));
				instancedVoxelData[index].nutrient = soil.at(col, row, lay).nutrient;
			}
		}
  }

	Agent a = Agent();
	a.state = a.SEARCHING;
	a.direction = glm::ivec3(1, 0, 0);
	agents.push_back(a);

  using namespace std::chrono;

  double time_step = panel::dt; // The time step size
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
    //
    // updates from panel
    //
    //if (panel::resetView)
      //view.camera.reset();
    //TODO reimplement reset logic
    //if (panel::resetModel)
      //model->reset();


    //
    // simulation
    //
		/*
    if (panel::playModel || panel::stepModel) {
      auto current_time = steady_clock::now(); // Get the current time
      auto elapsed_time = duration_cast<duration<double>>(current_time - previous_time); // Calculate the time elapsed since the last update
      previous_time = current_time; // Update the previous time to the current time

      accumulator += elapsed_time.count(); // Add the elapsed time to the accumulator
      int count = 0;
      while (accumulator >= time_step && count < 500) {
        stepPhysics(time_step); // Update the physics system with the fixed time step
        accumulator -= time_step; // Subtract the fixed time step from the accumulator
        count++;
      }
      if (count == 50)
        accumulator = 0;
    }
    else {
      previous_time = steady_clock::now();
    }
		*/

		//simulation loop that uses discrete steps
		if (panel::playModel) {
			auto current_time = steady_clock::now(); // Get the current time
			auto elapsed_time = duration_cast<duration<double>>(current_time - previous_time);
			previous_time = current_time;
			accumulator += elapsed_time.count();
			if (accumulator > 2) {
				accumulator -= 2;
				stepSimulation(soil, pheremones, agents);
				std::cout << "step\n";
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


		//render the voxels
		voxelShader.use();
		glBindVertexArray(voxels_vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, voxels_instanceTransformBuffer);
    GLuint cameraUniform = glGetUniformLocation(GLuint(voxelShader), "cameraMat");
    glUniformMatrix4fv(cameraUniform, 1, GL_FALSE, glm::value_ptr(glm::perspective(45.f, 1.f, 0.1f, 500.f) * V));

		GLuint thresholdUniform = glGetUniformLocation(GLuint(voxelShader), "nutrientThreshold");
		glUniform1f(thresholdUniform, panel::nutrientThreshold);

    glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::mat4) + sizeof(float)) * SOIL_X_LENGTH * SOIL_Y_LENGTH * SOIL_Z_LENGTH, instancedVoxelData, GL_DYNAMIC_DRAW);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, SOIL_X_LENGTH * SOIL_Y_LENGTH * SOIL_Z_LENGTH);

		
		//render the boids
		/*
		boidShader.use();
		glBindVertexArray(boids_vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, boids_instanceTransformBuffer);
		cameraUniform = glGetUniformLocation(GLuint(boidShader), "cameraMat");
		glUniformMatrix4fv(cameraUniform, 1, GL_FALSE, glm::value_ptr(glm::perspective(45.f, 1.f, 0.1f, 500.f) * V));

		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * NUMBER_OF_AGENTS, instancedAgentData, GL_DYNAMIC_DRAW);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 36, COLUMNS * ROWS * LAYERS);
		*/

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
