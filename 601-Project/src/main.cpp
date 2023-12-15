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
#include <glm/glm.hpp>

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

#include "settings.h"
#include "agent.h"
#include "pheromones.h"
#include "soil.h"
#include "clippingPlanes.h"
#include <thread>

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




void stepSimulation(VoxelGrid<SoilVoxel>& soil, VoxelGrid<PheromoneVoxel>& pheromones, std::vector<Agent>& agents) {
	pheromoneReactions(pheromones);
	diffusePheromones(pheromones, soil);
	evaporatePheromones(pheromones);
	stepAgents(agents, pheromones, soil);
}

void simulationThread(VoxelGrid<SoilVoxel>& soil, std::vector<Agent>& agents, VoxelGrid<PheromoneVoxel>& pheromones) {
	//spin up worker threads
	//create a job pool for the threads to pull from


	using namespace std::chrono;

	double accumulator = 0.0; // The accumulator for the remaining time

	auto previous_time = steady_clock::now(); // The time of the previous update
	double frameTime = 1.f;
	while (true) {
		//simulation loop that uses discrete steps
		if (panel::playModel || panel::stepModel) {
			if (panel::stepModel) {
				accumulator = panel::stepTime;
				panel::stepModel = false;
			}
			auto current_time = steady_clock::now(); // Get the current time
			auto elapsed_time = duration_cast<duration<double>>(current_time - previous_time);
			previous_time = current_time;
			accumulator += elapsed_time.count();
			if (accumulator >= panel::stepTime) {
				accumulator = 0;
				stepSimulation(soil, pheromones, agents);
				//std::cout << "step\n";
			}
		}
		else {
			accumulator = 0;
			previous_time = steady_clock::now();
		}



	} //end main while loop
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
	VoxelGrid<PheromoneVoxel> pheromones(SOIL_X_LENGTH * 3, SOIL_Y_LENGTH * 3, SOIL_Z_LENGTH * 3);
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

	loadSoilRenderData(soil, instancedVoxelData, panel::renderSoil == 1);
	glBindVertexArray(voxels_vertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, voxels_instanceTransformBuffer);
	glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::mat4) + sizeof(float))* instancedVoxelData.size(), instancedVoxelData.data(), GL_DYNAMIC_DRAW);

	for (int i = 0; i < NUMBER_OF_STARTING_AGENTS; i++) {
		Agent a = Agent();
		a.state = a.SEARCHING;
		a.position = glm::vec3((SOIL_X_LENGTH * 3) / 2, SOIL_Y_LENGTH * 3 - 1, (SOIL_Z_LENGTH * 3) / 2);
		agents.push_back(a);
	}

	loadAgentRenderData(agents, instancedAgentData);

	//buffer agent data
	glBindVertexArray(agents_vertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, agents_instanceTransformBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4)* instancedAgentData.size(), instancedAgentData.data(), GL_DYNAMIC_DRAW);


	//setup panel
	panel::maxPheromoneClipBounds = glm::vec3(SOIL_X_LENGTH * 3, SOIL_Y_LENGTH * 3, SOIL_Z_LENGTH * 3);
	panel::maxSoilClipBounds = glm::vec3(SOIL_X_LENGTH, SOIL_Y_LENGTH, SOIL_Z_LENGTH);

	panel::soilClipping.xClipMax = SOIL_X_LENGTH;
	panel::soilClipping.yClipMax = SOIL_Y_LENGTH;
	panel::soilClipping.zClipMax = SOIL_Z_LENGTH;

	panel::pheromoneClipping.xClipMax = SOIL_X_LENGTH * 3;
	panel::pheromoneClipping.yClipMax = SOIL_Y_LENGTH * 3;
	panel::pheromoneClipping.zClipMax = SOIL_Z_LENGTH * 3;

	std::thread SimulationThread(simulationThread, std::ref(soil), std::ref(agents), std::ref(pheromones));

	using namespace std::chrono;

	double frameTime = 1.f;
	std::chrono::system_clock::time_point start;
	std::chrono::system_clock::time_point end;
	std::srand(static_cast<unsigned>(std::time(nullptr)));
  //
  // main loop
  //
  while (!glfwWindowShouldClose(window)) {
		start = std::chrono::system_clock::now();
    glfwPollEvents();
		if(!io.WantCaptureMouse && !io.WantCaptureKeyboard)
			camera.update(frameTime);

		//buffer soil data
		if (panel::renderGround) {
			clippingPlanes* clip = nullptr;
			if (panel::useSoilClipping)
				clip = &panel::soilClipping;
			loadSoilRenderData(soil, instancedVoxelData, panel::renderSoil == 1, clip);
			glBindVertexArray(voxels_vertexArray);
			glBindBuffer(GL_ARRAY_BUFFER, voxels_instanceTransformBuffer);
			glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::mat4) + sizeof(float)) * instancedVoxelData.size(), instancedVoxelData.data(), GL_DYNAMIC_DRAW);
		}
		if (panel::renderAgents) {
			//buffer agent data
			loadAgentRenderData(agents, instancedAgentData);
			glBindVertexArray(agents_vertexArray);
			glBindBuffer(GL_ARRAY_BUFFER, agents_instanceTransformBuffer);
			glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::mat4) + sizeof(glm::vec3)) * instancedAgentData.size(), instancedAgentData.data(), GL_DYNAMIC_DRAW);
		}
		if (panel::renderPheremones) {
			//buffer pheremone data
			std::vector<PheromoneVoxel::Pheromones> filter;
			if (panel::renderFood)
				filter.push_back(PheromoneVoxel::Food);
			if (panel::renderWander)
				filter.push_back(PheromoneVoxel::Wander);
			if (panel::renderRoot)
				filter.push_back(PheromoneVoxel::Root);
			clippingPlanes* clip = nullptr;
			if (panel::usePheromoneClipping)
				clip = &panel::pheromoneClipping;
			loadPheremoneRenderData(pheromones, instancedPheremoneData, filter, clip);
			glBindVertexArray(pheremones_vertexArray);
			glBindBuffer(GL_ARRAY_BUFFER, pheremones_instanceTransformBuffer);
			glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::mat4) + sizeof(glm::vec3)) * instancedPheremoneData.size(), instancedPheremoneData.data(), GL_DYNAMIC_DRAW);
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

			glDrawArraysInstanced(GL_TRIANGLES, 0, 36, instancedVoxelData.size());
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
		//limit the FPS on fast computers
		end = std::chrono::system_clock::now();
		std::chrono::duration<double, std::milli> executionTime = end - start;
		if (executionTime.count() < 60)
		{
			std::chrono::duration<double, std::milli> delta_ms(60 - executionTime.count());
			auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
			std::this_thread::sleep_for(std::chrono::milliseconds(delta_ms_duration.count()));
		}
		std::chrono::duration<double> diff = end - end;
		frameTime = diff.count();

  }
  glfwDestroyWindow(window);
  glfwTerminate();
  return EXIT_SUCCESS;
}
