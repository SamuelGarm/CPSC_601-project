#version 330 core
out vec4 color;

in vec3 norm;
in vec3 FragPos;
in vec3 col;

void main() {
	vec3 lightDir = normalize(vec3(1, 5, 3) - FragPos);
	float diff = max(0, dot(norm, lightDir));
	float amb = 0.6;
	color = vec4((diff + amb) * col, 1);
}
