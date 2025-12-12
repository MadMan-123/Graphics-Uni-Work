#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D skyboxTex;
uniform samplerCube envMap;

//lights 
const int LIGHTS = 10;
const float AMBIENT = 0.1;

// Controls
uniform float envIntensity; 
uniform float smoothness;   // 0.0 = rough, 1.0 = smooth/mirror
uniform float lightIntensity[LIGHTS]; 

uniform vec3 lightPositions[LIGHTS];
uniform vec3 lightColours[LIGHTS];
uniform float lightConstants[LIGHTS]; 
uniform float lightLinears[LIGHTS];
uniform float lightQuadratics[LIGHTS];
uniform float lightRadii[LIGHTS];

layout(std140) uniform CoreShaderData {
	vec3 camPos; 
	float time;
} CSD;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
	vec3 FragPos = texture(gPosition, TexCoords).rgb;
	// If no geometry wrote to this pixel (position == 0), show skybox
	if (length(FragPos) < 0.001)
	{
		FragColor = texture(skyboxTex, TexCoords);
		return;
	}

	vec3 Normal = normalize(texture(gNormal, TexCoords).rgb);
	vec3 Albedo = texture(gAlbedoSpec, TexCoords).rgb;
	float Specular = texture(gAlbedoSpec, TexCoords).a;

	// base ambient
	vec3 lighting = Albedo * AMBIENT;

	vec3 viewDir = normalize(CSD.camPos - FragPos);

	// environment reflection (world-space assumed)
	vec3 R = reflect(-viewDir, Normal);
	vec3 envColor = texture(envMap, R).rgb;

	// fresnel base (dielectric)
	vec3 F0 = vec3(0.04);
	float cosV = max(dot(viewDir, Normal), 0.0);
	vec3 fresnel = fresnelSchlick(cosV, F0);

	// accumulate per-light contributions
	// shininess controls specular tightness based on smoothness
	float shininess = mix(8.0, 256.0, clamp(smoothness, 0.0, 1.0));
	for(int i = 0; i < LIGHTS; ++i)
	{
		//work out distance

	float distance = length(lightPositions[i] - FragPos);

		if(distance < lightRadii[i])
		{
			float d = distance / lightRadii[i];

			float fade = 1.0 - smoothstep(0.7, 1.0, d);      
			float falloff = 1.0 / (1.0 + d * d * 16.0);      

			float attenuation = fade * falloff;

			vec3 lightDir = normalize(lightPositions[i] - FragPos);
			float diff = max(dot(Normal, lightDir), 0.0);

			vec3 reflectDir = reflect(-lightDir, Normal);
			float specFactor = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

			float li = lightIntensity[i];

			vec3 diffuse  = diff       * Albedo   * lightColours[i] * attenuation * li;
		    vec3 specular = specFactor * Specular * lightColours[i] * attenuation * li;

			lighting += diffuse + specular;
		}

	}

	// add environment specular contribution (modulated by material specular and fresnel)
	lighting += envColor * Specular * fresnel * envIntensity * clamp(smoothness, 0.0, 1.0);

	FragColor = vec4(lighting, 1.0);
}


