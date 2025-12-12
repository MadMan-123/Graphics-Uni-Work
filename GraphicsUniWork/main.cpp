
#include <druid.h>
#include <iostream>
#include "Gbuffer.h"



static Application* app = nullptr;

static GBuffer gBuffer = { 0 };
static u32 gBufferShader = 0;
static u32 gBufferLightingShader = 0;


// Framebuffer for off-screen rendering
static Framebuffer mainFBO = { 0 };
static Framebuffer skyboxFBO = { 0 };
static Framebuffer postProcessFBO = { 0 };

// Screen quad for post-processing
static Mesh* screenQuadMesh = nullptr;

// Skybox resources
static Mesh* skyboxMesh = nullptr;
static u32 cubeMapTexture = 0;
static u32 skyboxShader = 0;
static u32 skyboxViewLoc = 0;
static u32 skyboxProjLoc = 0;

// Post-processing shader
static u32 fboShader = 0;
// debug: show gbuffer targets
static bool debugShowGBuffer = false;
// Camera
static Camera camera = { 0 };
static f32 currentYaw = 0.0f;
static f32 currentPitch = 0.0f;
static const f32 camMoveSpeed = 3.0f;
static const f32 camRotateSpeed = 2.0f;

// Window dimensions
static u32 windowWidth = 1280;
static u32 windowHeight = 720;

static f32 FOV = 80.f;

void render(f32 dt);

// Forward declarations
static void handleCameraInput(f32 dt);
//models
Model* duckModel = NULL;
Model* shieldModel = NULL;
Model* sphere = NULL;


//shaders
u32 enviromentShader = 0;
u32 geometryShader = 0;
u32 lightingSphereShader = 0;
//textures
u32 metalTexture = 0;

Transform modelTransforms[3] = {
    {{0.0f, 0.0f, 0.0f}, quatIdentity(), v3Scale(v3One,0.1f)},
    {{2.0f, 0.0f, 0.0f}, quatMul(quatIdentity() ,quatFromEuler({90,0,0})), v3One},
    {{0.0f, 0.0f, 0.0f}, quatIdentity(), v3One}
};

#define UNIFORM_NAME_SIZE 64
#define MAX_LIGHTS 10
Vec3 LightingPositions[MAX_LIGHTS] = {0};
Vec3 LightingColors[MAX_LIGHTS] = {0};
float LightingRadii[MAX_LIGHTS] = {0};

// Cached uniform locations (populated in init())
static i32 geometryViewProjLoc = -1;
// GBuffer shader uniforms
static i32 gBufferDiffuseLoc = -1;
static i32 gBufferSpecularLoc = -1;
// FBO shader uniform
static i32 fboScreenTextureLoc = -1;
// Lighting shader uniforms
static i32 gPositionLoc = -1;
static i32 gNormalLoc = -1;
static i32 gAlbedoSpecLoc = -1;
static i32 gSkyboxTexLoc = -1;
static i32 gEnvMapLoc = -1;
static i32 gEnvIntensityLoc = -1;
static i32 gSmoothnessLoc = -1;
static i32 lightingSphereColourLoc = -1;
static i32 lightingPosLocs[MAX_LIGHTS] = { -1 };
static i32 lightingColorLocs[MAX_LIGHTS] = { -1 };
static i32 lightingConstantsLocs[MAX_LIGHTS] = { -1 };
static i32 lightingLinearsLocs[MAX_LIGHTS] = { -1 };
static i32 lightingQuadraticsLocs[MAX_LIGHTS] = { -1 };
static i32 lightingIntensityLocs[MAX_LIGHTS] = { -1 };
static i32 lightingRadiiLocs[MAX_LIGHTS] = { -1 };

f32 randomRange(f32 min, f32 max)
{
	f32 random = ((f32)rand()) / (f32)RAND_MAX;
	f32 range = max - min;
	return (random * range) + min;
}

void init()
{
    //seed random
    srand((u32)time(NULL));
    //loop to setup lights
    for (auto i{ 0u }; i < MAX_LIGHTS; i++)
    {
        Vec3 randPos = { randomRange(-5.0f,5.0f), randomRange(0.5f,3.0f), randomRange(-5.0f,5.0f) };
        LightingPositions[i] = randPos;
        Vec3 randColor = { randomRange(0.0f,1.0f), randomRange(0.0f,1.0f), randomRange(0.0f,1.0f) };
        LightingColors[i] = randColor;
        float randRadius = randomRange(3.0f,5.0f);
        LightingRadii[i] = randRadius;
    }


    initCamera(&camera,
        { 0.0f, 0.0f, 5.0f },  // position
        FOV,               // FOV
        (f32)windowWidth / (f32)windowHeight,  // aspect
        0.1f, 100.0f);       // near/far


    const char* faces[6] = {
        "../res/Skybox/right.png",
        "../res/Skybox/left.png",
        "../res/Skybox/top.png",
        "../res/Skybox/bottom.png",
        "../res/Skybox/front.png",
        "../res/Skybox/back.png"
    };

    cubeMapTexture = createCubeMapTexture(faces, 6);
    if (cubeMapTexture == 0)
    {
        WARN("Failed to load skybox textures - using fallback");
    }

    skyboxMesh = createSkyboxMesh();
    if (!skyboxMesh)
    {
        ERROR("Failed to create skybox mesh!");
        return;
    }

    // get the shader from resource manager
    u32 skyboxID = 0;
    findInMap(&resources->shaderIDs, "Skybox", &skyboxID);
    skyboxShader = resources->shaderHandles[skyboxID];

    // Cache uniform locations
    skyboxViewLoc = glGetUniformLocation(skyboxShader, "view");
    skyboxProjLoc = glGetUniformLocation(skyboxShader, "projection");

    // Enable seamless cubemap sampling
    #ifdef GL_TEXTURE_CUBE_MAP_SEAMLESS
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    #endif


    // Create screen quad for FBO rendering
    screenQuadMesh = createQuadMesh();
    if (!screenQuadMesh)
    {
        ERROR("Failed to create screen quad mesh!");
        return;
    }


    i32 FBOIDReturn = -1;
    if (!findInMap(&resources->shaderIDs, "FBOShader", &FBOIDReturn))
    {
        WARN("Failed to get FBO shader - post-processing disabled");
    }
       
    u32 FBOID = (u32)FBOIDReturn;
    fboShader = resources->shaderHandles[FBOID];
    
    //setup GBuffer
    gBuffer = createGBuffer(windowWidth, windowHeight);
   
    //setup gbuffer shaders
    i32 gBufferShaderIDReturn = -1;
    if (!findInMap(&resources->shaderIDs, "GBuffer", &gBufferShaderIDReturn))
    {
		WARN("Failed to get GBuffer shader");
	}
    u32 gBufferShaderID = (u32)gBufferShaderIDReturn;
    gBufferShader = resources->shaderHandles[gBufferShaderID];

	i32 gBufferLightingShaderIDReturn = -1;
    if (!findInMap(&resources->shaderIDs, "Lighting", &gBufferLightingShaderIDReturn))
    {
        WARN("Failed to get GBuffer Lighting shader");
    }

    u32 gBufferLightingShaderID = (u32)gBufferLightingShaderIDReturn;
    gBufferLightingShader = resources->shaderHandles[gBufferLightingShaderID];

    // Skybox FBO 
    skyboxFBO = createFramebuffer(windowWidth, windowHeight, GL_RGBA8, false);

    // Main scene FBO with depth buffer 
    mainFBO = createFramebuffer(windowWidth, windowHeight, GL_RGBA8, true);

    // Post-process FBO 
    postProcessFBO = createFramebuffer(windowWidth, windowHeight, GL_RGBA8, false);

    //Get model data 
    u32 duckID = 0;
    findInMap(&resources->modelIDs,"Duck Model.fbx",&duckID);
    duckModel = &resources->modelBuffer[duckID];

    u32 sheildID = 0;
    findInMap(&resources->modelIDs,"Shield_Crusader.fbx",&sheildID);
	shieldModel = &resources->modelBuffer[sheildID];

    u32 sphereID = 0;
    findInMap(&resources->modelIDs,"Ball.obj",&sphereID);
    sphere = &resources->modelBuffer[sphereID];

    //Get shaders
    u32 envShaderID = 0;
    findInMap(&resources->shaderIDs,"eMapping",&envShaderID);
    enviromentShader = resources->shaderHandles[envShaderID];

    u32 geomShaderID = 0;
    findInMap(&resources->shaderIDs,"Geo",&geomShaderID);
    geometryShader = resources->shaderHandles[geomShaderID];

    u32 lightSphereShaderID = 0;
    if (!findInMap(&resources->shaderIDs, "LightingSphere", &lightSphereShaderID))
    {
        WARN("Failed to get Light Sphere shader");
    }
   
    lightingSphereShader = resources->shaderHandles[lightSphereShaderID];

    // Cache uniform locations for shaders to avoid repeated lookups
    if (geometryShader != 0)
        geometryViewProjLoc = glGetUniformLocation(geometryShader, "viewProj");

    if (gBufferShader != 0) {
        gBufferDiffuseLoc = glGetUniformLocation(gBufferShader, "diffuse");
        gBufferSpecularLoc = glGetUniformLocation(gBufferShader, "specular");
    }

    if (fboShader != 0)
        fboScreenTextureLoc = glGetUniformLocation(fboShader, "screenTexture");

    if (gBufferLightingShader != 0) {
        gPositionLoc = glGetUniformLocation(gBufferLightingShader, "gPosition");
        gNormalLoc = glGetUniformLocation(gBufferLightingShader, "gNormal");
        gAlbedoSpecLoc = glGetUniformLocation(gBufferLightingShader, "gAlbedoSpec");
        gSkyboxTexLoc = glGetUniformLocation(gBufferLightingShader, "skyboxTex");
        gEnvMapLoc = glGetUniformLocation(gBufferLightingShader, "envMap");
        gEnvIntensityLoc = glGetUniformLocation(gBufferLightingShader, "envIntensity");
        gSmoothnessLoc = glGetUniformLocation(gBufferLightingShader, "smoothness");
        // cache per-light array element locations
        for (int i = 0; i < MAX_LIGHTS; ++i) {
            char name[UNIFORM_NAME_SIZE];
            sprintf_s(name, UNIFORM_NAME_SIZE, "lightPositions[%d]", i);
            lightingPosLocs[i] = glGetUniformLocation(gBufferLightingShader, name);
            sprintf_s(name, UNIFORM_NAME_SIZE, "lightColours[%d]", i);
            lightingColorLocs[i] = glGetUniformLocation(gBufferLightingShader, name);
            sprintf_s(name, UNIFORM_NAME_SIZE, "lightConstants[%d]", i);
            lightingConstantsLocs[i] = glGetUniformLocation(gBufferLightingShader, name);
            sprintf_s(name, UNIFORM_NAME_SIZE, "lightLinears[%d]", i);
            lightingLinearsLocs[i] = glGetUniformLocation(gBufferLightingShader, name);
            sprintf_s(name, UNIFORM_NAME_SIZE, "lightQuadratics[%d]", i);
            lightingQuadraticsLocs[i] = glGetUniformLocation(gBufferLightingShader, name);
            sprintf_s(name, UNIFORM_NAME_SIZE, "lightIntensity[%d]", i);
            lightingIntensityLocs[i] = glGetUniformLocation(gBufferLightingShader, name);
            sprintf_s(name, UNIFORM_NAME_SIZE, "lightRadii[%d]", i);
            lightingRadiiLocs[i] = glGetUniformLocation(gBufferLightingShader, name);
        }
    }

    if (lightingSphereShader != 0)
        lightingSphereColourLoc = glGetUniformLocation(lightingSphereShader, "colour");

    //Get textures
    u32 metalTextureID = 0;
    findInMap(&resources->textureIDs, "metal.jpg", &metalTextureID);
    metalTexture = resources->textureHandles[metalTextureID];
    INFO("Initialization complete!");
}

void update(f32 dt)
{
    // Handle camera movement
    handleCameraInput(dt);


    for (auto i{ 0u }; i < MAX_LIGHTS; ++i)
    {
        //animate the lights in a circle
        float speed = 0.5f + i * 0.1f;
        float angle = SDL_GetTicks() / 1000.0f * speed;
        LightingPositions[i].x = cosf(angle) * LightingRadii[i];
        LightingPositions[i].z = sinf(angle) * LightingRadii[i];
    }

    // Update projection matrix 
    camera.projection = mat4Perspective(
        radians(70.0f),
        (f32)windowWidth / (f32)windowHeight,
        0.1f, 100.0f
    );



}

void renderSkybox()
{
    bindFramebuffer(&skyboxFBO);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(skyboxShader);

    // Get view matrix without translation
    Mat4 skyboxView = getView(&camera, true);  
    glUniformMatrix4fv(skyboxViewLoc, 1, GL_FALSE, &skyboxView.m[0][0]);
    glUniformMatrix4fv(skyboxProjLoc, 1, GL_FALSE, &camera.projection.m[0][0]);

    glBindVertexArray(skyboxMesh->vao);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    unbindFramebuffer();
}

void forwardRenderPass()
{
    // Ensure the main scene FBO has the scene depth from the GBuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mainFBO.fbo);
    glBlitFramebuffer(0, 0, windowWidth, windowHeight,
                      0, 0, windowWidth, windowHeight,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    bindFramebuffer(&mainFBO);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    // Geom shader (explode)
    glUseProgram(geometryShader);
    Mat4 vp = getViewProjection(&camera);
    glUniformMatrix4fv(geometryViewProjLoc, 1, GL_FALSE, &vp.m[0][0]);
    updateShaderMVP(geometryShader, modelTransforms[1], camera);
    draw(shieldModel, geometryShader, true);

    // restore default framebuffer binding
    unbindFramebuffer();
}

void geometryPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(gBufferShader);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, metalTexture);
    if (gBufferDiffuseLoc != -1) glUniform1i(gBufferDiffuseLoc, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, metalTexture);
    if (gBufferSpecularLoc != -1) glUniform1i(gBufferSpecularLoc, 1);

    // Duck
    updateShaderMVP(gBufferShader, modelTransforms[0], camera);
    draw(duckModel, gBufferShader, false); 



    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
}

void lightingPass()
{
    // Render lighting into the main scene FBO 
    bindFramebuffer(&mainFBO);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(gBufferLightingShader);

    // Bind GBuffer textures to texture units and assign sampler uniforms
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gBuffer.positionTex);
    if (gPositionLoc != -1) glUniform1i(gPositionLoc, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gBuffer.normalTex);
    if (gNormalLoc != -1) glUniform1i(gNormalLoc, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gBuffer.albedoSpecTex);
    if (gAlbedoSpecLoc != -1) glUniform1i(gAlbedoSpecLoc, 2);


    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, skyboxFBO.texture);
    if (gSkyboxTexLoc != -1) glUniform1i(gSkyboxTexLoc, 3);

    // Bind environment cubemap for reflections
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapTexture);
    if (gEnvMapLoc != -1) glUniform1i(gEnvMapLoc, 4);

    const float envIntensityVal = 2.5f; 
    const float smoothnessVal = 1.0f;
    if (gEnvIntensityLoc != -1) glUniform1f(gEnvIntensityLoc, envIntensityVal);
    if (gSmoothnessLoc != -1) glUniform1f(gSmoothnessLoc, smoothnessVal);

    // send light uniforms
    for (auto i{ 0u }; i < MAX_LIGHTS; ++i)
    {
        // Normalize stored colour into 0-1 and separate brightness
        float r = LightingColors[i].x;
        float g = LightingColors[i].y;
        float b = LightingColors[i].z;
        float intensity = r;
        if (g > intensity) intensity = g;
        if (b > intensity) intensity = b;
        float colorVal[3] = {0.0f, 0.0f, 0.0f};
        if (intensity > 1e-6f)
        {
            colorVal[0] = r / intensity;
            colorVal[1] = g / intensity;
            colorVal[2] = b / intensity;
        }
        else
        {
            colorVal[0] = colorVal[1] = colorVal[2] = 0.0f;
            intensity = 0.0f;
        }

        if (lightingPosLocs[i] != -1) glUniform3fv(lightingPosLocs[i], 1, &LightingPositions[i].x);
        if (lightingColorLocs[i] != -1) glUniform3fv(lightingColorLocs[i], 1, colorVal);

        const f32 constant = 1.0f;
        const f32 linear = 0.7f;
        const f32 quadratic = 1.8f;

        if (lightingConstantsLocs[i] != -1) glUniform1f(lightingConstantsLocs[i], constant);
        if (lightingLinearsLocs[i] != -1) glUniform1f(lightingLinearsLocs[i], linear);
        if (lightingQuadraticsLocs[i] != -1) glUniform1f(lightingQuadraticsLocs[i], quadratic);

        // making the lights default a little brighter
        intensity *= 20.0f;
        if (lightingIntensityLocs[i] != -1) glUniform1f(lightingIntensityLocs[i], intensity);
        if (lightingRadiiLocs[i] != -1) glUniform1f(lightingRadiiLocs[i], LightingRadii[i]);
    }

    if (debugShowGBuffer && fboShader != 0 && screenQuadMesh)
    {
        bindFramebuffer(&mainFBO);
        glDisable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(fboShader);

        int w = (int)mainFBO.width;
        int h = (int)mainFBO.height;

        glViewport(0, h/2, w/2, h/2);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gBuffer.positionTex);
        glBindVertexArray(screenQuadMesh->vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glViewport(w/2, h/2, w/2, h/2);
        glBindTexture(GL_TEXTURE_2D, gBuffer.normalTex);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glViewport(0, 0, w/2, h/2);
        glBindTexture(GL_TEXTURE_2D, gBuffer.albedoSpecTex);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glViewport(w/2, 0, w/2, h/2);
        glBindTexture(GL_TEXTURE_2D, skyboxFBO.texture);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);
        // restore
        glViewport(0, 0, windowWidth, windowHeight);
        glEnable(GL_DEPTH_TEST);
        unbindFramebuffer();
        return;
    }
    if (screenQuadMesh)
    {
        glBindVertexArray(screenQuadMesh->vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    glEnable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mainFBO.fbo);
    glBlitFramebuffer(0, 0, windowWidth, windowHeight,
                      0, 0, windowWidth, windowHeight,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    // Render the light spheres into mainFBO
    glBindFramebuffer(GL_FRAMEBUFFER, mainFBO.fbo);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(lightingSphereShader);
    for (auto i{ 0u }; i < MAX_LIGHTS; i++)
    {
        updateShaderMVP(lightingSphereShader, { LightingPositions[i], quatIdentity(), v3Scale(v3One,0.1f) }, camera);
        if (lightingSphereColourLoc != -1) glUniform3fv(lightingSphereColourLoc, 1, &LightingColors[i].x);
        draw(sphere, lightingSphereShader,false);
    }

    // restore default framebuffer
    unbindFramebuffer();
}
void render(f32 dt)
{
    f32 t = (f32)SDL_GetTicks() / 1000.0f;
    updateCoreShaderUBO(t, &camera.pos);
    renderSkybox();     // This should render normally with depth disabled
    bindFramebuffer(&mainFBO);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);     // fullscreen quad doesn't need culling

    glUseProgram(fboShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skyboxFBO.texture);
    if (fboScreenTextureLoc != -1)
        glUniform1i(fboScreenTextureLoc, 0);

    glBindVertexArray(screenQuadMesh->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);

    // Restore
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

      geometryPass();

      lightingPass();

       forwardRenderPass();

      if (fboShader != 0)
      {
          bindFramebuffer(&postProcessFBO);
          glDisable(GL_DEPTH_TEST);
          glDepthMask(GL_FALSE);
          glDisable(GL_CULL_FACE);

          glClear(GL_COLOR_BUFFER_BIT);

          glUseProgram(fboShader);
          glActiveTexture(GL_TEXTURE0);
          glBindTexture(GL_TEXTURE_2D, mainFBO.texture);
          if (fboScreenTextureLoc != -1)
            glUniform1i(fboScreenTextureLoc, 0);

          glBindVertexArray(screenQuadMesh->vao);
          glDrawArrays(GL_TRIANGLES, 0, 6);
          glBindVertexArray(0);

          glDepthMask(GL_TRUE);
          unbindFramebuffer();
      }

       glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(fboShader);
    glActiveTexture(GL_TEXTURE0);

    u32 finalTexture =
        (fboShader != 0) ? postProcessFBO.texture : mainFBO.texture;

    glBindTexture(GL_TEXTURE_2D, finalTexture);
    if (fboScreenTextureLoc != -1)
        glUniform1i(fboScreenTextureLoc, 0);

    glBindVertexArray(screenQuadMesh->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);

    // Restore state for next frame
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void destroy()
{
    INFO("Cleaning up...");

    // Destroy framebuffers
    destroyFramebuffer(&mainFBO);
    destroyFramebuffer(&skyboxFBO);
    destroyFramebuffer(&postProcessFBO);

    // Destroy meshes
    if (screenQuadMesh)
    {
        freeMesh(screenQuadMesh);
        screenQuadMesh = nullptr;
    }

    if (skyboxMesh)
    {
        freeMesh(skyboxMesh);
        skyboxMesh = nullptr;
    }

    INFO("Cleanup complete!");
}

void processInput(void* appData)
{
    Application* app = (Application*)appData;
    SDL_PumpEvents();

    // get the current state of the keyboard
    SDL_Event evnt;
    while (SDL_PollEvent(&evnt)) // get and process events
    {
        // pass imgui events
        switch (evnt.type)
        {
            // if the quit event is triggered then change the state to exit
        case SDL_EVENT_QUIT:
            app->state = EXIT;
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            checkForGamepadConnection(&evnt);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            checkForGamepadRemoved(&evnt);
            break;
        default:;
        }
    }
}
static void handleCameraInput(f32 dt)
{
    // WASD movement
    if (isKeyDown(KEY_W))
        moveForward(&camera, camMoveSpeed * dt);
    if (isKeyDown(KEY_S))
        moveForward(&camera, -camMoveSpeed * dt);
    if (isKeyDown(KEY_A))
        moveRight(&camera, -camMoveSpeed * dt);
    if (isKeyDown(KEY_D))
        moveRight(&camera, camMoveSpeed * dt);

    // Space/Ctrl for up/down
    if (isKeyDown(KEY_SPACE))
        camera.pos.y += camMoveSpeed * dt;
    if (isKeyDown(KEY_LCTRL))
        camera.pos.y -= camMoveSpeed * dt;

    // Mouse look (hold right mouse button)
    if (isMouseDown(SDL_BUTTON_RIGHT))
    {
        // Get the mouse delta
        f32 x, y;
        getMouseDelta(&x, &y);

        // apply the mouse delta to the camera
        currentYaw += -x * (camRotateSpeed)*dt;
        currentPitch += -y * (camRotateSpeed)*dt;

        // 89 in radians
        f32 goal = radians(89.0f);
        // Clamp pitch to avoid gimbal lock
        currentPitch = clamp(currentPitch, -goal, goal);

        // Create yaw quaternion based on the world-up vector
        Vec4 yawQuat = quatFromAxisAngle(v3Up, currentYaw);
        Vec4 pitchQuat = quatFromAxisAngle(v3Right, currentPitch);
        camera.orientation = quatNormalize(quatMul(yawQuat, pitchQuat));
    }

    // ESC to quit
    if (isKeyDown(KEY_ESCAPE))
    {
        app->state = EXIT;
    }
}

int main(int argc, char** argv)
{
    app = createApplication(init, update, render, destroy);

    if (!app)
    {
        ERROR("Failed to create application!");
        return -1;
    }
    app->width = windowWidth;
    app->height = windowHeight;
    app->inputProcess = processInput;

    // Run the main loop
    run(app);
    INFO("Application terminated.");
    return 0;
}

