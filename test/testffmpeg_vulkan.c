/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "testffmpeg_vulkan.h"

#ifdef FFMPEG_VULKAN_SUPPORT

#define VULKAN_FUNCTIONS()                                             \
    VULKAN_GLOBAL_FUNCTION(vkCreateInstance)                           \
    VULKAN_GLOBAL_FUNCTION(vkEnumerateInstanceExtensionProperties)     \
    VULKAN_GLOBAL_FUNCTION(vkEnumerateInstanceLayerProperties)         \
    VULKAN_INSTANCE_FUNCTION(vkCreateDevice)                           \
    VULKAN_INSTANCE_FUNCTION(vkDestroyInstance)                        \
    VULKAN_INSTANCE_FUNCTION(vkDestroySurfaceKHR)                      \
    VULKAN_INSTANCE_FUNCTION(vkEnumerateDeviceExtensionProperties)     \
    VULKAN_INSTANCE_FUNCTION(vkEnumeratePhysicalDevices)               \
    VULKAN_INSTANCE_FUNCTION(vkGetDeviceProcAddr)                      \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceFeatures2)             \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties) \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR)     \
    VULKAN_INSTANCE_FUNCTION(vkQueueWaitIdle)                          \
    VULKAN_DEVICE_FUNCTION(vkAllocateCommandBuffers)                   \
    VULKAN_DEVICE_FUNCTION(vkBeginCommandBuffer)                       \
    VULKAN_DEVICE_FUNCTION(vkCmdPipelineBarrier2)                      \
    VULKAN_DEVICE_FUNCTION(vkCreateCommandPool)                        \
    VULKAN_DEVICE_FUNCTION(vkCreateSemaphore)                          \
    VULKAN_DEVICE_FUNCTION(vkDestroyCommandPool)                       \
    VULKAN_DEVICE_FUNCTION(vkDestroyDevice)                            \
    VULKAN_DEVICE_FUNCTION(vkDestroySemaphore)                         \
    VULKAN_DEVICE_FUNCTION(vkDeviceWaitIdle)                           \
    VULKAN_DEVICE_FUNCTION(vkEndCommandBuffer)                         \
    VULKAN_DEVICE_FUNCTION(vkFreeCommandBuffers)                       \
    VULKAN_DEVICE_FUNCTION(vkGetDeviceQueue)                           \
    VULKAN_DEVICE_FUNCTION(vkQueueSubmit)                              \
\
VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceVideoFormatPropertiesKHR) \

typedef struct
{
    VkPhysicalDeviceFeatures2 device_features;
    VkPhysicalDeviceVulkan11Features device_features_1_1;
    VkPhysicalDeviceVulkan12Features device_features_1_2;
    VkPhysicalDeviceVulkan13Features device_features_1_3;
    VkPhysicalDeviceDescriptorBufferFeaturesEXT desc_buf_features;
    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomic_float_features;
    VkPhysicalDeviceCooperativeMatrixFeaturesKHR coop_matrix_features;
} VulkanDeviceFeatures;

struct VulkanVideoContext
{
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    int presentQueueFamilyIndex;
    int presentQueueCount;
    int graphicsQueueFamilyIndex;
    int graphicsQueueCount;
    int transferQueueFamilyIndex;
    int transferQueueCount;
    int computeQueueFamilyIndex;
    int computeQueueCount;
    int decodeQueueFamilyIndex;
    int decodeQueueCount;
    VkDevice device;
    VkQueue graphicsQueue;
    VkCommandPool commandPool;
    VkCommandBuffer *commandBuffers;
    uint32_t commandBufferCount;
    uint32_t commandBufferIndex;
    VkSemaphore *waitSemaphores;
    uint32_t waitSemaphoreCount;
    VkSemaphore *signalSemaphores;
    uint32_t signalSemaphoreCount;

    const char **instanceExtensions;
    int instanceExtensionsCount;

    const char **deviceExtensions;
    int deviceExtensionsCount;

    VulkanDeviceFeatures features;

	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
#define VULKAN_GLOBAL_FUNCTION(name)   PFN_##name name;
#define VULKAN_INSTANCE_FUNCTION(name) PFN_##name name;
#define VULKAN_DEVICE_FUNCTION(name)   PFN_##name name;
	VULKAN_FUNCTIONS()
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
#undef VULKAN_DEVICE_FUNCTION
};


static int loadGlobalFunctions(VulkanVideoContext *context)
{
    context->vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!context->vkGetInstanceProcAddr) {
        return -1;
    }

#define VULKAN_GLOBAL_FUNCTION(name)                                                        \
    context->name = (PFN_##name)context->vkGetInstanceProcAddr(VK_NULL_HANDLE, #name);      \
    if (!context->name) {                                                                   \
        return SDL_SetError("vkGetInstanceProcAddr(VK_NULL_HANDLE, \"" #name "\") failed"); \
    }
#define VULKAN_INSTANCE_FUNCTION(name)
#define VULKAN_DEVICE_FUNCTION(name)
    VULKAN_FUNCTIONS()
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
#undef VULKAN_DEVICE_FUNCTION
    return 0;
}

static int loadInstanceFunctions(VulkanVideoContext *context)
{
#define VULKAN_GLOBAL_FUNCTION(name)
#define VULKAN_INSTANCE_FUNCTION(name)                                                    \
    context->name = (PFN_##name)context->vkGetInstanceProcAddr(context->instance, #name); \
    if (!context->name) {                                                                 \
        return SDL_SetError("vkGetInstanceProcAddr(instance, \"" #name "\") failed");     \
    }
#define VULKAN_DEVICE_FUNCTION(name)
    VULKAN_FUNCTIONS()
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
#undef VULKAN_DEVICE_FUNCTION
    return 0;
}

static int loadDeviceFunctions(VulkanVideoContext *context)
{
#define VULKAN_GLOBAL_FUNCTION(name)
#define VULKAN_INSTANCE_FUNCTION(name)
#define VULKAN_DEVICE_FUNCTION(name)                                                  \
    context->name = (PFN_##name)context->vkGetDeviceProcAddr(context->device, #name); \
    if (!context->name) {                                                             \
        return SDL_SetError("vkGetDeviceProcAddr(device, \"" #name "\") failed\n");   \
    }
    VULKAN_FUNCTIONS()
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
#undef VULKAN_DEVICE_FUNCTION
    return 0;
}

#undef VULKAN_FUNCTIONS

static const char *getVulkanResultString(VkResult result)
{
    switch ((int)result) {
#define RESULT_CASE(x) \
    case x:            \
        return #x
        RESULT_CASE(VK_SUCCESS);
        RESULT_CASE(VK_NOT_READY);
        RESULT_CASE(VK_TIMEOUT);
        RESULT_CASE(VK_EVENT_SET);
        RESULT_CASE(VK_EVENT_RESET);
        RESULT_CASE(VK_INCOMPLETE);
        RESULT_CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
        RESULT_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        RESULT_CASE(VK_ERROR_INITIALIZATION_FAILED);
        RESULT_CASE(VK_ERROR_DEVICE_LOST);
        RESULT_CASE(VK_ERROR_MEMORY_MAP_FAILED);
        RESULT_CASE(VK_ERROR_LAYER_NOT_PRESENT);
        RESULT_CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
        RESULT_CASE(VK_ERROR_FEATURE_NOT_PRESENT);
        RESULT_CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
        RESULT_CASE(VK_ERROR_TOO_MANY_OBJECTS);
        RESULT_CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
        RESULT_CASE(VK_ERROR_FRAGMENTED_POOL);
        RESULT_CASE(VK_ERROR_SURFACE_LOST_KHR);
        RESULT_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        RESULT_CASE(VK_SUBOPTIMAL_KHR);
        RESULT_CASE(VK_ERROR_OUT_OF_DATE_KHR);
        RESULT_CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        RESULT_CASE(VK_ERROR_VALIDATION_FAILED_EXT);
        RESULT_CASE(VK_ERROR_OUT_OF_POOL_MEMORY_KHR);
        RESULT_CASE(VK_ERROR_INVALID_SHADER_NV);
#undef RESULT_CASE
    default:
        break;
    }
    return (result < 0) ? "VK_ERROR_<Unknown>" : "VK_<Unknown>";
}

static int createInstance(VulkanVideoContext *context)
{
    static const char *optional_extensions[] = {
        VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };
    VkApplicationInfo appInfo = { 0 };
    VkInstanceCreateInfo instanceCreateInfo = { 0 };
    VkResult result;
    char const *const *instanceExtensions = SDL_Vulkan_GetInstanceExtensions(&instanceCreateInfo.enabledExtensionCount);

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_3;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    const char **instanceExtensionsCopy = SDL_calloc(instanceCreateInfo.enabledExtensionCount + SDL_arraysize(optional_extensions), sizeof(const char *));
    for (uint32_t i = 0; i < instanceCreateInfo.enabledExtensionCount; i++) {
        instanceExtensionsCopy[i] = instanceExtensions[i];
    }

    // Get the rest of the optional extensions
    {
        uint32_t extensionCount;
        if (context->vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL) == VK_SUCCESS && extensionCount > 0) {
            VkExtensionProperties *extensionProperties = SDL_calloc(sizeof(VkExtensionProperties), extensionCount);
            if (context->vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensionProperties) == VK_SUCCESS) {
                for (uint32_t i = 0; i < SDL_arraysize(optional_extensions); ++i) {
                    for (uint32_t j = 0; j < extensionCount; ++j) {
                        if (SDL_strcmp(extensionProperties[j].extensionName, optional_extensions[i]) == 0) {
                            instanceExtensionsCopy[instanceCreateInfo.enabledExtensionCount++] = optional_extensions[i];
                            break;
                        }
                    }
                }
            }
            SDL_free(extensionProperties);
        }
    }
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensionsCopy;

    context->instanceExtensions = instanceExtensionsCopy;
    context->instanceExtensionsCount = instanceCreateInfo.enabledExtensionCount;

    result = context->vkCreateInstance(&instanceCreateInfo, NULL, &context->instance);
    if (result != VK_SUCCESS) {
        context->instance = VK_NULL_HANDLE;
        return SDL_SetError("vkCreateInstance(): %s\n", getVulkanResultString(result));
    }
    if (loadInstanceFunctions(context) < 0) {
        return -1;
    }
    return 0;
}

static int createSurface(VulkanVideoContext *context, SDL_Window *window)
{
    if (SDL_Vulkan_CreateSurface(window, context->instance, NULL, &context->surface) < 0) {
        context->surface = VK_NULL_HANDLE;
        return -1;
    }
    return 0;
}

// Use the same queue scoring algorithm as ffmpeg to make sure we get the same device configuration
static int selectQueueFamily(VkQueueFamilyProperties *queueFamiliesProperties, uint32_t queueFamiliesCount, VkQueueFlagBits flags, int *queueCount)
{
    uint32_t queueFamilyIndex;
    uint32_t selectedQueueFamilyIndex = queueFamiliesCount;
    uint32_t min_score = ~0u;

    for (queueFamilyIndex = 0; queueFamilyIndex < queueFamiliesCount; ++queueFamilyIndex) {
        VkQueueFlagBits current_flags = queueFamiliesProperties[queueFamilyIndex].queueFlags;
        if (current_flags & flags) {
            uint32_t score = av_popcount(current_flags) + queueFamiliesProperties[queueFamilyIndex].timestampValidBits;
            if (score < min_score) {
                selectedQueueFamilyIndex = queueFamilyIndex;
                min_score = score;
            }
        }
    }

    if (selectedQueueFamilyIndex != queueFamiliesCount) {
        VkQueueFamilyProperties *selectedQueueFamily = &queueFamiliesProperties[selectedQueueFamilyIndex];
        *queueCount = (int)selectedQueueFamily->queueCount;
        ++selectedQueueFamily->timestampValidBits;
        return (int)selectedQueueFamilyIndex;
    } else {
        *queueCount = 0;
        return -1;
    }
}

static int findPhysicalDevice(VulkanVideoContext *context)
{
    uint32_t physicalDeviceCount = 0;
    VkPhysicalDevice *physicalDevices;
    VkQueueFamilyProperties *queueFamiliesProperties = NULL;
    uint32_t queueFamiliesPropertiesAllocatedSize = 0;
    VkExtensionProperties *deviceExtensions = NULL;
    uint32_t deviceExtensionsAllocatedSize = 0;
    uint32_t physicalDeviceIndex;
    VkResult result;

    result = context->vkEnumeratePhysicalDevices(context->instance, &physicalDeviceCount, NULL);
    if (result != VK_SUCCESS) {
        return SDL_SetError("vkEnumeratePhysicalDevices(): %s", getVulkanResultString(result));
    }
    if (physicalDeviceCount == 0) {
        return SDL_SetError("vkEnumeratePhysicalDevices(): no physical devices");
    }
    physicalDevices = (VkPhysicalDevice *)SDL_malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    if (!physicalDevices) {
        return -1;
    }
    result = context->vkEnumeratePhysicalDevices(context->instance, &physicalDeviceCount, physicalDevices);
    if (result != VK_SUCCESS) {
        SDL_free(physicalDevices);
        return SDL_SetError("vkEnumeratePhysicalDevices(): %s", getVulkanResultString(result));
    }
    context->physicalDevice = NULL;
    for (physicalDeviceIndex = 0; physicalDeviceIndex < physicalDeviceCount; physicalDeviceIndex++) {
        uint32_t queueFamiliesCount = 0;
        uint32_t queueFamilyIndex;
        uint32_t deviceExtensionCount = 0;
        SDL_bool hasSwapchainExtension = SDL_FALSE;
        uint32_t i;

        VkPhysicalDevice physicalDevice = physicalDevices[physicalDeviceIndex];
        context->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, NULL);
        if (queueFamiliesCount == 0) {
            continue;
        }
        if (queueFamiliesPropertiesAllocatedSize < queueFamiliesCount) {
            SDL_free(queueFamiliesProperties);
            queueFamiliesPropertiesAllocatedSize = queueFamiliesCount;
            queueFamiliesProperties = (VkQueueFamilyProperties *)SDL_malloc(sizeof(VkQueueFamilyProperties) * queueFamiliesPropertiesAllocatedSize);
            if (!queueFamiliesProperties) {
                SDL_free(physicalDevices);
                SDL_free(deviceExtensions);
                return -1;
            }
        }
        context->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, queueFamiliesProperties);

        // Initialize timestampValidBits for scoring in selectQueueFamily
        for (queueFamilyIndex = 0; queueFamilyIndex < queueFamiliesCount; queueFamilyIndex++) {
            queueFamiliesProperties[queueFamilyIndex].timestampValidBits = 0;
        }
        context->presentQueueFamilyIndex = -1;
        context->graphicsQueueFamilyIndex = -1;
        for (queueFamilyIndex = 0; queueFamilyIndex < queueFamiliesCount; queueFamilyIndex++) {
            VkBool32 supported = 0;

            if (queueFamiliesProperties[queueFamilyIndex].queueCount == 0) {
                continue;
            }

            if (queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                context->graphicsQueueFamilyIndex = queueFamilyIndex;
            }

            result = context->vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, context->surface, &supported);
            if (result == VK_SUCCESS) {
                if (supported) {
                    context->presentQueueFamilyIndex = queueFamilyIndex;
                    if (queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                        break; // use this queue because it can present and do graphics
                    }
                }
            }
        }
        if (context->presentQueueFamilyIndex < 0 || context->graphicsQueueFamilyIndex < 0) {
            // We can't render and present on this device
            continue;
        }

        context->presentQueueCount = queueFamiliesProperties[context->presentQueueFamilyIndex].queueCount;
        ++queueFamiliesProperties[context->presentQueueFamilyIndex].timestampValidBits;
        context->graphicsQueueCount = queueFamiliesProperties[context->graphicsQueueFamilyIndex].queueCount;
        ++queueFamiliesProperties[context->graphicsQueueFamilyIndex].timestampValidBits;

        context->transferQueueFamilyIndex = selectQueueFamily(queueFamiliesProperties, queueFamiliesCount, VK_QUEUE_TRANSFER_BIT, &context->transferQueueCount);
        context->computeQueueFamilyIndex = selectQueueFamily(queueFamiliesProperties, queueFamiliesCount, VK_QUEUE_COMPUTE_BIT, &context->computeQueueCount);
        context->decodeQueueFamilyIndex = selectQueueFamily(queueFamiliesProperties, queueFamiliesCount, VK_QUEUE_VIDEO_DECODE_BIT_KHR, &context->decodeQueueCount);
        if (context->transferQueueFamilyIndex < 0) {
            // ffmpeg can fall back to the compute or graphics queues for this
            context->transferQueueFamilyIndex = selectQueueFamily(queueFamiliesProperties, queueFamiliesCount, VK_QUEUE_COMPUTE_BIT, &context->transferQueueCount);
            if (context->transferQueueFamilyIndex < 0) {
                context->transferQueueFamilyIndex = selectQueueFamily(queueFamiliesProperties, queueFamiliesCount, VK_QUEUE_GRAPHICS_BIT, &context->transferQueueCount);
            }
        }

        if (context->transferQueueFamilyIndex < 0 ||
            context->computeQueueFamilyIndex < 0) {
            // This device doesn't have the queues we need for video decoding
            continue;
        }

        result = context->vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &deviceExtensionCount, NULL);
        if (result != VK_SUCCESS) {
            SDL_free(physicalDevices);
            SDL_free(queueFamiliesProperties);
            SDL_free(deviceExtensions);
            return SDL_SetError("vkEnumerateDeviceExtensionProperties(): %s", getVulkanResultString(result));
        }
        if (deviceExtensionCount == 0) {
            continue;
        }
        if (deviceExtensionsAllocatedSize < deviceExtensionCount) {
            SDL_free(deviceExtensions);
            deviceExtensionsAllocatedSize = deviceExtensionCount;
            deviceExtensions = SDL_malloc(sizeof(VkExtensionProperties) * deviceExtensionsAllocatedSize);
            if (!deviceExtensions) {
                SDL_free(physicalDevices);
                SDL_free(queueFamiliesProperties);
                return -1;
            }
        }
        result = context->vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &deviceExtensionCount, deviceExtensions);
        if (result != VK_SUCCESS) {
            SDL_free(physicalDevices);
            SDL_free(queueFamiliesProperties);
            SDL_free(deviceExtensions);
            return SDL_SetError("vkEnumerateDeviceExtensionProperties(): %s", getVulkanResultString(result));
        }
        for (i = 0; i < deviceExtensionCount; i++) {
            if (SDL_strcmp(deviceExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchainExtension = SDL_TRUE;
                break;
            }
        }
        if (!hasSwapchainExtension) {
            continue;
        }
        context->physicalDevice = physicalDevice;
        break;
    }
    SDL_free(physicalDevices);
    SDL_free(queueFamiliesProperties);
    SDL_free(deviceExtensions);
    if (!context->physicalDevice) {
        return SDL_SetError("Vulkan: no viable physical devices found");
    }
	return 0;
}

static void initDeviceFeatures(VulkanDeviceFeatures *features)
{
    features->device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features->device_features.pNext = &features->device_features_1_1;
    features->device_features_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features->device_features_1_1.pNext = &features->device_features_1_2;
    features->device_features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features->device_features_1_2.pNext = &features->device_features_1_3;
    features->device_features_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features->device_features_1_3.pNext = &features->desc_buf_features;
    features->desc_buf_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    features->desc_buf_features.pNext = &features->atomic_float_features;
    features->atomic_float_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
    features->atomic_float_features.pNext = &features->coop_matrix_features;
    features->coop_matrix_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR;
    features->coop_matrix_features.pNext = NULL;
}

static void copyDeviceFeatures(VulkanDeviceFeatures *supported_features, VulkanDeviceFeatures *requested_features)
{
#define COPY_OPTIONAL_FEATURE(X) requested_features->X = supported_features->X
    COPY_OPTIONAL_FEATURE(device_features.features.shaderImageGatherExtended);
    COPY_OPTIONAL_FEATURE(device_features.features.shaderStorageImageReadWithoutFormat);
    COPY_OPTIONAL_FEATURE(device_features.features.shaderStorageImageWriteWithoutFormat);
    COPY_OPTIONAL_FEATURE(device_features.features.fragmentStoresAndAtomics);
    COPY_OPTIONAL_FEATURE(device_features.features.vertexPipelineStoresAndAtomics);
    COPY_OPTIONAL_FEATURE(device_features.features.shaderInt64);
    COPY_OPTIONAL_FEATURE(device_features.features.shaderInt16);
    COPY_OPTIONAL_FEATURE(device_features.features.shaderFloat64);
    COPY_OPTIONAL_FEATURE(device_features_1_1.samplerYcbcrConversion);
    COPY_OPTIONAL_FEATURE(device_features_1_1.storagePushConstant16);
    COPY_OPTIONAL_FEATURE(device_features_1_2.bufferDeviceAddress);
    COPY_OPTIONAL_FEATURE(device_features_1_2.hostQueryReset);
    COPY_OPTIONAL_FEATURE(device_features_1_2.storagePushConstant8);
    COPY_OPTIONAL_FEATURE(device_features_1_2.shaderInt8);
    COPY_OPTIONAL_FEATURE(device_features_1_2.storageBuffer8BitAccess);
    COPY_OPTIONAL_FEATURE(device_features_1_2.uniformAndStorageBuffer8BitAccess);
    COPY_OPTIONAL_FEATURE(device_features_1_2.shaderFloat16);
    COPY_OPTIONAL_FEATURE(device_features_1_2.shaderSharedInt64Atomics);
    COPY_OPTIONAL_FEATURE(device_features_1_2.vulkanMemoryModel);
    COPY_OPTIONAL_FEATURE(device_features_1_2.vulkanMemoryModelDeviceScope);
    COPY_OPTIONAL_FEATURE(device_features_1_2.hostQueryReset);
    COPY_OPTIONAL_FEATURE(device_features_1_3.dynamicRendering);
    COPY_OPTIONAL_FEATURE(device_features_1_3.maintenance4);
    COPY_OPTIONAL_FEATURE(device_features_1_3.synchronization2);
    COPY_OPTIONAL_FEATURE(device_features_1_3.computeFullSubgroups);
    COPY_OPTIONAL_FEATURE(device_features_1_3.shaderZeroInitializeWorkgroupMemory);
    COPY_OPTIONAL_FEATURE(desc_buf_features.descriptorBuffer);
    COPY_OPTIONAL_FEATURE(desc_buf_features.descriptorBufferPushDescriptors);
    COPY_OPTIONAL_FEATURE(atomic_float_features.shaderBufferFloat32Atomics);
    COPY_OPTIONAL_FEATURE(atomic_float_features.shaderBufferFloat32AtomicAdd);
    COPY_OPTIONAL_FEATURE(coop_matrix_features.cooperativeMatrix);
#undef COPY_OPTIONAL_FEATURE

    // timeline semaphores is required by ffmpeg
    requested_features->device_features_1_2.timelineSemaphore = 1;
}

static int addQueueFamily(VkDeviceQueueCreateInfo **pQueueCreateInfos, uint32_t *pQueueCreateInfoCount, uint32_t queueFamilyIndex, uint32_t queueCount)
{
    VkDeviceQueueCreateInfo *queueCreateInfo;
    VkDeviceQueueCreateInfo *queueCreateInfos = *pQueueCreateInfos;
    uint32_t queueCreateInfoCount = *pQueueCreateInfoCount;
    float *queuePriorities;

    if (queueCount == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < queueCreateInfoCount; ++i) {
        if (queueCreateInfos[i].queueFamilyIndex == queueFamilyIndex) {
            return 0;
        }
    }

    queueCreateInfos = (VkDeviceQueueCreateInfo *)SDL_realloc(queueCreateInfos, (queueCreateInfoCount + 1) * sizeof(*queueCreateInfos));
    if (!queueCreateInfos) {
        return -1;
    }

    queuePriorities = (float *)SDL_malloc(queueCount * sizeof(*queuePriorities));
    if (!queuePriorities) {
        return -1;
    }

    for (uint32_t i = 0; i < queueCount; ++i) {
        queuePriorities[i] = 1.0f / queueCount;
    }

    queueCreateInfo = &queueCreateInfos[queueCreateInfoCount++];
    SDL_zerop(queueCreateInfo);
    queueCreateInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo->queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo->queueCount = queueCount;
    queueCreateInfo->pQueuePriorities = queuePriorities;

    *pQueueCreateInfos = queueCreateInfos;
    *pQueueCreateInfoCount = queueCreateInfoCount;
    return 0;
}

static int createDevice(VulkanVideoContext *context)
{
    static const char *const deviceExtensionNames[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    };
    static const char *optional_extensions[] = {
        VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME
    };
    VkDeviceCreateInfo deviceCreateInfo = { 0 };
    VkDeviceQueueCreateInfo *queueCreateInfos = NULL;
    uint32_t queueCreateInfoCount = 0;
    VulkanDeviceFeatures supported_features;
    VkResult result = VK_ERROR_UNKNOWN;

    if (addQueueFamily(&queueCreateInfos, &queueCreateInfoCount, context->presentQueueFamilyIndex, context->presentQueueCount) < 0 ||
        addQueueFamily(&queueCreateInfos, &queueCreateInfoCount, context->graphicsQueueFamilyIndex, context->graphicsQueueCount) < 0 ||
        addQueueFamily(&queueCreateInfos, &queueCreateInfoCount, context->transferQueueFamilyIndex, context->transferQueueCount) < 0 ||
        addQueueFamily(&queueCreateInfos, &queueCreateInfoCount, context->computeQueueFamilyIndex, context->computeQueueCount) < 0 ||
        addQueueFamily(&queueCreateInfos, &queueCreateInfoCount, context->decodeQueueFamilyIndex, context->decodeQueueCount) < 0) {
        goto done;
    }

    initDeviceFeatures(&supported_features);
    initDeviceFeatures(&context->features);
    context->vkGetPhysicalDeviceFeatures2(context->physicalDevice, &supported_features.device_features);
    copyDeviceFeatures(&supported_features, &context->features);

    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
    deviceCreateInfo.pEnabledFeatures = NULL;
    deviceCreateInfo.enabledExtensionCount = SDL_arraysize(deviceExtensionNames);
    deviceCreateInfo.pNext = &context->features.device_features;

    const char **deviceExtensionsCopy = SDL_calloc(deviceCreateInfo.enabledExtensionCount + SDL_arraysize(optional_extensions), sizeof(const char *));
    for (uint32_t i = 0; i < deviceCreateInfo.enabledExtensionCount; i++) {
        deviceExtensionsCopy[i] = deviceExtensionNames[i];
    }

    // Get the rest of the optional extensions
    {
        uint32_t extensionCount;
        if (context->vkEnumerateDeviceExtensionProperties(context->physicalDevice, NULL, &extensionCount, NULL) == VK_SUCCESS && extensionCount > 0) {
            VkExtensionProperties *extensionProperties = SDL_calloc(sizeof(VkExtensionProperties), extensionCount);
            if (context->vkEnumerateDeviceExtensionProperties(context->physicalDevice, NULL, &extensionCount, extensionProperties) == VK_SUCCESS) {
                for (uint32_t i = 0; i < SDL_arraysize(optional_extensions); ++i) {
                    for (uint32_t j = 0; j < extensionCount; ++j) {
                        if (SDL_strcmp(extensionProperties[j].extensionName, optional_extensions[i]) == 0) {
                            deviceExtensionsCopy[deviceCreateInfo.enabledExtensionCount++] = optional_extensions[i];
                            break;
                        }
                    }
                }
            }
            SDL_free(extensionProperties);
        }
    }
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensionsCopy;

    context->deviceExtensions = deviceExtensionsCopy;
    context->deviceExtensionsCount = deviceCreateInfo.enabledExtensionCount;

    result = context->vkCreateDevice(context->physicalDevice, &deviceCreateInfo, NULL, &context->device);
    if (result != VK_SUCCESS) {
        SDL_SetError("vkCreateDevice(): %s", getVulkanResultString(result));
        goto done;
    }

    if (loadDeviceFunctions(context) < 0) {
        result = VK_ERROR_UNKNOWN;
        context->device = VK_NULL_HANDLE;
        goto done;
    }

    // Get the graphics queue that SDL will use
    context->vkGetDeviceQueue(context->device, context->graphicsQueueFamilyIndex, 0, &context->graphicsQueue);

    // Create a command pool
    VkCommandPoolCreateInfo commandPoolCreateInfo = { 0 };
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = context->graphicsQueueFamilyIndex;
    result = context->vkCreateCommandPool(context->device, &commandPoolCreateInfo, NULL, &context->commandPool);
    if (result != VK_SUCCESS) {
        SDL_SetError("vkCreateCommandPool(): %s", getVulkanResultString(result));
        goto done;
    }

done:
    for (uint32_t i = 0; i < queueCreateInfoCount; ++i) {
        SDL_free((void *)queueCreateInfos[i].pQueuePriorities);
    }
    SDL_free(queueCreateInfos);

    if (result != VK_SUCCESS) {
        return -1;
    }
	return 0;
}

VulkanVideoContext *CreateVulkanVideoContext(SDL_Window *window)
{
	VulkanVideoContext *context = SDL_calloc(1, sizeof(*context));
	if (!context) {
		return NULL;
	}
    if (loadGlobalFunctions(context) < 0 ||
        createInstance(context) < 0 ||
        createSurface(context, window) < 0 ||
        findPhysicalDevice(context) < 0 ||
        createDevice(context) < 0) {
		DestroyVulkanVideoContext(context);
		return NULL;
	}
	return context;
}

void SetupVulkanRenderProperties(VulkanVideoContext *context, SDL_PropertiesID props)
{
    SDL_SetProperty(props, SDL_PROP_RENDERER_CREATE_VULKAN_INSTANCE_POINTER, context->instance);
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_VULKAN_SURFACE_NUMBER, (Sint64)context->surface);
    SDL_SetProperty(props, SDL_PROP_RENDERER_CREATE_VULKAN_PHYSICAL_DEVICE_POINTER, context->physicalDevice);
    SDL_SetProperty(props, SDL_PROP_RENDERER_CREATE_VULKAN_DEVICE_POINTER, context->device);
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_VULKAN_PRESENT_QUEUE_FAMILY_INDEX_NUMBER, context->presentQueueFamilyIndex);
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_VULKAN_GRAPHICS_QUEUE_FAMILY_INDEX_NUMBER, context->graphicsQueueFamilyIndex);
}

void SetupVulkanDeviceContextData(VulkanVideoContext *context, AVVulkanDeviceContext *ctx)
{
    ctx->get_proc_addr = context->vkGetInstanceProcAddr;
    ctx->inst = context->instance;
    ctx->phys_dev = context->physicalDevice;
    ctx->act_dev = context->device;
    ctx->device_features = context->features.device_features;
    ctx->enabled_inst_extensions = context->instanceExtensions;
    ctx->nb_enabled_inst_extensions = context->instanceExtensionsCount;
    ctx->enabled_dev_extensions = context->deviceExtensions;
    ctx->nb_enabled_dev_extensions = context->deviceExtensionsCount;
    ctx->queue_family_index = context->graphicsQueueFamilyIndex;
    ctx->nb_graphics_queues = context->graphicsQueueCount;
    ctx->queue_family_tx_index = context->transferQueueFamilyIndex;
    ctx->nb_tx_queues = context->transferQueueCount;
    ctx->queue_family_comp_index = context->computeQueueFamilyIndex;
    ctx->nb_comp_queues = context->computeQueueCount;
    ctx->queue_family_encode_index = -1;
    ctx->nb_encode_queues = 0;
    ctx->queue_family_decode_index = context->decodeQueueFamilyIndex;
    ctx->nb_decode_queues = context->decodeQueueCount;
}

static int CreateCommandBuffers(VulkanVideoContext *context, SDL_Renderer *renderer)
{
    uint32_t commandBufferCount = (uint32_t)SDL_GetNumberProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_VULKAN_SWAPCHAIN_IMAGE_COUNT_NUMBER, 1);

    if (commandBufferCount > context->waitSemaphoreCount) {
        VkSemaphore *semaphores = (VkSemaphore *)SDL_realloc(context->waitSemaphores, commandBufferCount * sizeof(*semaphores));
        if (!semaphores) {
            return -1;
        }
        context->waitSemaphores = semaphores;

        VkSemaphoreCreateInfo semaphoreCreateInfo = { 0 };
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        while (context->waitSemaphoreCount < commandBufferCount) {
            VkResult result = context->vkCreateSemaphore(context->device, &semaphoreCreateInfo, NULL, &context->waitSemaphores[context->waitSemaphoreCount]);
            if (result != VK_SUCCESS) {
                SDL_SetError("vkCreateSemaphore(): %s", getVulkanResultString(result));
                return -1;
            }
            ++context->waitSemaphoreCount;
        }
    }

    if (commandBufferCount > context->signalSemaphoreCount) {
        VkSemaphore *semaphores = (VkSemaphore *)SDL_realloc(context->signalSemaphores, commandBufferCount * sizeof(*semaphores));
        if (!semaphores) {
            return -1;
        }
        context->signalSemaphores = semaphores;

        VkSemaphoreCreateInfo semaphoreCreateInfo = { 0 };
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        while (context->signalSemaphoreCount < commandBufferCount) {
            VkResult result = context->vkCreateSemaphore(context->device, &semaphoreCreateInfo, NULL, &context->signalSemaphores[context->signalSemaphoreCount]);
            if (result != VK_SUCCESS) {
                SDL_SetError("vkCreateSemaphore(): %s", getVulkanResultString(result));
                return -1;
            }
            ++context->signalSemaphoreCount;
        }
    }

    if (commandBufferCount > context->commandBufferCount) {
        uint32_t needed = (commandBufferCount - context->commandBufferCount);
        VkCommandBuffer *commandBuffers = (VkCommandBuffer *)SDL_realloc(context->commandBuffers, commandBufferCount * sizeof(*commandBuffers));
        if (!commandBuffers) {
            return -1;
        }
        context->commandBuffers = commandBuffers;

        VkCommandBufferAllocateInfo commandBufferAllocateInfo = { 0 };
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = context->commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = needed;
        VkResult result = context->vkAllocateCommandBuffers(context->device, &commandBufferAllocateInfo, &context->commandBuffers[context->commandBufferCount]);
        if (result != VK_SUCCESS) {
            SDL_SetError("vkAllocateCommandBuffers(): %s", getVulkanResultString(result));
            return -1;
        }

        context->commandBufferCount = commandBufferCount;
    }
    return 0;
}

int BeginVulkanFrameRendering(VulkanVideoContext *context, AVFrame *frame, SDL_Renderer *renderer)
{
    AVHWFramesContext *frames = (AVHWFramesContext *)(frame->hw_frames_ctx->data);
    AVVulkanFramesContext *vk = (AVVulkanFramesContext *)(frames->hwctx);
    AVVkFrame *pVkFrame = (AVVkFrame *)frame->data[0];

    if (CreateCommandBuffers(context, renderer) < 0) {
        return -1;
    }

    vk->lock_frame(frames, pVkFrame);

    VkTimelineSemaphoreSubmitInfo timeline = { 0 };
    timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline.waitSemaphoreValueCount = 1;
    timeline.pWaitSemaphoreValues = pVkFrame->sem_value;

    VkPipelineStageFlags pipelineStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    VkSubmitInfo submitInfo = { 0 };
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = pVkFrame->sem;
    submitInfo.pWaitDstStageMask = &pipelineStageMask;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &context->waitSemaphores[context->commandBufferIndex];
    submitInfo.pNext = &timeline;

    if (pVkFrame->layout[0] != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        VkCommandBuffer commandBuffer = context->commandBuffers[context->commandBufferIndex];

        VkCommandBufferBeginInfo beginInfo = { 0 };
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        context->vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkImageMemoryBarrier2 barrier = { 0 };
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcAccessMask = VK_ACCESS_2_NONE;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        barrier.oldLayout = pVkFrame->layout[0];
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.image = pVkFrame->img[0];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcQueueFamilyIndex = pVkFrame->queue_family[0];
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        VkDependencyInfo dep = { 0 };
        dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;
        context->vkCmdPipelineBarrier2(commandBuffer, &dep);

        context->vkEndCommandBuffer(commandBuffer);

        // Add the image barrier to the submit info
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &context->commandBuffers[context->commandBufferIndex];

        pVkFrame->layout[0] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        pVkFrame->queue_family[0] = VK_QUEUE_FAMILY_IGNORED;
    }

    VkResult result = context->vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, 0);
    if (result != VK_SUCCESS) {
        // Don't return an error here, we need to complete the frame operation
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION , "vkQueueSubmit(): %s", getVulkanResultString(result));
    }

    SDL_AddVulkanRenderSemaphores(renderer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (Sint64)context->waitSemaphores[context->commandBufferIndex], (Sint64)context->signalSemaphores[context->commandBufferIndex]);

    return 0;
}

int FinishVulkanFrameRendering(VulkanVideoContext *context, AVFrame *frame, SDL_Renderer *renderer)
{
    AVHWFramesContext *frames = (AVHWFramesContext *)(frame->hw_frames_ctx->data);
    AVVulkanFramesContext *vk = (AVVulkanFramesContext *)(frames->hwctx);
    AVVkFrame *pVkFrame = (AVVkFrame *)frame->data[0];

    // Transition the frame back to ffmpeg
    ++pVkFrame->sem_value[0];

    VkTimelineSemaphoreSubmitInfo timeline = { 0 };
    timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline.signalSemaphoreValueCount = 1;
    timeline.pSignalSemaphoreValues = pVkFrame->sem_value;

    VkPipelineStageFlags pipelineStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    VkSubmitInfo submitInfo = { 0 };
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &context->signalSemaphores[context->commandBufferIndex];
    submitInfo.pWaitDstStageMask = &pipelineStageMask;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = pVkFrame->sem;
    submitInfo.pNext = &timeline;

    VkResult result = context->vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, 0);
    if (result != VK_SUCCESS) {
        // Don't return an error here, we need to complete the frame operation
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "vkQueueSubmit(): %s", getVulkanResultString(result));
    }

    vk->unlock_frame(frames, pVkFrame);

    context->commandBufferIndex = (context->commandBufferIndex + 1) % context->commandBufferCount;

    return 0;
}

SDL_Texture *CreateVulkanVideoTexture(VulkanVideoContext *context, AVFrame *frame, SDL_Renderer *renderer, SDL_PropertiesID props)
{
    AVHWFramesContext *frames = (AVHWFramesContext *)(frame->hw_frames_ctx->data);
    AVVulkanFramesContext *vk = (AVVulkanFramesContext *)(frames->hwctx);
    AVVkFrame *pVkFrame = (AVVkFrame *)frame->data[0];
    Uint32 format;

    switch (vk->format[0]) {
    case VK_FORMAT_G8B8G8R8_422_UNORM:
        format = SDL_PIXELFORMAT_YUY2;
        break;
    case VK_FORMAT_B8G8R8G8_422_UNORM:
        format = SDL_PIXELFORMAT_UYVY;
        break;
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        format = SDL_PIXELFORMAT_IYUV;
        break;
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
        format = SDL_PIXELFORMAT_NV12;
        break;
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        format = SDL_PIXELFORMAT_P010;
        break;
    default:
        format = SDL_PIXELFORMAT_UNKNOWN;
        break;
    }
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, format);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_VULKAN_TEXTURE_NUMBER, (Sint64)pVkFrame->img[0]);
    return SDL_CreateTextureWithProperties(renderer, props);
}

void DestroyVulkanVideoContext(VulkanVideoContext *context)
{
    if (context) {
        if (context->device) {
            context->vkDeviceWaitIdle(context->device);
        }
        if (context->instanceExtensions) {
            SDL_free(context->instanceExtensions);
        }
        if (context->deviceExtensions) {
            SDL_free(context->deviceExtensions);
        }
        if (context->waitSemaphores) {
            for (uint32_t i = 0; i < context->waitSemaphoreCount; ++i) {
                context->vkDestroySemaphore(context->device, context->waitSemaphores[i], NULL);
            }
            SDL_free(context->waitSemaphores);
            context->waitSemaphores = NULL;
        }
        if (context->signalSemaphores) {
            for (uint32_t i = 0; i < context->signalSemaphoreCount; ++i) {
                context->vkDestroySemaphore(context->device, context->signalSemaphores[i], NULL);
            }
            SDL_free(context->signalSemaphores);
            context->signalSemaphores = NULL;
        }
        if (context->commandBuffers) {
            context->vkFreeCommandBuffers(context->device, context->commandPool, context->commandBufferCount, context->commandBuffers);
            SDL_free(context->commandBuffers);
            context->commandBuffers = NULL;
        }
        if (context->commandPool) {
            context->vkDestroyCommandPool(context->device, context->commandPool, NULL);
            context->commandPool = VK_NULL_HANDLE;
        }
        if (context->device) {
            context->vkDestroyDevice(context->device, NULL);
        }
        if (context->surface) {
            context->vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        }
        if (context->instance) {
            context->vkDestroyInstance(context->instance, NULL);
        }
        SDL_free(context);
    }
}

#else

VulkanVideoContext *CreateVulkanVideoContext(SDL_Window *window)
{
    SDL_SetError("testffmpeg not built with Vulkan support");
    return NULL;
}

void SetupVulkanRenderProperties(VulkanVideoContext *context, SDL_PropertiesID props)
{
}

void SetupVulkanDeviceContextData(VulkanVideoContext *context, AVVulkanDeviceContext *ctx)
{
}

SDL_Texture *CreateVulkanVideoTexture(VulkanVideoContext *context, AVFrame *frame, SDL_Renderer *renderer, SDL_PropertiesID props)
{
    return NULL;
}

int BeginVulkanFrameRendering(VulkanVideoContext *context, AVFrame *frame, SDL_Renderer *renderer)
{
    return -1;
}

int FinishVulkanFrameRendering(VulkanVideoContext *context, AVFrame *frame, SDL_Renderer *renderer)
{
    return -1;
}

void DestroyVulkanVideoContext(VulkanVideoContext *context)
{
}

#endif // FFMPEG_VULKAN_SUPPORT
