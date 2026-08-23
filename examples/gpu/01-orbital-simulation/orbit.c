#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include "math.h"

const char *vtxShader = "\n\
struct Asteroid {\n\
  pos: vec3<f32>,\n\
  vel: vec3<f32>,\n\
};\n\
@group(0u) @binding(0u) var<storage, read> posBuf: array<Asteroid>;\n\
@group(1u) @binding(0u) var<uniform> camMatrix: mat4x4<f32>;\n\
\n\
@vertex\n\
fn main(@location(0u) pos: vec3<f32>, @builtin(instance_index) index: u32) -> @builtin(position) vec4<f32> {\n\
  return vec4<f32>(pos + posBuf[index].pos, 1.0f) * camMatrix;\n\
}";

const char *frgShader = "\n\
@fragment\n\
fn main() -> @location(0) vec4<f32> {\n\
  return vec4<f32>(0.9f, 0.9f, 0.9f, 1.0f);\n\
}";

const char *cmpShader = "\n\
struct Asteroid {\n\
  pos: vec3<f32>,\n\
  vel: vec3<f32>,\n\
};\n\
struct UniformData {\n\
  timescale: f32,\n\
  numAsteroids: f32,\n\
  deltaTime: f32,\n\
};\n\
const GM = 3.98589196e+14;\n\
\n\
@group(1u) @binding(0u) var<storage, read_write> buf: array<Asteroid>;\n\
@group(2u) @binding(0u) var<uniform> uniformData: UniformData;\n\
\n\
fn accel(pos: vec3<f32>) -> vec3<f32>{\n\
  let r2 = dot(pos, pos);\n\
  let r = sqrt(r2);\n\
  return pos * (-GM / (r2 * r));\n\
}\n\
@compute @workgroup_size(64, 1, 1)\n\
fn main(@builtin(global_invocation_id) id: vec3<u32>){\n\
  var ast: Asteroid = buf[id.x];\n\
  ast.vel += (accel(ast.pos) * (uniformData.deltaTime)) * uniformData.timescale;\n\
  ast.pos += (ast.vel * uniformData.deltaTime) * uniformData.timescale;\n\
  buf[id.x] = ast;\n\
}";

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
            .target_info = { .num_color_targets = 1,
                             .color_target_descriptions =
                                 (SDL_GPUColorTargetDescription[]){ {
                                     .format = SDL_GetGPUSwapchainTextureFormat(
                                         app.device, app.window),
                                 } } },
            .vertex_input_state =
                (SDL_GPUVertexInputState){
                    .num_vertex_buffers = 1,
                    .vertex_buffer_descriptions =
                        (SDL_GPUVertexBufferDescription[]){ {
                            .slot = 0,
                            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                            .instance_step_rate = 0,
                            .pitch = sizeof(Vertex),
                        } },
                    .num_vertex_attributes = 1,
                    .vertex_attributes = (SDL_GPUVertexAttribute[]){ {
                        .buffer_slot = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                        .location = 0,
                        .offset = 0,
                    } } },
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
    if (app.asteroidBuf != NULL) {
        SDL_ReleaseGPUBuffer(app.device, app.asteroidBuf);
    }

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

    for (int i = 0; i < app.numAsteroids; i++) {
        Uint32 distance = SDL_max(SDL_rand(app.maxDistance), app.minDistance);

        float theta = (2.0f * SDL_PI_F * i) / app.numAsteroids;
        float finalDist = EARTH_RADIUS + distance;

        asteroidData[i].pos.x = finalDist * SDL_sinf(theta);
        asteroidData[i].pos.z = finalDist * SDL_cosf(theta);

        asteroidData[i].vel.x = (7700 * SDL_cosf(theta)) * (1 + SDL_randf() / 5);
        asteroidData[i].vel.z = (7700 * -SDL_sinf(theta)) * (1 + SDL_randf() / 5);
        asteroidData[i].vel.y = 1500 * SDL_randf();
    }

    SDL_UnmapGPUTransferBuffer(app.device, asteroidTransferBuffer);
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
    SDL_ReleaseGPUTransferBuffer(app.device, asteroidTransferBuffer);
}

void InitBuffers(void)
{
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

    SDL_GPUTransferBuffer *meshTransferBuffer = SDL_CreateGPUTransferBuffer(app.device, &(SDL_GPUTransferBufferCreateInfo){
                                                                                            .size = (sizeof(Uint16) * 36) + (sizeof(Vertex) * 24),
                                                                                            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                                                        });
    Vertex *transferData =
        SDL_MapGPUTransferBuffer(app.device, meshTransferBuffer, false);

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

    Uint16 *indexData = (Uint16 *)&transferData[24];
    Uint16 indices[] = { 0, 1, 2, 0, 2, 3, 6, 5, 4, 7, 6, 4,
                         8, 9, 10, 8, 10, 11, 14, 13, 12, 15, 14, 12,
                         16, 17, 18, 16, 18, 19, 22, 21, 20, 23, 22, 20 };
    SDL_memcpy(indexData, indices, sizeof(indices));

    SDL_UnmapGPUTransferBuffer(app.device, meshTransferBuffer);

    SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(app.device);
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    CreateAsteroids(copyPass);

    SDL_UploadToGPUBuffer(copyPass,
                          &(SDL_GPUTransferBufferLocation){
                              .transfer_buffer = meshTransferBuffer, .offset = 0 },
                          &(SDL_GPUBufferRegion){ .buffer = app.vtxBuffer,
                                                  .offset = 0,
                                                  .size = sizeof(Vertex) * 24 },
                          false);

    SDL_UploadToGPUBuffer(
        copyPass,
        &(SDL_GPUTransferBufferLocation){ .transfer_buffer = meshTransferBuffer,
                                          .offset = sizeof(Vertex) * 24 },
        &(SDL_GPUBufferRegion){
            .buffer = app.idxBuffer, .offset = 0, .size = sizeof(Uint16) * 36 },
        false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);

    SDL_ReleaseGPUTransferBuffer(app.device, meshTransferBuffer);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    SDL_Init(SDL_INIT_VIDEO);

    app.window =
        SDL_CreateWindow("Orbit Simulation", 800, 600, 0);
    app.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_WGSL, true, NULL);

    app.numAsteroids = 100000;
    app.minDistance = 75000;
    app.maxDistance = 78000;
    app.cameraDistance = 10000; // cameraDistance is in kilometers
    app.timescale = timescales[3];
    app.timescaleIndex = 3;

    SDL_ClaimWindowForGPUDevice(app.device, app.window);

    InitPipelines();
    InitBuffers();
    app.timeLastIter = SDL_GetTicksNS();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Press left or right to change timescale.");
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Press up or down to increase/decrease asteroid count.");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    Uint64 timeLastIter = app.timeLastIter;
    app.timeLastIter = SDL_GetTicksNS();
    app.deltaTime = (app.timeLastIter - timeLastIter) / (float)1e9;

    app.cameraRotation += app.cameraRotationVelocity;
    app.cameraRotationVelocity /= 1.15f;

    SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(app.device);

    if (app.shouldRecreateAsteroids) {
        SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuf);

        CreateAsteroids(copyPass);
        SDL_EndGPUCopyPass(copyPass);

        app.shouldRecreateAsteroids = false;
    }

    SDL_GPUComputePass *computePass = SDL_BeginGPUComputePass(cmdBuf, NULL,
                                                              0, &(SDL_GPUStorageBufferReadWriteBinding){ .buffer = app.asteroidBuf, .cycle = false }, 1);
    SDL_BindGPUComputePipeline(computePass, app.computePipeline);
    SDL_PushGPUComputeUniformData(cmdBuf, 0, &(UniformDataCompute){
                                                 .deltaTime = app.deltaTime,
                                                 .numAsteroids = app.numAsteroids,
                                                 .timescale = app.timescale,
                                             },
                                  sizeof(UniformDataCompute));
    SDL_DispatchGPUCompute(computePass, (Uint32)SDL_ceilf(app.numAsteroids / 64.0f), 1, 1);

    SDL_EndGPUComputePass(computePass);

    SDL_GPUTexture *swapchainTexture = NULL;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuf, app.window, &swapchainTexture,
                                          NULL, NULL);

    Matrix4x4 proj = Matrix4x4_CreatePerspectiveFieldOfView(
        75.0f * SDL_PI_F / 180.0f,
        800.0f / 600.0f, 0.001f, 200.0f);
    Matrix4x4 view = Matrix4x4_CreateLookAt(
        (Vector3){
            (app.cameraDistance * 1000) * SDL_sinf(app.cameraRotation),
            (app.cameraDistance * 1000),
            (app.cameraDistance * 1000) * SDL_cosf(app.cameraRotation),
        },
        (Vector3){ 0, 0, 0 },
        (Vector3){ 0, 1, 0 });
    Matrix4x4 viewproj = Matrix4x4_Multiply(proj, view);

    if (swapchainTexture != NULL) {
        SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
        colorTargetInfo.texture = swapchainTexture;
        colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
        colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *renderPass =
            SDL_BeginGPURenderPass(cmdBuf, &colorTargetInfo, 1, NULL);

        SDL_BindGPUGraphicsPipeline(renderPass, app.graphicsPipeline);
        SDL_BindGPUVertexBuffers(
            renderPass, 0,
            &(SDL_GPUBufferBinding){ .buffer = app.vtxBuffer, .offset = 0 }, 1);
        SDL_BindGPUIndexBuffer(
            renderPass,
            &(SDL_GPUBufferBinding){ .buffer = app.idxBuffer, .offset = 0 },
            SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_BindGPUVertexStorageBuffers(renderPass, 0, &app.asteroidBuf, 1);
        SDL_PushGPUVertexUniformData(cmdBuf, 0, &viewproj, sizeof(Matrix4x4));
        SDL_DrawGPUIndexedPrimitives(renderPass, 36, app.numAsteroids, 0, 0, 0);

        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(cmdBuf);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
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
