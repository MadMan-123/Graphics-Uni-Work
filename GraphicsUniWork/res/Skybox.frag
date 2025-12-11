#version 330 core
out vec4 FragColor;

in vec3 TexCoord;

uniform samplerCube skybox;
// Core shader data (time + viewProj) provided via UBO
layout(std140) uniform CoreShaderData {
    vec3 camPos; 
    float time;
} CSD;
void main()
{
    FragColor = texture(skybox,TexCoord);
}
