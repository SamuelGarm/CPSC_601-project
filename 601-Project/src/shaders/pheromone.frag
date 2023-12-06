#version 330 core
out vec4 color;

in vec3 norm;
in vec3 FragPos;
in vec3 fragCol;

void main() {
	vec3 lightDir = normalize(vec3(1, 5, 3) - FragPos);
	color = vec4(fragCol, 1);
}
