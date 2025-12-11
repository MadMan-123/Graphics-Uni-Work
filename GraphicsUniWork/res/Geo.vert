//Version Number
#version 410
	
//The layout qualifers
layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec2 TexCoord;
layout (location = 2) in vec3 VertexNormal;

out VS_OUT {
	vec2 texCoords;
} vs_out;

//Uniform variable
uniform mat4 model;


//Passing out the normal and position data
out vec3 v_norm;
out vec4 v_pos; 

void main()
{
	//Assigning the normal and position data
	v_norm = VertexNormal;
	v_pos = vec4(VertexPosition, 0.0);

	vs_out.texCoords = TexCoord;
	// Pass world-space position to geometry shader (model only, not MVP)
	gl_Position = model * vec4(VertexPosition, 1.0);
}