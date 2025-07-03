/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VULKAN_OHOS_H
#define VULKAN_OHOS_H 1

#include <vulkan/vulkan_core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VK_KHR_OHOS_XCOMPONENT 1
struct ONativeWindow;

#define VK_KHR_OHOS_XCOMPONENT_SPEC_VERSION 6
#define VK_KHR_OHOS_XCOMPONENT_EXTENSION_NAME "VK_OHOS_surface"

typedef VkFlags VkOHOSXComponentCreateFlagsKHR;

typedef struct VkOHOSXComponentCreateInfoKHR {
    VkStructureType                   sType;
    const void*                       pNext;
    VkOHOSXComponentCreateFlagsKHR    flags;
    struct OHNativeWindow*             window;
} VkOHOSXComponentCreateInfoKHR;

typedef VkResult (VKAPI_PTR *PFN_vkCreateOHOSXComponentKHR)(VkInstance instance,
    const VkOHOSXComponentCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator,
        VkSurfaceKHR* pXComponent);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkCreateOHOSXComponentKHR(
    VkInstance                                  instance,
    const VkOHOSXComponentCreateInfoKHR*        pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSurfaceKHR*                               pSurface);
#endif

#define VK_OHOS_EXTERNAL_MEMORY_OHOS_HARDWARE_BUFFER 1
struct OHardwareBuffer;

#define VK_OHOS_EXTERNAL_MEMORY_OHOS_HARDWARE_BUFFER_SPEC_VERSION 3
#define VK_OHOS_EXTERNAL_MEMORY_OHOS_HARDWARE_BUFFER_EXTENSION_NAME "VK_OHOS_EXTERNAL_MEMORY_OHOS_HARDWARE_BUFFER"

typedef struct VkOHOSHardwareBufferUsageOHOS {
    VkStructureType    sType;
    void*              pNext;
    uint64_t           ohosHardwareBufferUsage;
} VkOHOSHardwareBufferUsageOHOS;

typedef struct VkOHOSHardwareBufferPropertiesOHOS {
    VkStructureType    sType;
    void*              pNext;
    VkDeviceSize       allocationSize;
    uint32_t           memoryTypeBits;
} VkOHOSHardwareBufferPropertiesOHOS;

typedef struct VkOHOSHardwareBufferFormatPropertiesOHOS {
    VkStructureType                  sType;
    void*                            pNext;
    VkFormat                         format;
    uint64_t                         externalFormat;
    VkFormatFeatureFlags             formatFeatures;
    VkComponentMapping               samplerYcbcrConversionComponents;
    VkSamplerYcbcrModelConversion    suggestedYcbcrModel;
    VkSamplerYcbcrRange              suggestedYcbcrRange;
    VkChromaLocation                 suggestedXChromaOffset;
    VkChromaLocation                 suggestedYChromaOffset;
} VkOHOSHardwareBufferFormatPropertiesOHOS;

typedef struct VkImportOHOSHardwareBufferInfoOHOS {
    VkStructureType            sType;
    const void*                pNext;
    struct OHardwareBuffer*    buffer;
} VkImportOHOSHardwareBufferInfoOHOS;

typedef struct VkMemoryGetOHOSHardwareBufferInfoOHOS {
    VkStructureType    sType;
    const void*        pNext;
    VkDeviceMemory     memory;
} VkMemoryGetOHOSHardwareBufferInfoOHOS;

typedef struct VkExternalFormatOHOS {
    VkStructureType    sType;
    void*              pNext;
    uint64_t           externalFormat;
} VkExternalFormatOHOS;


typedef VkResult (VKAPI_PTR *PFN_vkGetOHOSHardwareBufferPropertiesOHOS)(VkDevice device,
    const struct OHardwareBuffer* buffer, VkOHOSHardwareBufferPropertiesOHOS* pProperties);
typedef VkResult (VKAPI_PTR *PFN_vkGetMemoryOHOSHardwareBufferOHOS)(VkDevice device,
    const VkMemoryGetOHOSHardwareBufferInfoOHOS* pInfo, struct OHardwareBuffer** pBuffer);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkGetOHOSHardwareBufferPropertiesOHOS(
    VkDevice                                    device,
    const struct OHardwareBuffer*               buffer,
    VkOHOSHardwareBufferPropertiesOHOS*   pProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryOHOSHardwareBufferOHOS(
    VkDevice                                    device,
    const VkMemoryGetOHOSHardwareBufferInfoOHOS* pInfo,
    struct OHardwareBuffer**                    pBuffer);
#endif

#ifdef __cplusplus
}
#endif

#endif
