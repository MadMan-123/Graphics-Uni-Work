#version 410

in vec2 tc;
in vec3 Normal;
in vec3 FragPos;

uniform vec3 colour;


out vec4 FragColour;



void main()
{		

	FragColour = vec4(colour,1.0f);
}
