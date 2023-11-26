#version 330 core
layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;

//instanced 
layout (location = 2) in mat4 instanceMatrix; //uses 2,3,4,5
layout (location = 6) in float nutrient;

uniform mat4 cameraMat;
uniform float nutrientThreshold;

out vec3 norm;
out vec3 FragPos;
out vec3 vertCol;
out float vertAlpha;

vec3 lightBrown = vec3(205, 133, 63) / 255.f;
vec3 darkBrown = vec3(101, 67, 33) / 255.f;

void main() {
	vertAlpha = nutrient >= nutrientThreshold ? 1 : 0;
	vertCol = mix(lightBrown, darkBrown, nutrient);
	norm = in_norm;
	FragPos = vec3(instanceMatrix * vec4(in_pos, 1.0));
	gl_Position = cameraMat * instanceMatrix * vec4(in_pos, 1.0);
}
