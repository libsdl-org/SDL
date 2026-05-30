// BSD 3-Clause License
//
// Copyright (c) 2019, "WebGPU native" developers
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "SDL_internal.h"

#ifndef WEBGPU_H_
#define NO_WEBGPU_HEADER
#endif

#ifdef NO_WEBGPU_HEADER

#if !defined(WGPU_FUNCTION_ATTRIBUTE)
#define WGPU_FUNCTION_ATTRIBUTE
#endif

#if !defined(WGPU_STRLEN)
#define WGPU_STRLEN (SIZE_MAX)
#endif

typedef enum WGPUSType
{
    WGPUSType_ShaderSourceSPIRV = 0x00000001,
    WGPUSType_ShaderSourceWGSL = 0x00000002,
    WGPUSType_RenderPassMaxDrawCount = 0x00000003,
    WGPUSType_SurfaceSourceMetalLayer = 0x00000004,
    WGPUSType_SurfaceSourceWindowsHWND = 0x00000005,
    WGPUSType_SurfaceSourceXlibWindow = 0x00000006,
    WGPUSType_SurfaceSourceWaylandSurface = 0x00000007,
    WGPUSType_SurfaceSourceAndroidNativeWindow = 0x00000008,
    WGPUSType_SurfaceSourceXCBWindow = 0x00000009,
    WGPUSType_SurfaceColorManagement = 0x0000000A,
    WGPUSType_RequestAdapterWebXROptions = 0x0000000B,
    WGPUSType_TextureComponentSwizzleDescriptor = 0x0000000C,
    WGPUSType_ExternalTextureBindingLayout = 0x0000000D,
    WGPUSType_ExternalTextureBindingEntry = 0x0000000E,
    WGPUSType_CompatibilityModeLimits = 0x0000000F,
    WGPUSType_TextureBindingViewDimension = 0x00000010,
    WGPUSType_Force32 = 0x7FFFFFFF
} WGPUSType;

typedef struct WGPUChainedStruct
{
    struct WGPUChainedStruct *next;
    WGPUSType sType;
} WGPUChainedStruct;

typedef struct WGPUStringView
{
    char const *data;
    size_t length;
} WGPUStringView;

typedef struct WGPUSurfaceDescriptor
{
    WGPUChainedStruct *nextInChain;
    WGPUStringView label;
} WGPUSurfaceDescriptor;

typedef struct WGPUSurfaceSourceWaylandSurface
{
    WGPUChainedStruct chain;
    void *display;
    void *surface;
} WGPUSurfaceSourceWaylandSurface;

typedef struct WGPUSurfaceSourceXlibWindow {
    WGPUChainedStruct chain;
    void * display;
    uint64_t window;
} WGPUSurfaceSourceXlibWindow;

typedef struct WGPUSurfaceSourceWindowsHWND {
    WGPUChainedStruct chain;
    void * hinstance;
    void * hwnd;
} WGPUSurfaceSourceWindowsHWND;

typedef WGPUSurface (*WGPUProcInstanceCreateSurface)(WGPUInstance instance, WGPUSurfaceDescriptor const *descriptor) WGPU_FUNCTION_ATTRIBUTE;

#if defined(WGPU_STATIC)
extern WGPUSurface wgpuInstanceCreateSurface(WGPUInstance instance, WGPUSurfaceDescriptor const * descriptor) WGPU_FUNCTION_ATTRIBUTE;
#endif

#endif
