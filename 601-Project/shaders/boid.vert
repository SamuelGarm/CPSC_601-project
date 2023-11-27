#version 330 core
layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;

//instanced 
layout (location = 2) in mat4 instanceMatrix; //uses 2,3,4,5

uniform mat4 cameraMat;

out vec3 norm;
out vec3 FragPos;


void main() {
	norm = in_norm;
	FragPos = vec3(instanceMatrix * vec4(in_pos, 1.0));
	gl_Position = cameraMat * instanceMatrix * vec4(in_pos, 1.0);
}
