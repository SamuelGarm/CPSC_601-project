#include "Camera.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>

#include "glm/gtc/matrix_transform.hpp"

Camera::Camera() {
	glm::vec3 direction = glm::vec3(0,0,0);
	direction.x = cos(glm::radians(horizontalPan)) * cos(glm::radians(verticalPan));
	direction.y = sin(glm::radians(verticalPan));
	direction.z = sin(glm::radians(horizontalPan)) * cos(glm::radians(verticalPan));
	cameraDirection = glm::normalize(direction);
}

void Camera::update(float _dt) {
	glm::vec3 direction = glm::vec3(0, 0, 0);
	direction.x = cos(glm::radians(horizontalPan)) * cos(glm::radians(verticalPan));
	direction.y = sin(glm::radians(verticalPan));
	direction.z = sin(glm::radians(horizontalPan)) * cos(glm::radians(verticalPan));
	cameraDirection = glm::normalize(direction);

	cameraPos += glm::normalize(glm::cross(cameraDirection, cameraUp)) * cameraSpeed * hspeed;
	cameraPos += cameraSpeed * cameraDirection * fspeed;
}

glm::mat4 Camera::getView()
{
	return glm::lookAt(cameraPos, cameraPos + cameraDirection, cameraUp);
}

glm::vec3 Camera::getPos() {
	return cameraPos;
}

void Camera::setPos(glm::vec3 _position) {
	cameraPos = _position;
}

void Camera::panHorizontal(float _amount) {
	horizontalPan += _amount;
	if (horizontalPan > 360.f) {
		horizontalPan -= 360.f;
	} else if (horizontalPan < 0.f) {
		horizontalPan += 360.f;
	}
}

void Camera::panVertical(float _amount) {
	if (_amount < 0 && verticalPan > -80) {
		verticalPan += _amount;
	}
	else if (_amount > 0 && verticalPan < 80.f) {
		verticalPan += _amount;
	}
}
