#include <SDL3/SDL.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include "math.h"
#include "shaders.h"

typedef struct Asteroid
{
    Vector3 pos;
    float padding1;
    Vector3 vel;
    float padding2;
} Asteroid;

typedef struct UniformDataCompute
{
    float timescale;
    float numAsteroids;
    float deltaTime;
} UniformDataCompute;

typedef struct Vertex
{
    float x, y, z;
} Vertex;

typedef struct AppState
{
    bool shouldRecreateAsteroids;
    bool leftClickDown;

    Uint32 numAsteroids;
    Uint32 minDistance;
    Uint32 maxDistance;
    Uint32 cameraDistance; // KM

    float cameraRotation;
    float cameraRotationVelocity;

    float deltaTime;
    float timescale;
    int timescaleIndex;
    Uint64 timeLastIter;

    SDL_Window *window;
    SDL_GPUDevice *device;
    SDL_GPUShader *vs, *fs;
    SDL_GPUGraphicsPipeline *graphicsPipeline;
    SDL_GPUComputePipeline *computePipeline;

    SDL_GPUBuffer *vtxBuffer, *idxBuffer;
    SDL_GPUBuffer *asteroidBuf;
} AppState;

static AppState app = { 0 };
#define ASTEROID_SIZE 15000.0f
#define MAX_ASTEROIDS 4000000.0f
#define EARTH_RADIUS  6371000.0f
#define RESOLUTION_X  800.0f
#define RESOLUTION_Y  600.0f

// By pressing left or right arrow on your keyboard you can decrease/increase the time scale (how fast time goes)
const float timescales[6] = { 1.0f, 10.0f, 100.0f, 1000.0f, 2500.0f, 5000.0f };

void InitPipelines(void)
{
    app.vs =
        SDL_CreateGPUShader(app.device, &(SDL_GPUShaderCreateInfo){
                                            .code = (const Uint8 *)vtxShader,
                                            .code_size = SDL_strlen(vtxShader),
                                            .entrypoint = "main",
                                            .format = SDL_GPU_SHADERFORMAT_WGSL,
                                            .num_samplers = 0,
                                            .num_storage_buffers = 0,
                                            .num_storage_textures = 0,
                                            .num_uniform_buffers = 1,
                                            .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                                        });

    app.fs =
        SDL_CreateGPUShader(app.device, &(SDL_GPUShaderCreateInfo){
                                            .code = (const Uint8 *)frgShader,
                                            .code_size = SDL_strlen(frgShader),
                                            .entrypoint = "main",
                                            .format = SDL_GPU_SHADERFORMAT_WGSL,
                                            .num_samplers = 0,
                                            .num_storage_buffers = 0,
                                            .num_storage_textures = 0,
                                            .num_uniform_buffers = 0,
                                            .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                                        });

    app.graphicsPipeline = SDL_CreateGPUGraphicsPipeline(
        app.device,
        &(SDL_GPUGraphicsPipelineCreateInfo){
            .target_info = {
                .num_color_targets = 1,
                .color_target_descriptions =
                    (SDL_GPUColorTargetDescription[]){
                        {
                            .format = SDL_GetGPUSwapchainTextureFormat(
                                app.device, app.window),
                        },
                    }, // Note: I couldn't be bothered to do depth testing. Sorry!
            },
            .vertex_input_state = (SDL_GPUVertexInputState){
                .num_vertex_buffers = 1,
                .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]){
                    {
                        .slot = 0,
                        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                        .instance_step_rate = 0,
                        .pitch = sizeof(Vertex),
                    },
                },
                .num_vertex_attributes = 1,
                .vertex_attributes = (SDL_GPUVertexAttribute[]){
                    {
                        .buffer_slot = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                        .location = 0,
                        .offset = 0,
                    },
                },
            },
            .rasterizer_state = {
                // We'll enable backface culling here.
                // I'm not nearly good enough of a graphics programmer
                // to explain what backface culling in a simple manner,
                // but TLDR it's pretty much a free performance boost.
                .cull_mode = SDL_GPU_CULLMODE_BACK,
                .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
            },
            .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
            .vertex_shader = app.vs,
            .fragment_shader = app.fs,
        });

    app.computePipeline = SDL_CreateGPUComputePipeline(app.device, &(SDL_GPUComputePipelineCreateInfo){
                                                                       .code = (const Uint8 *)cmpShader,
                                                                       .code_size = SDL_strlen(cmpShader),
                                                                       .entrypoint = "main",
                                                                       .format = SDL_GPU_SHADERFORMAT_WGSL,
                                                                       .num_samplers = 0,
                                                                       .num_readonly_storage_buffers = 0,
                                                                       .num_readonly_storage_textures = 0,
                                                                       .num_readwrite_storage_buffers = 1,
                                                                       .num_readwrite_storage_textures = 0,
                                                                       .num_uniform_buffers = 1,
                                                                       .threadcount_x = 64,
                                                                       .threadcount_y = 1,
                                                                       .threadcount_z = 1,
                                                                   });
}

void CreateAsteroids(SDL_GPUCopyPass *copyPass)
{
    Uint32 i;

    if (app.asteroidBuf != NULL) {
        // If we're recreating the asteroids, we'll have to release the previous asteroid buffer
        // since otherwise we'd waste a TON of GPU memory and eventually crash.
        SDL_ReleaseGPUBuffer(app.device, app.asteroidBuf);
    }

    // We'll create a storage buffer which'll store each asteroid's position and velocity.
    // This will be written to in the compute shader, and then read from in the vertex shader.
    app.asteroidBuf = SDL_CreateGPUBuffer(app.device, &(SDL_GPUBufferCreateInfo){
                                                          .size = sizeof(Asteroid) * app.numAsteroids,
                                                          .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
                                                                   SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
                                                                   SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                                                      });

    SDL_GPUTransferBuffer *asteroidTransferBuffer = SDL_CreateGPUTransferBuffer(app.device, &(SDL_GPUTransferBufferCreateInfo){
                                                                                                .size = sizeof(Asteroid) * app.numAsteroids,
                                                                                                .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                                                            });

    Asteroid *asteroidData = SDL_MapGPUTransferBuffer(app.device, asteroidTransferBuffer, false);

    // This loop will create all the asteroids and randomly spread them out in Earth's orbit.
    for (i = 0; i < app.numAsteroids; i++) {
        Uint32 distance = SDL_max(SDL_rand(app.maxDistance), (Sint32)app.minDistance);

        float theta = (2.0f * SDL_PI_F * i) / app.numAsteroids;
        float finalDist = EARTH_RADIUS + distance;

        asteroidData[i].pos.x = finalDist * SDL_sinf(theta);
        asteroidData[i].pos.z = finalDist * SDL_cosf(theta);

        asteroidData[i].vel.x = (7700 * SDL_cosf(theta)) * (1 + SDL_randf() / 5);
        asteroidData[i].vel.z = (7700 * -SDL_sinf(theta)) * (1 + SDL_randf() / 5);
        // We give some initial random velocity to the Y axis to spread the asteroids out a little.
        asteroidData[i].vel.y = 1500 * SDL_randf();
    }

    SDL_UnmapGPUTransferBuffer(app.device, asteroidTransferBuffer);
    // We'll upload the transfer buffer's data to the storage buffer with this SDL_UploadToGPUBuffer call.
    SDL_UploadToGPUBuffer(
        copyPass,
        &(SDL_GPUTransferBufferLocation){
            .transfer_buffer = asteroidTransferBuffer,
            .offset = 0,
        },
        &(SDL_GPUBufferRegion){
            .buffer = app.asteroidBuf,
            .offset = 0,
            .size = sizeof(Asteroid) * app.numAsteroids,
        },
        false);

    // We don't need this transfer buffer any more, so we'll release it to free up GPU memory.
    SDL_ReleaseGPUTransferBuffer(app.device, asteroidTransferBuffer);
}

void InitBuffers(void)
{
    // We'll create our vertex & index buffers here.
    // Since we're using instancing, we only need one cube mesh for all asteroids.
    app.vtxBuffer =
        SDL_CreateGPUBuffer(app.device, &(SDL_GPUBufferCreateInfo){
                                            .size = sizeof(Vertex) * 24,
                                            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                                        });
    app.idxBuffer =
        SDL_CreateGPUBuffer(app.device, &(SDL_GPUBufferCreateInfo){
                                            .size = sizeof(Uint16) * 36,
                                            .usage = SDL_GPU_BUFFERUSAGE_INDEX,
                                        });

    // We'll have to create a transfer buffer to upload our mesh data.
    // We'll be smart here and use the same buffer for both uploads, so we'll make it large enough to fit both the vertex & index data.
    SDL_GPUTransferBuffer *meshTransferBuffer = SDL_CreateGPUTransferBuffer(app.device, &(SDL_GPUTransferBufferCreateInfo){
                                                                                            .size = (sizeof(Uint16) * 36) + (sizeof(Vertex) * 24),
                                                                                            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                                                        });

    // We'll map the transfer buffer so we can upload our cube mesh to the GPU.
    Vertex *transferData = SDL_MapGPUTransferBuffer(app.device, meshTransferBuffer, false);
    Uint16 *indexData = (Uint16 *)&transferData[24];

    // This is just a cube's raw data.
    transferData[0] = (Vertex){ -ASTEROID_SIZE, -ASTEROID_SIZE, -ASTEROID_SIZE };
    transferData[1] = (Vertex){ ASTEROID_SIZE, -ASTEROID_SIZE, -ASTEROID_SIZE };
    transferData[2] = (Vertex){ ASTEROID_SIZE, ASTEROID_SIZE, -ASTEROID_SIZE };
    transferData[3] = (Vertex){ -ASTEROID_SIZE, ASTEROID_SIZE, -ASTEROID_SIZE };
    transferData[4] = (Vertex){ -ASTEROID_SIZE, -ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[5] = (Vertex){ ASTEROID_SIZE, -ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[6] = (Vertex){ ASTEROID_SIZE, ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[7] = (Vertex){ -ASTEROID_SIZE, ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[8] = (Vertex){ -ASTEROID_SIZE, -ASTEROID_SIZE, -ASTEROID_SIZE };
    transferData[9] = (Vertex){ -ASTEROID_SIZE, ASTEROID_SIZE, -ASTEROID_SIZE };
    transferData[10] = (Vertex){ -ASTEROID_SIZE, ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[11] = (Vertex){ -ASTEROID_SIZE, -ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[12] = (Vertex){ ASTEROID_SIZE, -ASTEROID_SIZE, -ASTEROID_SIZE };
    transferData[13] = (Vertex){ ASTEROID_SIZE, ASTEROID_SIZE, -ASTEROID_SIZE };
    transferData[14] = (Vertex){ ASTEROID_SIZE, ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[15] = (Vertex){ ASTEROID_SIZE, -ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[16] = (Vertex){ -ASTEROID_SIZE, -ASTEROID_SIZE, -ASTEROID_SIZE };
    transferData[17] = (Vertex){ -ASTEROID_SIZE, -ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[18] = (Vertex){ ASTEROID_SIZE, -ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[19] = (Vertex){ ASTEROID_SIZE, -ASTEROID_SIZE, -ASTEROID_SIZE };
    transferData[20] = (Vertex){ -ASTEROID_SIZE, ASTEROID_SIZE, -ASTEROID_SIZE };
    transferData[21] = (Vertex){ -ASTEROID_SIZE, ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[22] = (Vertex){ ASTEROID_SIZE, ASTEROID_SIZE, ASTEROID_SIZE };
    transferData[23] = (Vertex){ ASTEROID_SIZE, ASTEROID_SIZE, -ASTEROID_SIZE };
    Uint16 indices[] = { 0, 1, 2, 0, 2, 3, 6, 5, 4, 7, 6, 4,
                         8, 9, 10, 8, 10, 11, 14, 13, 12, 15, 14, 12,
                         16, 17, 18, 16, 18, 19, 22, 21, 20, 23, 22, 20 };
    SDL_memcpy(indexData, indices, sizeof(indices));

    // Now that we're done copying the cube mesh data to the transfer buffer,
    // we'll unmap the transfer buffer so we can upload to the vertex & index buffers from it.
    SDL_UnmapGPUTransferBuffer(app.device, meshTransferBuffer);

    // We'll acquire a command buffer and begin a copy pass.
    // In order to upload anything to the GPU in SDLGPU, you must first begin a "copy pass".
    // Within this copy pass, you can... copy data. Once you're finished with the copy pass,
    // you call SDL_EndGPUCopyPass and then continue on with whatever else you need to do.
    SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(app.device);
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    // While we're here, we'll also upload our initial asteroid data.
    CreateAsteroids(copyPass);

    SDL_UploadToGPUBuffer(copyPass,
                          &(SDL_GPUTransferBufferLocation){
                              .transfer_buffer = meshTransferBuffer,
                              .offset = 0,
                          },
                          &(SDL_GPUBufferRegion){
                              .buffer = app.vtxBuffer,
                              .offset = 0,
                              .size = sizeof(Vertex) * 24,
                          },
                          false);

    SDL_UploadToGPUBuffer(
        copyPass,
        &(SDL_GPUTransferBufferLocation){
            .transfer_buffer = meshTransferBuffer,
            // Remember that we're using the same transfer buffer here,
            // so we'll add the vertex data size to the offset.
            .offset = sizeof(Vertex) * 24,
        },
        &(SDL_GPUBufferRegion){
            .buffer = app.idxBuffer,
            .offset = 0,
            .size = sizeof(Uint16) * 36,
        },
        false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);

    SDL_ReleaseGPUTransferBuffer(app.device, meshTransferBuffer);
}

// We're using the SDL callbacks here, since they generally play nicer with Emscripten.
// You can also run a traditional main loop if you want to, but you'll have to do a bit of screwing around to get it working with Emscripten.
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed (%s)", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app.window =
        SDL_CreateWindow("Orbit Simulation", (int)RESOLUTION_X, (int)RESOLUTION_Y, 0);
    if (app.window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed (%s)", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // We'll create the SDL_GPUDevice here. This device will be used to interface with the GPU.
    // This function takes in three arguments: The shader format(s) you support, whether or not debug mode is enabled, and additional properties.
    //
    // The shader format(s) argument allows you to say "Hey, I can give shaders in these formats!".
    // This lets you support Vulkan, DX12, Metal, and WebGPU while still using the same SDLGPU code.
    //
    // However, in this example we are only supporting WGSL, which is WebGPU's shading language.
    // (Quite frankly, I just didn't wanna rewrite the shader code in HLSL.)
    app.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_WGSL, true, NULL);
    if (app.device == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "This example only supports SDL_GPU through webgpu");
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUDevice failed (%s)", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app.numAsteroids = 100000;
    app.minDistance = 75000;
    app.maxDistance = 78000;
    app.cameraDistance = 10000; // cameraDistance is in kilometers
    app.timescale = timescales[3];
    app.timescaleIndex = 3;

    // In order to render an image to the SDL window with SDLGPU, you must call SDL_ClaimWindowForGPUDevice.
    // This creates the "swapchain", allowing you to later call SDL_(WaitAnd)AcquireGPUSwapchainTexture.
    SDL_ClaimWindowForGPUDevice(app.device, app.window);

    InitPipelines();
    InitBuffers();

    // We have to set this here, since this is part of the delta time calculations and dividing by 0 is not good.
    app.timeLastIter = SDL_GetTicksNS();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Press left or right to change timescale.");
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Press up or down to increase/decrease asteroid count.");

    // We'll return SDL_APP_CONTINUE, which tells SDL that the app initialization succeeded, and that the app should continue running.
    return SDL_APP_CONTINUE;
}

// This function runs once per frame, and therefore we'll put our GPU rendering code here.
SDL_AppResult SDL_AppIterate(void *appstate)
{
    Uint64 timeLastIter = app.timeLastIter;

    app.timeLastIter = SDL_GetTicksNS();
    app.deltaTime = (app.timeLastIter - timeLastIter) / (float)1e9;

    // This code changes the camera rotation by cameraRotationVelocity. It then also divides the velocity by 1.15f to apply some drag.
    app.cameraRotation += app.cameraRotationVelocity;
    app.cameraRotationVelocity /= 1.15f;

    // We'll acquire a command buffer here.
    // In order to run anything on the GPU, you must first acquire a GPU command buffer.
    // After you've acquired the buffer, you proceed to fill it with GPU commands. (e.g: draw this, upload that, clear this, etc...)
    // Once you've filled the command buffer with all your GPU commands, you "submit" it.
    // This sends the commands to the GPU where they'll be run.
    SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(app.device);

    // If the asteroid count's changed we'll have to upload the new asteroids.
    if (app.shouldRecreateAsteroids) {
        // Important note with the WebGPU backend: There's an edge case in WebGPU which makes it so that any swapchain texture you've
        // previously acquired will instantly be destroyed if you map a transfer buffer. I have no idea why it does that, but it does.
        // Keep that in mind!
        SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuf);

        CreateAsteroids(copyPass);
        SDL_EndGPUCopyPass(copyPass);

        app.shouldRecreateAsteroids = false;
    }

    // This code starts a "compute pass". A compute pass is a pass in which you execute compute shaders.
    // This is unlike a copy pass, which copies data, and a render pass, which renders triangles.
    //
    // A compute shader is a special kind of shader which allows you to run non-graphics related code on a GPU.
    // This example makes use of compute shaders to simulate each asteroid's orbit direcly on the GPU, without any input from the CPU.
    // This not only simplifies the code, but this also runs much better than if we were to run the simulation on the CPU.
    // Using this compute shader, I can simulate 4 million asteroids in less than a millisecond on my 6700XT, without ever uploading any data from the CPU.
    //
    // (Note that we're binding the asteroid buffer here, since SDLGPU requires R/W compute storage buffers to be bound in SDL_BeginGPUComputePass.)
    SDL_GPUComputePass *computePass = SDL_BeginGPUComputePass(cmdBuf, NULL, 0, &(SDL_GPUStorageBufferReadWriteBinding){ .buffer = app.asteroidBuf, .cycle = false }, 1);

    // We'll bind our compute pipeline here.
    SDL_BindGPUComputePipeline(computePass, app.computePipeline);

    // We'll push our uniform data here. This doesn't include all that much, just the deltaTime, amount of asteroids, and the timescale.
    SDL_PushGPUComputeUniformData(cmdBuf, 0, &(UniformDataCompute){
                                                 .deltaTime = app.deltaTime,
                                                 .numAsteroids = (float)app.numAsteroids,
                                                 .timescale = app.timescale,
                                             },
                                  sizeof(UniformDataCompute));

    // Finally, we'll dispatch our compute. This tells the GPU "hey, run this compute code please".
    // Since we've previously set our X thread size to 64, we'll have to divide the asteroid count by 64 to get our thread dispatch size.
    SDL_DispatchGPUCompute(computePass, app.numAsteroids / 64, 1, 1);

    // Compute is finished, let's move on to the rendering!
    SDL_EndGPUComputePass(computePass);

    SDL_GPUTexture *swapchainTexture = NULL;
    // As written about in the asteroid upload code above, WebGPU has a weird edge case where mapping a buffer will destroy any currently acquired swapchain textures.
    // So, the swapchain acquisition is done as late as possible here, to prevent shenanigans.
    SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuf, app.window, &swapchainTexture, NULL, NULL); // We don't care about the texture's size since we already know it

    // If the swapchain texture's NULL, we won't bother doing anything.
    // A NULL swapchain texture means that we can't render to the swapchain right now, for some reason.
    // This is normal behaviour (for example: if the window's not visible this'll be NULL), so we'll just skip over the rendering code if that's the case.
    if (swapchainTexture != NULL) {
        // This is some basic matrix math that creates perspective and also allows us to move the camera around.
        Matrix4x4 proj = Matrix4x4_CreatePerspectiveFieldOfView(75.0f * SDL_PI_F / 180.0f, RESOLUTION_X / RESOLUTION_Y, 0.001f, 200.0f);
        Matrix4x4 view = Matrix4x4_CreateLookAt(
            (Vector3){
                ((float)app.cameraDistance * 1000) * SDL_sinf(app.cameraRotation),
                ((float)app.cameraDistance * 1000),
                ((float)app.cameraDistance * 1000) * SDL_cosf(app.cameraRotation),
            },
            (Vector3){ 0, 0, 0 },
            (Vector3){ 0, 1, 0 });
        Matrix4x4 viewproj = Matrix4x4_Multiply(proj, view);

        // In order to start a render pass in SDLGPU you have to provide your color targets.
        // In this example there's only one color target (the swapchain), so that's all we care about.
        SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
        colorTargetInfo.texture = swapchainTexture;
        colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
        colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;   // We'll tell the GPU to clear the color target to pure black here.
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE; // We'll want to keep the finished image so that we can display it to the user.

        // Finally, we'll begin our render pass.
        SDL_GPURenderPass *renderPass =
            SDL_BeginGPURenderPass(cmdBuf, &colorTargetInfo, 1, NULL);

        // We'll first bind our pipeline, vertex & index buffers.
        SDL_BindGPUGraphicsPipeline(renderPass, app.graphicsPipeline);
        SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ .buffer = app.vtxBuffer, .offset = 0 }, 1);
        SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){ .buffer = app.idxBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        // We'll bind our asteroid storage buffer here (which was previously updated by the compute pass) so that the vertex shader can access the asteroid position.
        SDL_BindGPUVertexStorageBuffers(renderPass, 0, &app.asteroidBuf, 1);
        // We'll push our combined view + projection matrix to the GPU here.
        SDL_PushGPUVertexUniformData(cmdBuf, 0, &viewproj, sizeof(Matrix4x4));

        // Time to draw all of the asteroids. I wasn't very fancy with it, so the "asteroids" are in reality just cubes.
        // This example is mostly a showcase of WebGPU's compute abilities.
        // You can see here that we pass in app.numAsteroids here as our instance count.
        // This tells the GPU "hey, you'll be rendering the exact same thing a billion times, no need for multiple draw calls."
        // This greatly speeds up rendering since there's only one draw call for the GPU.
        SDL_DrawGPUIndexedPrimitives(renderPass, 36, app.numAsteroids, 0, 0, 0);

        // We're done with our render pass, so we'll end it here.
        SDL_EndGPURenderPass(renderPass);
    }

    // Finally, it's time to submit our command buffer.
    // SDLGPU handles all of the swapchain stuff for you, so we don't need to bother with presentation and such.
    SDL_SubmitGPUCommandBuffer(cmdBuf);

    return SDL_APP_CONTINUE;
}

// This callback gets run whenever an event occurs (e.g keyboard input)
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        // If the "quit" event's happened we'll return SDL_APP_SUCCESS, which tells SDL that we're done with our app.
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.scancode == SDL_SCANCODE_LEFT) {
            if (app.timescaleIndex > 0) {
                app.timescaleIndex -= 1;
                app.timescale = timescales[app.timescaleIndex];
            }

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Timescale: %.3f", app.timescale);
        } else if (event->key.scancode == SDL_SCANCODE_RIGHT) {
            if (app.timescaleIndex < 5) {
                app.timescaleIndex += 1;
                app.timescale = timescales[app.timescaleIndex];
            }
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Timescale: %.3f", app.timescale);
        } else if (event->key.scancode == SDL_SCANCODE_UP && (app.numAsteroids + 100000) <= MAX_ASTEROIDS) {
            app.numAsteroids += 100000;
            app.shouldRecreateAsteroids = true;

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Asteroid count: %i", (int)app.numAsteroids);
        } else if (event->key.scancode == SDL_SCANCODE_DOWN && ((int)app.numAsteroids - 100000) >= 10000) {
            app.numAsteroids -= 100000;
            app.shouldRecreateAsteroids = true;

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Asteroid count: %i", (int)app.numAsteroids);
        }
    }

    if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        app.cameraDistance -= event->wheel.integer_y * 500;
    }

    // If left click is pressed down on the mouse, we'll hide it and allow the user to rotate the camera view.
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event->button.button == 1) {
            app.leftClickDown = event->button.down;
            SDL_SetWindowRelativeMouseMode(app.window, app.leftClickDown);
        }
    }

    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        if (app.leftClickDown) {
            app.cameraRotationVelocity += event->motion.xrel / 2000.0f;
        }
    }

    return SDL_APP_CONTINUE;
}

// This callback is called when the app is shutting down.
// We'll free our GPU resources here.
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_ReleaseGPUBuffer(app.device, app.vtxBuffer);
    SDL_ReleaseGPUBuffer(app.device, app.idxBuffer);
    SDL_ReleaseGPUBuffer(app.device, app.asteroidBuf);

    SDL_ReleaseGPUGraphicsPipeline(app.device, app.graphicsPipeline);
    SDL_ReleaseGPUShader(app.device, app.fs);
    SDL_ReleaseGPUShader(app.device, app.vs);

    SDL_ReleaseWindowFromGPUDevice(app.device, app.window);
    SDL_DestroyGPUDevice(app.device);

    SDL_DestroyWindow(app.window);
    SDL_Quit();
}
