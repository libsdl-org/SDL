/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <libavutil/hwcontext_vulkan.h>


typedef struct VulkanVideoContext VulkanVideoContext;

VulkanVideoContext *CreateVulkanVideoContext(SDL_Window *window);
void SetupVulkanRenderProperties(VulkanVideoContext *context, SDL_PropertiesID props);
void SetupVulkanDeviceContextData(VulkanVideoContext *context, AVVulkanDeviceContext *ctx);
SDL_Texture *CreateVulkanVideoTexture(VulkanVideoContext *context, AVFrame *frame, SDL_Renderer *renderer, SDL_PropertiesID props);
void DestroyVulkanVideoContext(VulkanVideoContext *context);
