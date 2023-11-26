#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <algorithm>

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

//simulation variables
#define NUMBER_OF_AGENTS 1
#define COLUMNS 100
#define ROWS 100
#define LAYERS 1

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

struct agent {
  glm::vec3 velocity;
  glm::vec3 position;
  unsigned short int index = 0;
};

struct agentVoxel {
	//stores how many agents of each type are in a single 'agent voxel'
	unsigned short seekers = 0; //agents looking for nutrients
	unsigned short returners = 0; //agents that have found nutrients and are returning
	unsigned short pheromoneA = 0;
	unsigned short pheromoneB = 0;
};

struct voxelRenderData {
	glm::mat4 transform = glm::mat4(1);
	float nutrient = 1;
};

struct agentRenderData {
	glm::mat4 transform = glm::mat4(1);
};



void stepPhysics(float dt) {
	return;
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
	voxelRenderData* instancedVoxelData = new voxelRenderData[COLUMNS * ROWS * LAYERS];
	agentRenderData* instancedAgentData = new agentRenderData[NUMBER_OF_AGENTS];

	//simulation state variables
	agentVoxel* voxelData = new agentVoxel[COLUMNS * 3 * ROWS * 3 * LAYERS * 3];
	agent* agents = new agent[NUMBER_OF_AGENTS];

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

  //set up simulation (columns sweep the x, rows sweep the y, layers sweep the z
  for (int col = 0; col < COLUMNS; col++) {
		for (int row = 0; row < ROWS; row++) {
			for (int lay = 0; lay < LAYERS; lay++) {
				int index = row + col * ROWS + lay * COLUMNS * ROWS;
				instancedVoxelData[index].transform = glm::translate(glm::mat4(1), glm::vec3(col, row, lay));
				instancedVoxelData[index].nutrient = glm::linearRand(0.f, 1.f);
			}
		}
  }

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
				std::cout << "hello\n";
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

    glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::mat4) + sizeof(float)) * COLUMNS * ROWS * LAYERS, instancedVoxelData, GL_DYNAMIC_DRAW);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, COLUMNS * ROWS * LAYERS);

		
		//render the boids
		boidShader.use();
		glBindVertexArray(boids_vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, boids_instanceTransformBuffer);
		cameraUniform = glGetUniformLocation(GLuint(boidShader), "cameraMat");
		glUniformMatrix4fv(cameraUniform, 1, GL_FALSE, glm::value_ptr(glm::perspective(45.f, 1.f, 0.1f, 500.f) * V));

		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * NUMBER_OF_AGENTS, instancedAgentData, GL_DYNAMIC_DRAW);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 36, COLUMNS * ROWS * LAYERS);
		

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
