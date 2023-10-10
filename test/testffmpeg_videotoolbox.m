/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <SDL3/SDL.h>

#include "testffmpeg_videotoolbox.h"

#include <CoreVideo/CoreVideo.h>
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>
#include <simd/simd.h>


// Metal BT.601 to RGB conversion shader
static NSString *drawMetalShaderSource =
@"    using namespace metal;\n"
"\n"
"    struct Vertex\n"
"    {\n"
"        float4 position [[position]];\n"
"        float2 texCoords;\n"
"    };\n"
"\n"
"    constexpr sampler s(coord::normalized, address::clamp_to_edge, filter::linear);\n"
"\n"
"    vertex Vertex draw_vs(constant Vertex *vertices [[ buffer(0) ]], uint vid [[ vertex_id ]])\n"
"    {\n"
"        return vertices[ vid ];\n"
"    }\n"
"\n"
"    fragment float4 draw_ps_bt601(Vertex in [[ stage_in ]],\n"
"                                   texture2d<float> textureY [[ texture(0) ]],\n"
"                                   texture2d<float> textureUV [[ texture(1) ]])\n"
"    {\n"
"        float3 yuv = float3(textureY.sample(s, in.texCoords).r, textureUV.sample(s, in.texCoords).rg);\n"
"        float3 rgb;\n"
"        yuv += float3(-0.0627451017, -0.501960814, -0.501960814);\n"
"        rgb.r = dot(yuv, float3(1.1644,  0.000,   1.596));\n"
"        rgb.g = dot(yuv, float3(1.1644, -0.3918, -0.813));\n"
"        rgb.b = dot(yuv, float3(1.1644,  2.0172,  0.000));\n"
"        return float4(rgb, 1.0);\n"
"    }\n"
;

// keep this structure aligned with the proceeding drawMetalShaderSource's struct Vertex
typedef struct Vertex
{
    vector_float4 position;
    vector_float2 texCoord;
} Vertex;

static void SetVertex(Vertex *vertex, float x, float y, float s, float t)
{
    vertex->position[ 0 ] = x;
    vertex->position[ 1 ] = y;
    vertex->position[ 2 ] = 0.0f;
    vertex->position[ 3 ] = 1.0f;
    vertex->texCoord[ 0 ] = s;
    vertex->texCoord[ 1 ] = t;
}

static CAMetalLayer *metal_layer;
static id<MTLLibrary> library;
static id<MTLRenderPipelineState> video_pipeline;

SDL_bool SetupVideoToolboxOutput(SDL_Renderer *renderer)
{ @autoreleasepool {
    NSError *error;
    
    // Create the metal view
    metal_layer = (CAMetalLayer *)SDL_GetRenderMetalLayer(renderer);
    if (!metal_layer) {
        return SDL_FALSE;
    }

    // FIXME: Handle other colorspaces besides BT.601
    library = [metal_layer.device newLibraryWithSource:drawMetalShaderSource options:nil error:&error];

    MTLRenderPipelineDescriptor *videoPipelineDescriptor = [[MTLRenderPipelineDescriptor new] autorelease];
    videoPipelineDescriptor.vertexFunction = [library newFunctionWithName:@"draw_vs"];
    videoPipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"draw_ps_bt601"];
    videoPipelineDescriptor.colorAttachments[ 0 ].pixelFormat = metal_layer.pixelFormat;

    video_pipeline = [metal_layer.device newRenderPipelineStateWithDescriptor:videoPipelineDescriptor error:nil];
    if (!video_pipeline) {
        SDL_SetError("Couldn't create video pipeline");
        return SDL_FALSE;
    }

    return true;
}}

SDL_bool DisplayVideoToolboxFrame(SDL_Renderer *renderer, void *buffer, int srcX, int srcY, int srcW, int srcH, int dstX, int dstY, int dstW, int dstH )
{ @autoreleasepool {
    CVPixelBufferRef pPixelBuffer = (CVPixelBufferRef)buffer;
    size_t nPixelBufferWidth = CVPixelBufferGetWidthOfPlane(pPixelBuffer, 0);
    size_t nPixelBufferHeight = CVPixelBufferGetHeightOfPlane(pPixelBuffer, 0);
    id<MTLTexture> videoFrameTextureY = nil;
    id<MTLTexture> videoFrameTextureUV = nil;

    IOSurfaceRef pSurface = CVPixelBufferGetIOSurface(pPixelBuffer);

    MTLTextureDescriptor *textureDescriptorY = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:nPixelBufferWidth height:nPixelBufferHeight mipmapped:NO];
    MTLTextureDescriptor *textureDescriptorUV = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG8Unorm width:CVPixelBufferGetWidthOfPlane(pPixelBuffer, 1) height:CVPixelBufferGetHeightOfPlane(pPixelBuffer, 1) mipmapped:NO];

    videoFrameTextureY = [[metal_layer.device newTextureWithDescriptor:textureDescriptorY iosurface:pSurface plane:0] autorelease];
    videoFrameTextureUV = [[metal_layer.device newTextureWithDescriptor:textureDescriptorUV iosurface:pSurface plane:1] autorelease];

    float flMinSrcX = ( srcX + 0.5f ) / nPixelBufferWidth;
    float flMaxSrcX = ( srcX + srcW + 0.5f ) / nPixelBufferWidth;
    float flMinSrcY = ( srcY + 0.5f ) / nPixelBufferHeight;
    float flMaxSrcY = ( srcY + srcH + 0.5f ) / nPixelBufferHeight;

    int nOutputWidth, nOutputHeight;
    nOutputWidth = metal_layer.drawableSize.width;
    nOutputHeight = metal_layer.drawableSize.height;
    float flMinDstX = 2.0f * ( ( dstX + 0.5f ) / nOutputWidth ) - 1.0f;
    float flMaxDstX = 2.0f * ( ( dstX + dstW + 0.5f ) / nOutputWidth ) - 1.0f;
    float flMinDstY = 2.0f * ( ( nOutputHeight - dstY - 0.5f ) / nOutputHeight ) - 1.0f;
    float flMaxDstY = 2.0f * ( ( nOutputHeight - ( dstY + dstH ) - 0.5f ) / nOutputHeight ) - 1.0f;

    Vertex arrVerts[4];
    SetVertex(&arrVerts[0], flMinDstX, flMaxDstY, flMinSrcX, flMaxSrcY);
    SetVertex(&arrVerts[1], flMinDstX, flMinDstY, flMinSrcX, flMinSrcY);
    SetVertex(&arrVerts[2], flMaxDstX, flMaxDstY, flMaxSrcX, flMaxSrcY);
    SetVertex(&arrVerts[3], flMaxDstX, flMinDstY, flMaxSrcX, flMinSrcY);

    id<MTLRenderCommandEncoder> renderEncoder = (id<MTLRenderCommandEncoder>)SDL_GetRenderMetalCommandEncoder(renderer);
    [renderEncoder setRenderPipelineState:video_pipeline];
    [renderEncoder setFragmentTexture:videoFrameTextureY atIndex:0];
    [renderEncoder setFragmentTexture:videoFrameTextureUV atIndex:1];
    [renderEncoder setVertexBytes:arrVerts length:sizeof(arrVerts) atIndex:0];
    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:SDL_arraysize(arrVerts)];
    return SDL_TRUE;
}}

void CleanupVideoToolboxOutput()
{
}
