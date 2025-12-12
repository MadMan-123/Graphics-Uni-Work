#version 410
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;



in vec2 tc;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D diffuse;
uniform sampler2D specular;



void main()
{		
	//store frag position vector in gPosition texture
	gPosition = FragPos;
	//also store the per-fragment normals into gNormal texture
	gNormal = normalize(Normal);
	//and the diffuse per-fragment color
	vec4 texColor = texture(diffuse, tc);
	gAlbedoSpec.rgb = texColor.rgb;
    
	//store specular intensity in gAlbedoSpec alpha channel
	gAlbedoSpec.a = texture(specular, tc).r;

}