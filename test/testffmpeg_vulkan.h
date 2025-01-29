/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>


typedef struct VulkanVideoContext VulkanVideoContext;

extern VulkanVideoContext *CreateVulkanVideoContext(SDL_Window *window);
extern void SetupVulkanRenderProperties(VulkanVideoContext *context, SDL_PropertiesID props);
extern void SetupVulkanDeviceContextData(VulkanVideoContext *context, AVVulkanDeviceContext *ctx);
extern SDL_Texture *CreateVulkanVideoTexture(VulkanVideoContext *context, AVFrame *frame, SDL_Renderer *renderer, SDL_PropertiesID props);
extern int BeginVulkanFrameRendering(VulkanVideoContext *context, AVFrame *frame, SDL_Renderer *renderer);
extern int FinishVulkanFrameRendering(VulkanVideoContext *context, AVFrame *frame, SDL_Renderer *renderer);
extern void DestroyVulkanVideoContext(VulkanVideoContext *context);
