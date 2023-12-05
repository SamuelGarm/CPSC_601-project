#version 330 core
out vec4 color;

in vec3 norm;
in vec3 FragPos;  
in vec3 vertCol;
in float vertAlpha;

void main() {
	if(vertAlpha < 0.5)
		discard;
	vec3 lightDir = normalize(vec3(1, 5, 3) - FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * vec3(1);
	vec3 ambient = 0.7 * vec3(1);
	vec3 result = (ambient + diffuse) * vertCol;
	color = vec4(result, 1);//vec4(max(0, dot(norm, lightDir)) * vertCol, vertAlpha);
}
