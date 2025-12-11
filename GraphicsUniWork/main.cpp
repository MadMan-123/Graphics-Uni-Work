
#include <druid.h>
#include <iostream>

// Global state
static Application* app = nullptr;

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

//shaders
u32 enviromentShader = 0;
u32 geometryShader = 0;

//textures
u32 metalTexture = 0;

Transform modelTransforms[3] = {
    {{0.0f, 0.0f, 0.0f}, quatIdentity(), v3Scale(v3One,0.1f)},
    {{2.0f, 0.0f, 0.0f}, quatMul(quatIdentity() ,quatFromEuler({90,0,0})), v3One},
    {{0.0f, 0.0f, 0.0f}, quatIdentity(), v3One}
};

void init()
{
    initCamera(&camera,
        { 0.0f, 0.0f, 5.0f },  // position
        FOV,               // FOV
        (f32)windowWidth / (f32)windowHeight,  // aspect
        0.1f, 100.0f);       // near/far


    // Cubemap faces (right, left, top, bottom, front, back)
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
    findInMap(&resources->shaderIDs, "FBOShader", &FBOIDReturn);
    
    // Compile FBO shader
    if (fboShader == -1)
    {
        WARN("Failed to get FBO shader - post-processing disabled");
    }
       
    u32 FBOID = (u32)FBOIDReturn;
    fboShader = resources->shaderHandles[FBOID];
    // Skybox FBO (no depth needed)
    skyboxFBO = createFramebuffer(windowWidth, windowHeight, GL_RGBA8, false);

    // Main scene FBO (with depth buffer for 3D rendering)
    mainFBO = createFramebuffer(windowWidth, windowHeight, GL_RGBA8, true);

    // Post-process FBO (no depth needed, just color)
    postProcessFBO = createFramebuffer(windowWidth, windowHeight, GL_RGBA8, false);

    //Get model data 
    u32 duckID = 0;
    findInMap(&resources->modelIDs,"Duck Model.fbx",&duckID);
    duckModel = &resources->modelBuffer[duckID];

    u32 sheildID = 0;
    findInMap(&resources->modelIDs,"Shield_Crusader.fbx",&sheildID);
	shieldModel = &resources->modelBuffer[sheildID];

    //Get shaders
    u32 envShaderID = 0;
    findInMap(&resources->shaderIDs,"eMapping",&envShaderID);
    enviromentShader = resources->shaderHandles[envShaderID];

    u32 geomShaderID = 0;
    findInMap(&resources->shaderIDs,"Geo",&geomShaderID);
    geometryShader = resources->shaderHandles[geomShaderID];

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

    // Update projection matrix (in case window was resized)
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

    // Get view matrix without translation (skybox follows camera)
    Mat4 skyboxView = getView(&camera, true);  // true = remove translation
    glUniformMatrix4fv(skyboxViewLoc, 1, GL_FALSE, &skyboxView.m[0][0]);
    glUniformMatrix4fv(skyboxProjLoc, 1, GL_FALSE, &camera.projection.m[0][0]);

    glBindVertexArray(skyboxMesh->vao);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    unbindFramebuffer();
}

void mainRenderPass()
{
    glUseProgram(enviromentShader);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, metalTexture);
    glUniform1i(glGetUniformLocation(enviromentShader, "albedoTexture"), 1);

    updateShaderMVP(enviromentShader, modelTransforms[0], camera);
    draw(duckModel, enviromentShader,false);


    //Geom shader
    glUseProgram(geometryShader);
   
    Mat4 vp = getViewProjection(&camera);    
    glUniformMatrix4fv(glGetUniformLocation(geometryShader,"viewProj"), 1, GL_FALSE, &vp.m[0][0]);
    updateShaderMVP(geometryShader, modelTransforms[1], camera);
    draw(shieldModel, geometryShader,true);

}

void render(f32 dt)
{
    //update core UBO
    f32 t = (f32)SDL_GetTicks();
    updateCoreShaderUBO(t, &camera.pos);
    
    //render the skybox to its own FBO
    renderSkybox();
    bindFramebuffer(&mainFBO);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw skybox texture as background
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glUseProgram(fboShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skyboxFBO.texture);
    glBindVertexArray(screenQuadMesh->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);


    mainRenderPass();

    unbindFramebuffer();


    if (fboShader != 0)
    {
        // Render to post-process FBO (or directly to screen)
        bindFramebuffer(&postProcessFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        glUseProgram(fboShader);

        // Bind the scene texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mainFBO.texture);

        // Draw screen quad
        glBindVertexArray(screenQuadMesh->vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        unbindFramebuffer();
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(fboShader);  // Reuse FBO shader as passthrough

    glActiveTexture(GL_TEXTURE0);
    u32 finalTexture = (fboShader != 0) ? postProcessFBO.texture : mainFBO.texture;
    glBindTexture(GL_TEXTURE_2D, finalTexture);

    glBindVertexArray(screenQuadMesh->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
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

