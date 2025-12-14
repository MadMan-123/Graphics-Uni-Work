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
    gPosition = FragPos;
    gNormal = normalize(Normal);
    
    vec4 texColor = texture(diffuse, tc);
    
	//gAlbedoSpec.rgb = pow(texColor.rgb, vec3(2.2));
	gAlbedoSpec.rgb = texColor.rgb;
    gAlbedoSpec.a = texture(specular, tc).r;
}