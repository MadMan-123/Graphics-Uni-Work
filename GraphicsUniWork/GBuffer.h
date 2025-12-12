#pragma once
#include <druid.h>


// GBuffer for deferred rendering
typedef struct GBuffer {
    u32 fbo;
    u32 positionTex;
    u32 normalTex;
    u32 albedoSpecTex;
    u32 depthTex;
} GBuffer;
GBuffer createGBuffer(u32 width, u32 height);

