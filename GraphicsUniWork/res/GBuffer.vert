#version 410

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 normal;

out vec2 tc;
out vec3 Normal;
out vec3 FragPos;


uniform mat4 transform;
uniform mat4 model;


void main()
{
	FragPos = vec3(model*vec4(position,1.0));
	Normal = normal;	
	tc = texCoord;	

	gl_Position = transform * vec4(position, 1.0);

}