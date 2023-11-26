#pragma once

//------------------------------------------------------------------------------
// This file contains an implementation of a spherical camera
//------------------------------------------------------------------------------

#include <GL/glew.h>
#include <glm/glm.hpp>

class Camera {
public:
	Camera();

	glm::mat4 getView();
	glm::vec3 getPos();
	void setPos(glm::vec3 _position);
	void panHorizontal(float _amount); //angle on the plane defiend by up vector
	void panVertical(float _amount); //elevation angle of the camera
	void update(float _dt);

	float fspeed = 0, hspeed = 0, cameraSpeed = 0.3f;

private:
	glm::vec3 cameraPos = glm::vec3(-15.0f, 5.0f, 0.0f);
	glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 cameraDirection = glm::vec3(0.0f, 0.0f, -1.0f);
	//these are degrees
	float horizontalPan = 0.f, verticalPan = -10.0f;
};
