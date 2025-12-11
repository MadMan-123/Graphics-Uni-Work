#version 330 core
out vec4 FragColor;
  
in vec2 TexCoords;

uniform sampler2D screenTexture;

vec4 applyGreyScale(vec4 color)
{
    float average = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
    return vec4(average, average, average, 1.0);
}


//PSX Rendering techniques:


float bayerMatrix(vec2 pos)
{
    //bayer matrix 4x4
    const float matrix[16] = float[16](
         0.0,  8.0,  2.0, 10.0,
        12.0,  4.0, 14.0,  6.0,
         3.0, 11.0,  1.0,  9.0,
        15.0,  7.0, 13.0,  5.0
    );
    
    //bitmask to essentially do mod 4 
    int x = int(pos.x) & 3;
    int y = int(pos.y) & 3;

    int index = x + y * 4;
    
    // Normalize to 0-1
    return matrix[index] / 16.0;

}
vec4 pixelateTexture(vec2 uv, float scale)
{
    // Pixelate the UV coordinates
    uv = floor(uv * scale) / scale;
    return texture(screenTexture, uv);
}

vec4 quantize(vec4 color,float levels, bool dither = false)
{

    float ditherAmount = 0;
    if(dither)
    {
        // Apply dithering using a simple Bayer matrix
        ditherAmount = bayerMatrix(gl_FragCoord.xy);
    }


    // Quantize colors
    color.rgb = floor(color.rgb * levels + ditherAmount) / levels;

    return color;
}

vec4 applyInvert(vec4 color)
{
    return vec4(1.0 - color.rgb, 1.0);
}
const float offset = 1.0 / 300.0;  

vec2 offsets[9] = vec2[](
        vec2(-offset,  offset), // top-left
        vec2( 0.0f,    offset), // top-center
        vec2( offset,  offset), // top-right
        vec2(-offset,  0.0f),   // center-left
        vec2( 0.0f,    0.0f),   // center-center
        vec2( offset,  0.0f),   // center-right
        vec2(-offset, -offset), // bottom-left
        vec2( 0.0f,   -offset), // bottom-center
        vec2( offset, -offset)  // bottom-right    
    );


vec4 edgeDetect(vec4 sampleTex[9],float strength)
{
       float kernel[9] = float[](
        -1, -1, -1,
        -1,  strength, -1,
        -1, -1, -1
    );

    vec4 result = vec4(0.0);
    for(int i = 0; i < 9; i++)
    {
        result += sampleTex[i] * kernel[i];
    }
    
    return result;

}



void main()
{ 
    //vec4 color = texture(screenTexture, TexCoords);
    // vec4 sampleTex[9];
    // for(int i = 0; i < 9; i++)
    // {
    //     sampleTex[i] = pixelateTexture(TexCoords + offsets[i], 250);
    // }

    // vec4 color = edgeDetect(sampleTex, 9.0);
    
    
   
    vec4 color = pixelateTexture(TexCoords, 850);
    color = quantize(color, 16.0, true);


    FragColor = color;


}
