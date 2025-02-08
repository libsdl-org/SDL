/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <stdlib.h>

#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>

#if defined(SDL_PLATFORM_ANDROID) && defined(__ARM_EABI__) && !defined(__ARM_ARCH_7A__)

int main(int argc, char *argv[])
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No Vulkan support on this system");
    return 1;
}

#else

#define VK_NO_PROTOTYPES
#ifdef HAVE_VULKAN_H
#include <vulkan/vulkan.h>
#else
/* SDL includes a copy for building on systems without the Vulkan SDK */
#include "../src/video/khronos/vulkan/vulkan.h"
#endif
#include <SDL3/SDL_vulkan.h>

#ifndef UINT64_MAX /* VS2008 */
#define UINT64_MAX 18446744073709551615
#endif

#define VULKAN_FUNCTIONS()                                              \
    VULKAN_DEVICE_FUNCTION(vkAcquireNextImageKHR)                       \
    VULKAN_DEVICE_FUNCTION(vkAllocateCommandBuffers)                    \
    VULKAN_DEVICE_FUNCTION(vkBeginCommandBuffer)                        \
    VULKAN_DEVICE_FUNCTION(vkCmdClearColorImage)                        \
    VULKAN_DEVICE_FUNCTION(vkCmdPipelineBarrier)                        \
    VULKAN_DEVICE_FUNCTION(vkCreateCommandPool)                         \
    VULKAN_DEVICE_FUNCTION(vkCreateFence)                               \
    VULKAN_DEVICE_FUNCTION(vkCreateImageView)                           \
    VULKAN_DEVICE_FUNCTION(vkCreateSemaphore)                           \
    VULKAN_DEVICE_FUNCTION(vkCreateSwapchainKHR)                        \
    VULKAN_DEVICE_FUNCTION(vkDestroyCommandPool)                        \
    VULKAN_DEVICE_FUNCTION(vkDestroyDevice)                             \
    VULKAN_DEVICE_FUNCTION(vkDestroyFence)                              \
    VULKAN_DEVICE_FUNCTION(vkDestroyImageView)                          \
    VULKAN_DEVICE_FUNCTION(vkDestroySemaphore)                          \
    VULKAN_DEVICE_FUNCTION(vkDestroySwapchainKHR)                       \
    VULKAN_DEVICE_FUNCTION(vkDeviceWaitIdle)                            \
    VULKAN_DEVICE_FUNCTION(vkEndCommandBuffer)                          \
    VULKAN_DEVICE_FUNCTION(vkFreeCommandBuffers)                        \
    VULKAN_DEVICE_FUNCTION(vkGetDeviceQueue)                            \
    VULKAN_DEVICE_FUNCTION(vkGetFenceStatus)                            \
    VULKAN_DEVICE_FUNCTION(vkGetSwapchainImagesKHR)                     \
    VULKAN_DEVICE_FUNCTION(vkQueuePresentKHR)                           \
    VULKAN_DEVICE_FUNCTION(vkQueueSubmit)                               \
    VULKAN_DEVICE_FUNCTION(vkResetCommandBuffer)                        \
    VULKAN_DEVICE_FUNCTION(vkResetFences)                               \
    VULKAN_DEVICE_FUNCTION(vkWaitForFences)                             \
    VULKAN_GLOBAL_FUNCTION(vkCreateInstance)                            \
    VULKAN_GLOBAL_FUNCTION(vkEnumerateInstanceExtensionProperties)      \
    VULKAN_GLOBAL_FUNCTION(vkEnumerateInstanceLayerProperties)          \
    VULKAN_INSTANCE_FUNCTION(vkCreateDevice)                            \
    VULKAN_INSTANCE_FUNCTION(vkDestroyInstance)                         \
    VULKAN_INSTANCE_FUNCTION(vkDestroySurfaceKHR)                       \
    VULKAN_INSTANCE_FUNCTION(vkEnumerateDeviceExtensionProperties)      \
    VULKAN_INSTANCE_FUNCTION(vkEnumeratePhysicalDevices)                \
    VULKAN_INSTANCE_FUNCTION(vkGetDeviceProcAddr)                       \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceFeatures)               \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceProperties)             \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties)  \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR)      \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR) \
    VULKAN_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR)

#define VULKAN_DEVICE_FUNCTION(name)   static PFN_##name name = NULL;
#define VULKAN_GLOBAL_FUNCTION(name)   static PFN_##name name = NULL;
#define VULKAN_INSTANCE_FUNCTION(name) static PFN_##name name = NULL;
VULKAN_FUNCTIONS()
#undef VULKAN_DEVICE_FUNCTION
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;

/* Based on the headers found in
 * https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers
 */
#if VK_HEADER_VERSION < 22
enum
{
    VK_ERROR_FRAGMENTED_POOL = -12,
};
#endif
#if VK_HEADER_VERSION < 38
enum
{
    VK_ERROR_OUT_OF_POOL_MEMORY_KHR = -1000069000
};
#endif

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

typedef struct VulkanContext
{
    SDL_Window *window;
    VkInstance instance;
    VkDevice device;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    uint32_t graphicsQueueFamilyIndex;
    uint32_t presentQueueFamilyIndex;
    VkPhysicalDevice physicalDevice;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderingFinishedSemaphore;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkSurfaceFormatKHR *surfaceFormats;
    uint32_t surfaceFormatsAllocatedCount;
    uint32_t surfaceFormatsCount;
    uint32_t swapchainDesiredImageCount;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D swapchainSize;
    VkCommandPool commandPool;
    uint32_t swapchainImageCount;
    VkImage *swapchainImages;
    VkCommandBuffer *commandBuffers;
    VkFence *fences;
} VulkanContext;

static SDLTest_CommonState *state;
static VulkanContext *vulkanContexts = NULL; /* an array of state->num_windows items */
static VulkanContext *vulkanContext = NULL;  /* for the currently-rendering window */

static void shutdownVulkan(bool doDestroySwapchain);

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc)
{
    shutdownVulkan(true);
    SDLTest_CommonQuit(state);
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

static void loadGlobalFunctions(void)
{
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!vkGetInstanceProcAddr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_Vulkan_GetVkGetInstanceProcAddr(): %s",
                     SDL_GetError());
        quit(2);
    }

#define VULKAN_DEVICE_FUNCTION(name)
#define VULKAN_GLOBAL_FUNCTION(name)                                                 \
    name = (PFN_##name)vkGetInstanceProcAddr(VK_NULL_HANDLE, #name);                 \
    if (!name) {                                                                     \
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,                                   \
                     "vkGetInstanceProcAddr(VK_NULL_HANDLE, \"" #name "\") failed"); \
        quit(2);                                                                     \
    }
#define VULKAN_INSTANCE_FUNCTION(name)
    VULKAN_FUNCTIONS()
#undef VULKAN_DEVICE_FUNCTION
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
}

static void createInstance(void)
{
    VkApplicationInfo appInfo = { 0 };
    VkInstanceCreateInfo instanceCreateInfo = { 0 };
    VkResult result;

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_0;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
#ifdef __APPLE__
    instanceCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    instanceCreateInfo.ppEnabledExtensionNames = SDL_Vulkan_GetInstanceExtensions(&instanceCreateInfo.enabledExtensionCount);
    result = vkCreateInstance(&instanceCreateInfo, NULL, &vulkanContext->instance);
    if (result != VK_SUCCESS) {
        vulkanContext->instance = VK_NULL_HANDLE;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkCreateInstance(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
}

static void loadInstanceFunctions(void)
{
#define VULKAN_DEVICE_FUNCTION(name)
#define VULKAN_GLOBAL_FUNCTION(name)
#define VULKAN_INSTANCE_FUNCTION(name)                                         \
    name = (PFN_##name)vkGetInstanceProcAddr(vulkanContext->instance, #name);  \
    if (!name) {                                                               \
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,                             \
                     "vkGetInstanceProcAddr(instance, \"" #name "\") failed"); \
        quit(2);                                                               \
    }
    VULKAN_FUNCTIONS()
#undef VULKAN_DEVICE_FUNCTION
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
}

static void createSurface(void)
{
    if (!SDL_Vulkan_CreateSurface(vulkanContext->window, vulkanContext->instance, NULL, &vulkanContext->surface)) {
        vulkanContext->surface = VK_NULL_HANDLE;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Vulkan_CreateSurface(): %s", SDL_GetError());
        quit(2);
    }
}

static void findPhysicalDevice(void)
{
    uint32_t physicalDeviceCount = 0;
    VkPhysicalDevice *physicalDevices;
    VkQueueFamilyProperties *queueFamiliesProperties = NULL;
    uint32_t queueFamiliesPropertiesAllocatedSize = 0;
    VkExtensionProperties *deviceExtensions = NULL;
    uint32_t deviceExtensionsAllocatedSize = 0;
    uint32_t physicalDeviceIndex;
    VkResult result;

    result = vkEnumeratePhysicalDevices(vulkanContext->instance, &physicalDeviceCount, NULL);
    if (result != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkEnumeratePhysicalDevices(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
    if (physicalDeviceCount == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkEnumeratePhysicalDevices(): no physical devices");
        quit(2);
    }
    physicalDevices = (VkPhysicalDevice *)SDL_malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    if (!physicalDevices) {
        quit(2);
    }
    result = vkEnumeratePhysicalDevices(vulkanContext->instance, &physicalDeviceCount, physicalDevices);
    if (result != VK_SUCCESS) {
        SDL_free(physicalDevices);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkEnumeratePhysicalDevices(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
    vulkanContext->physicalDevice = NULL;
    for (physicalDeviceIndex = 0; physicalDeviceIndex < physicalDeviceCount; physicalDeviceIndex++) {
        uint32_t queueFamiliesCount = 0;
        uint32_t queueFamilyIndex;
        uint32_t deviceExtensionCount = 0;
        bool hasSwapchainExtension = false;
        bool supportsPresent;
        uint32_t i;

        VkPhysicalDevice physicalDevice = physicalDevices[physicalDeviceIndex];
        vkGetPhysicalDeviceProperties(physicalDevice, &vulkanContext->physicalDeviceProperties);
        if (VK_VERSION_MAJOR(vulkanContext->physicalDeviceProperties.apiVersion) < 1) {
            continue;
        }
        vkGetPhysicalDeviceFeatures(physicalDevice, &vulkanContext->physicalDeviceFeatures);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, NULL);
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
                quit(2);
            }
        }
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, queueFamiliesProperties);
        vulkanContext->graphicsQueueFamilyIndex = queueFamiliesCount;
        vulkanContext->presentQueueFamilyIndex = queueFamiliesCount;
        for (queueFamilyIndex = 0; queueFamilyIndex < queueFamiliesCount; queueFamilyIndex++) {
            VkBool32 supported = 0;

            if (queueFamiliesProperties[queueFamilyIndex].queueCount == 0) {
                continue;
            }

            if (queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                vulkanContext->graphicsQueueFamilyIndex = queueFamilyIndex;
            }

            result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, vulkanContext->surface, &supported);
            if (result != VK_SUCCESS) {
                SDL_free(physicalDevices);
                SDL_free(queueFamiliesProperties);
                SDL_free(deviceExtensions);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "vkGetPhysicalDeviceSurfaceSupportKHR(): %s",
                             getVulkanResultString(result));
                quit(2);
            }
            if (supported) {
                /* This check isn't necessary if you are able to check a
                 * VkSurfaceKHR like above, but just as a sanity check we do
                 * this here as part of testing the API.
                 * -flibit
                 */
                supportsPresent = SDL_Vulkan_GetPresentationSupport(vulkanContext->instance, physicalDevice, queueFamilyIndex);
                if (!supportsPresent) {
                    SDL_free(physicalDevices);
                    SDL_free(queueFamiliesProperties);
                    SDL_free(deviceExtensions);
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "SDL_Vulkan_GetPresentationSupport(): %s",
                                 SDL_GetError());
                    quit(2);
                }

                vulkanContext->presentQueueFamilyIndex = queueFamilyIndex;
                if (queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    break; // use this queue because it can present and do graphics
                }
            }
        }

        if (vulkanContext->graphicsQueueFamilyIndex == queueFamiliesCount) { // no good queues found
            continue;
        }
        if (vulkanContext->presentQueueFamilyIndex == queueFamiliesCount) { // no good queues found
            continue;
        }
        result = vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &deviceExtensionCount, NULL);
        if (result != VK_SUCCESS) {
            SDL_free(physicalDevices);
            SDL_free(queueFamiliesProperties);
            SDL_free(deviceExtensions);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "vkEnumerateDeviceExtensionProperties(): %s",
                         getVulkanResultString(result));
            quit(2);
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
                quit(2);
            }
        }
        result = vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &deviceExtensionCount, deviceExtensions);
        if (result != VK_SUCCESS) {
            SDL_free(physicalDevices);
            SDL_free(queueFamiliesProperties);
            SDL_free(deviceExtensions);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "vkEnumerateDeviceExtensionProperties(): %s",
                         getVulkanResultString(result));
            quit(2);
        }
        for (i = 0; i < deviceExtensionCount; i++) {
            if (SDL_strcmp(deviceExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchainExtension = true;
                break;
            }
        }
        if (!hasSwapchainExtension) {
            continue;
        }
        vulkanContext->physicalDevice = physicalDevice;
        break;
    }
    SDL_free(physicalDevices);
    SDL_free(queueFamiliesProperties);
    SDL_free(deviceExtensions);
    if (!vulkanContext->physicalDevice) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: no viable physical devices found");
        quit(2);
    }
}

static void createDevice(void)
{
    VkDeviceQueueCreateInfo deviceQueueCreateInfo[2] = { { 0 }, { 0 } };
    static const float queuePriority[] = { 1.0f };
    VkDeviceCreateInfo deviceCreateInfo = { 0 };
    static const char *const deviceExtensionNames[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef __APPLE__        
        "VK_KHR_portability_subset"
#endif        
    };
    VkResult result;

    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 0;
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = NULL;
    deviceCreateInfo.enabledExtensionCount = SDL_arraysize(deviceExtensionNames);
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensionNames;

    deviceQueueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo[0].queueFamilyIndex = vulkanContext->graphicsQueueFamilyIndex;
    deviceQueueCreateInfo[0].queueCount = 1;
    deviceQueueCreateInfo[0].pQueuePriorities = queuePriority;
    ++deviceCreateInfo.queueCreateInfoCount;

    if (vulkanContext->presentQueueFamilyIndex != vulkanContext->graphicsQueueFamilyIndex) {
        deviceQueueCreateInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        deviceQueueCreateInfo[1].queueFamilyIndex = vulkanContext->presentQueueFamilyIndex;
        deviceQueueCreateInfo[1].queueCount = 1;
        deviceQueueCreateInfo[1].pQueuePriorities = queuePriority;
        ++deviceCreateInfo.queueCreateInfoCount;
    }

    result = vkCreateDevice(vulkanContext->physicalDevice, &deviceCreateInfo, NULL, &vulkanContext->device);
    if (result != VK_SUCCESS) {
        vulkanContext->device = VK_NULL_HANDLE;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "vkCreateDevice(): %s", getVulkanResultString(result));
        quit(2);
    }
}

static void loadDeviceFunctions(void)
{
#define VULKAN_DEVICE_FUNCTION(name)                                       \
    name = (PFN_##name)vkGetDeviceProcAddr(vulkanContext->device, #name);  \
    if (!name) {                                                           \
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,                         \
                     "vkGetDeviceProcAddr(device, \"" #name "\") failed"); \
        quit(2);                                                           \
    }
#define VULKAN_GLOBAL_FUNCTION(name)
#define VULKAN_INSTANCE_FUNCTION(name)
    VULKAN_FUNCTIONS()
#undef VULKAN_DEVICE_FUNCTION
#undef VULKAN_GLOBAL_FUNCTION
#undef VULKAN_INSTANCE_FUNCTION
}

#undef VULKAN_FUNCTIONS

static void getQueues(void)
{
    vkGetDeviceQueue(vulkanContext->device,
                     vulkanContext->graphicsQueueFamilyIndex,
                     0,
                     &vulkanContext->graphicsQueue);
    if (vulkanContext->graphicsQueueFamilyIndex != vulkanContext->presentQueueFamilyIndex) {
        vkGetDeviceQueue(vulkanContext->device,
                         vulkanContext->presentQueueFamilyIndex,
                         0,
                         &vulkanContext->presentQueue);
    } else {
        vulkanContext->presentQueue = vulkanContext->graphicsQueue;
    }
}

static void createSemaphore(VkSemaphore *semaphore)
{
    VkResult result;

    VkSemaphoreCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = vkCreateSemaphore(vulkanContext->device, &createInfo, NULL, semaphore);
    if (result != VK_SUCCESS) {
        *semaphore = VK_NULL_HANDLE;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkCreateSemaphore(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
}

static void createSemaphores(void)
{
    createSemaphore(&vulkanContext->imageAvailableSemaphore);
    createSemaphore(&vulkanContext->renderingFinishedSemaphore);
}

static void getSurfaceCaps(void)
{
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanContext->physicalDevice, vulkanContext->surface, &vulkanContext->surfaceCapabilities);
    if (result != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkGetPhysicalDeviceSurfaceCapabilitiesKHR(): %s",
                     getVulkanResultString(result));
        quit(2);
    }

    // check surface usage
    if (!(vulkanContext->surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Vulkan surface doesn't support VK_IMAGE_USAGE_TRANSFER_DST_BIT");
        quit(2);
    }
}

static void getSurfaceFormats(void)
{
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanContext->physicalDevice,
                                                           vulkanContext->surface,
                                                           &vulkanContext->surfaceFormatsCount,
                                                           NULL);
    if (result != VK_SUCCESS) {
        vulkanContext->surfaceFormatsCount = 0;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkGetPhysicalDeviceSurfaceFormatsKHR(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
    if (vulkanContext->surfaceFormatsCount > vulkanContext->surfaceFormatsAllocatedCount) {
        vulkanContext->surfaceFormatsAllocatedCount = vulkanContext->surfaceFormatsCount;
        SDL_free(vulkanContext->surfaceFormats);
        vulkanContext->surfaceFormats = (VkSurfaceFormatKHR *)SDL_malloc(sizeof(VkSurfaceFormatKHR) * vulkanContext->surfaceFormatsAllocatedCount);
        if (!vulkanContext->surfaceFormats) {
            vulkanContext->surfaceFormatsCount = 0;
            quit(2);
        }
    }
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanContext->physicalDevice,
                                                  vulkanContext->surface,
                                                  &vulkanContext->surfaceFormatsCount,
                                                  vulkanContext->surfaceFormats);
    if (result != VK_SUCCESS) {
        vulkanContext->surfaceFormatsCount = 0;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkGetPhysicalDeviceSurfaceFormatsKHR(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
}

static void getSwapchainImages(void)
{
    VkResult result;

    SDL_free(vulkanContext->swapchainImages);
    vulkanContext->swapchainImages = NULL;
    result = vkGetSwapchainImagesKHR(vulkanContext->device, vulkanContext->swapchain, &vulkanContext->swapchainImageCount, NULL);
    if (result != VK_SUCCESS) {
        vulkanContext->swapchainImageCount = 0;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkGetSwapchainImagesKHR(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
    vulkanContext->swapchainImages = SDL_malloc(sizeof(VkImage) * vulkanContext->swapchainImageCount);
    if (!vulkanContext->swapchainImages) {
        quit(2);
    }
    result = vkGetSwapchainImagesKHR(vulkanContext->device,
                                     vulkanContext->swapchain,
                                     &vulkanContext->swapchainImageCount,
                                     vulkanContext->swapchainImages);
    if (result != VK_SUCCESS) {
        SDL_free(vulkanContext->swapchainImages);
        vulkanContext->swapchainImages = NULL;
        vulkanContext->swapchainImageCount = 0;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkGetSwapchainImagesKHR(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
}

static bool createSwapchain(void)
{
    uint32_t i;
    int w, h;
    VkSwapchainCreateInfoKHR createInfo = { 0 };
    VkResult result;
    SDL_WindowFlags flags;

    // pick an image count
    vulkanContext->swapchainDesiredImageCount = vulkanContext->surfaceCapabilities.minImageCount + 1;
    if ((vulkanContext->swapchainDesiredImageCount > vulkanContext->surfaceCapabilities.maxImageCount) &&
        (vulkanContext->surfaceCapabilities.maxImageCount > 0)) {
        vulkanContext->swapchainDesiredImageCount = vulkanContext->surfaceCapabilities.maxImageCount;
    }

    // pick a format
    if ((vulkanContext->surfaceFormatsCount == 1) &&
        (vulkanContext->surfaceFormats[0].format == VK_FORMAT_UNDEFINED)) {
        // aren't any preferred formats, so we pick
        vulkanContext->surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        vulkanContext->surfaceFormat.format = VK_FORMAT_R8G8B8A8_UNORM;
    } else {
        vulkanContext->surfaceFormat = vulkanContext->surfaceFormats[0];
        for (i = 0; i < vulkanContext->surfaceFormatsCount; i++) {
            if (vulkanContext->surfaceFormats[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
                vulkanContext->surfaceFormat = vulkanContext->surfaceFormats[i];
                break;
            }
        }
    }

    // get size
    SDL_GetWindowSizeInPixels(vulkanContext->window, &w, &h);

    // get flags
    flags = SDL_GetWindowFlags(vulkanContext->window);

    // Clamp the size to the allowable image extent.
    // SDL_GetWindowSizeInPixels()'s result it not always in this range (bug #3287)
    vulkanContext->swapchainSize.width = SDL_clamp((uint32_t)w,
                                                   vulkanContext->surfaceCapabilities.minImageExtent.width,
                                                   vulkanContext->surfaceCapabilities.maxImageExtent.width);

    vulkanContext->swapchainSize.height = SDL_clamp((uint32_t)h,
                                                    vulkanContext->surfaceCapabilities.minImageExtent.height,
                                                    vulkanContext->surfaceCapabilities.maxImageExtent.height);

    if (w == 0 || h == 0) {
        return false;
    }

    getSurfaceCaps();

    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vulkanContext->surface;
    createInfo.minImageCount = vulkanContext->swapchainDesiredImageCount;
    createInfo.imageFormat = vulkanContext->surfaceFormat.format;
    createInfo.imageColorSpace = vulkanContext->surfaceFormat.colorSpace;
    createInfo.imageExtent = vulkanContext->swapchainSize;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = vulkanContext->surfaceCapabilities.currentTransform;
    if (flags & SDL_WINDOW_TRANSPARENT) {
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else {
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = vulkanContext->swapchain;
    result = vkCreateSwapchainKHR(vulkanContext->device, &createInfo, NULL, &vulkanContext->swapchain);

    if (createInfo.oldSwapchain) {
        vkDestroySwapchainKHR(vulkanContext->device, createInfo.oldSwapchain, NULL);
    }

    if (result != VK_SUCCESS) {
        vulkanContext->swapchain = VK_NULL_HANDLE;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkCreateSwapchainKHR(): %s",
                     getVulkanResultString(result));
        quit(2);
    }

    getSwapchainImages();
    return true;
}

static void destroySwapchain(void)
{
    if (vulkanContext->swapchain) {
        vkDestroySwapchainKHR(vulkanContext->device, vulkanContext->swapchain, NULL);
        vulkanContext->swapchain = VK_NULL_HANDLE;
    }
    SDL_free(vulkanContext->swapchainImages);
    vulkanContext->swapchainImages = NULL;
}

static void destroyCommandBuffers(void)
{
    if (vulkanContext->commandBuffers) {
        vkFreeCommandBuffers(vulkanContext->device,
                             vulkanContext->commandPool,
                             vulkanContext->swapchainImageCount,
                             vulkanContext->commandBuffers);
        SDL_free(vulkanContext->commandBuffers);
        vulkanContext->commandBuffers = NULL;
    }
}

static void destroyCommandPool(void)
{
    if (vulkanContext->commandPool) {
        vkDestroyCommandPool(vulkanContext->device, vulkanContext->commandPool, NULL);
    }
    vulkanContext->commandPool = VK_NULL_HANDLE;
}

static void createCommandPool(void)
{
    VkResult result;
    VkCommandPoolCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    createInfo.queueFamilyIndex = vulkanContext->graphicsQueueFamilyIndex;
    result = vkCreateCommandPool(vulkanContext->device, &createInfo, NULL, &vulkanContext->commandPool);
    if (result != VK_SUCCESS) {
        vulkanContext->commandPool = VK_NULL_HANDLE;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkCreateCommandPool(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
}

static void createCommandBuffers(void)
{
    VkResult result;
    VkCommandBufferAllocateInfo allocateInfo = { 0 };
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = vulkanContext->commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = vulkanContext->swapchainImageCount;
    vulkanContext->commandBuffers = (VkCommandBuffer *)SDL_malloc(sizeof(VkCommandBuffer) * vulkanContext->swapchainImageCount);
    result = vkAllocateCommandBuffers(vulkanContext->device, &allocateInfo, vulkanContext->commandBuffers);
    if (result != VK_SUCCESS) {
        SDL_free(vulkanContext->commandBuffers);
        vulkanContext->commandBuffers = NULL;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkAllocateCommandBuffers(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
}

static void createFences(void)
{
    uint32_t i;

    vulkanContext->fences = SDL_malloc(sizeof(VkFence) * vulkanContext->swapchainImageCount);
    if (!vulkanContext->fences) {
        quit(2);
    }
    for (i = 0; i < vulkanContext->swapchainImageCount; i++) {
        VkResult result;
        VkFenceCreateInfo createInfo = { 0 };
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        result = vkCreateFence(vulkanContext->device, &createInfo, NULL, &vulkanContext->fences[i]);
        if (result != VK_SUCCESS) {
            for (; i > 0; i--) {
                vkDestroyFence(vulkanContext->device, vulkanContext->fences[i - 1], NULL);
            }
            SDL_free(vulkanContext->fences);
            vulkanContext->fences = NULL;
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "vkCreateFence(): %s",
                         getVulkanResultString(result));
            quit(2);
        }
    }
}

static void destroyFences(void)
{
    uint32_t i;

    if (!vulkanContext->fences) {
        return;
    }

    for (i = 0; i < vulkanContext->swapchainImageCount; i++) {
        vkDestroyFence(vulkanContext->device, vulkanContext->fences[i], NULL);
    }
    SDL_free(vulkanContext->fences);
    vulkanContext->fences = NULL;
}

static void recordPipelineImageBarrier(VkCommandBuffer commandBuffer,
                                       VkAccessFlags sourceAccessMask,
                                       VkAccessFlags destAccessMask,
                                       VkImageLayout sourceLayout,
                                       VkImageLayout destLayout,
                                       VkImage image)
{
    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = sourceAccessMask;
    barrier.dstAccessMask = destAccessMask;
    barrier.oldLayout = sourceLayout;
    barrier.newLayout = destLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         &barrier);
}

static void rerecordCommandBuffer(uint32_t frameIndex, const VkClearColorValue *clearColor)
{
    VkCommandBuffer commandBuffer = vulkanContext->commandBuffers[frameIndex];
    VkImage image = vulkanContext->swapchainImages[frameIndex];
    VkCommandBufferBeginInfo beginInfo = { 0 };
    VkImageSubresourceRange clearRange = { 0 };

    VkResult result = vkResetCommandBuffer(commandBuffer, 0);
    if (result != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkResetCommandBuffer(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkBeginCommandBuffer(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
    recordPipelineImageBarrier(commandBuffer,
                               0,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               image);
    clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearRange.baseMipLevel = 0;
    clearRange.levelCount = 1;
    clearRange.baseArrayLayer = 0;
    clearRange.layerCount = 1;
    vkCmdClearColorImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, clearColor, 1, &clearRange);
    recordPipelineImageBarrier(commandBuffer,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_ACCESS_MEMORY_READ_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                               image);
    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkEndCommandBuffer(): %s",
                     getVulkanResultString(result));
        quit(2);
    }
}

static void destroySwapchainAndSwapchainSpecificStuff(bool doDestroySwapchain)
{
    if (vkDeviceWaitIdle != NULL) {
        vkDeviceWaitIdle(vulkanContext->device);
    }
    destroyFences();
    destroyCommandBuffers();
    destroyCommandPool();
    if (doDestroySwapchain) {
        destroySwapchain();
    }
}

static bool createNewSwapchainAndSwapchainSpecificStuff(void)
{
    destroySwapchainAndSwapchainSpecificStuff(false);
    getSurfaceCaps();
    getSurfaceFormats();
    if (!createSwapchain()) {
        return false;
    }
    createCommandPool();
    createCommandBuffers();
    createFences();
    return true;
}

static void initVulkan(void)
{
    int i;

    SDL_Vulkan_LoadLibrary(NULL);

    vulkanContexts = (VulkanContext *)SDL_calloc(state->num_windows, sizeof(VulkanContext));
    if (!vulkanContexts) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!");
        quit(2);
    }

    for (i = 0; i < state->num_windows; ++i) {
        vulkanContext = &vulkanContexts[i];
        vulkanContext->window = state->windows[i];
        loadGlobalFunctions();
        createInstance();
        loadInstanceFunctions();
        createSurface();
        findPhysicalDevice();
        createDevice();
        loadDeviceFunctions();
        getQueues();
        createSemaphores();
        createNewSwapchainAndSwapchainSpecificStuff();
    }
}

static void shutdownVulkan(bool doDestroySwapchain)
{
    if (vulkanContexts) {
        int i;
        for (i = 0; i < state->num_windows; ++i) {
            vulkanContext = &vulkanContexts[i];
            if (vulkanContext->device && vkDeviceWaitIdle) {
                vkDeviceWaitIdle(vulkanContext->device);
            }

            destroySwapchainAndSwapchainSpecificStuff(doDestroySwapchain);

            if (vulkanContext->imageAvailableSemaphore && vkDestroySemaphore) {
                vkDestroySemaphore(vulkanContext->device, vulkanContext->imageAvailableSemaphore, NULL);
            }

            if (vulkanContext->renderingFinishedSemaphore && vkDestroySemaphore) {
                vkDestroySemaphore(vulkanContext->device, vulkanContext->renderingFinishedSemaphore, NULL);
            }

            if (vulkanContext->device && vkDestroyDevice) {
                vkDestroyDevice(vulkanContext->device, NULL);
            }

            if (vulkanContext->surface && vkDestroySurfaceKHR) {
                vkDestroySurfaceKHR(vulkanContext->instance, vulkanContext->surface, NULL);
            }

            if (vulkanContext->instance && vkDestroyInstance) {
                vkDestroyInstance(vulkanContext->instance, NULL);
            }

            SDL_free(vulkanContext->surfaceFormats);
        }
        SDL_free(vulkanContexts);
        vulkanContexts = NULL;
    }

    SDL_Vulkan_UnloadLibrary();
}

static bool render(void)
{
    uint32_t frameIndex;
    VkResult rc;
    double currentTime;
    VkClearColorValue clearColor = { { 0 } };
    VkPipelineStageFlags waitDestStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo = { 0 };
    VkPresentInfoKHR presentInfo = { 0 };
    int w, h;

    if (!vulkanContext->swapchain) {
        bool result = createNewSwapchainAndSwapchainSpecificStuff();
        if (!result) {
            SDL_Delay(100);
        }
        return result;
    }
    rc = vkAcquireNextImageKHR(vulkanContext->device,
                               vulkanContext->swapchain,
                               UINT64_MAX,
                               vulkanContext->imageAvailableSemaphore,
                               VK_NULL_HANDLE,
                               &frameIndex);
    if (rc == VK_ERROR_OUT_OF_DATE_KHR) {
        return createNewSwapchainAndSwapchainSpecificStuff();
    }

    if ((rc != VK_SUBOPTIMAL_KHR) && (rc != VK_SUCCESS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkAcquireNextImageKHR(): %s",
                     getVulkanResultString(rc));
        quit(2);
    }
    rc = vkWaitForFences(vulkanContext->device, 1, &vulkanContext->fences[frameIndex], VK_FALSE, UINT64_MAX);
    if (rc != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "vkWaitForFences(): %s", getVulkanResultString(rc));
        quit(2);
    }
    rc = vkResetFences(vulkanContext->device, 1, &vulkanContext->fences[frameIndex]);
    if (rc != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "vkResetFences(): %s", getVulkanResultString(rc));
        quit(2);
    }
    currentTime = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
    clearColor.float32[0] = (float)(0.5 + 0.5 * SDL_sin(currentTime));
    clearColor.float32[1] = (float)(0.5 + 0.5 * SDL_sin(currentTime + SDL_PI_D * 2 / 3));
    clearColor.float32[2] = (float)(0.5 + 0.5 * SDL_sin(currentTime + SDL_PI_D * 4 / 3));
    clearColor.float32[3] = 0.5; // for SDL_WINDOW_TRANSPARENT, ignored with VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
    rerecordCommandBuffer(frameIndex, &clearColor);
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vulkanContext->imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitDestStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vulkanContext->commandBuffers[frameIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &vulkanContext->renderingFinishedSemaphore;
    rc = vkQueueSubmit(vulkanContext->graphicsQueue, 1, &submitInfo, vulkanContext->fences[frameIndex]);

    if (rc != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "vkQueueSubmit(): %s", getVulkanResultString(rc));
        quit(2);
    }
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &vulkanContext->renderingFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &vulkanContext->swapchain;
    presentInfo.pImageIndices = &frameIndex;
    rc = vkQueuePresentKHR(vulkanContext->presentQueue, &presentInfo);
    if ((rc == VK_ERROR_OUT_OF_DATE_KHR) || (rc == VK_SUBOPTIMAL_KHR)) {
        return createNewSwapchainAndSwapchainSpecificStuff();
    }

    if (rc != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vkQueuePresentKHR(): %s",
                     getVulkanResultString(rc));
        quit(2);
    }
    SDL_GetWindowSizeInPixels(vulkanContext->window, &w, &h);
    if (w != (int)vulkanContext->swapchainSize.width || h != (int)vulkanContext->swapchainSize.height) {
        return createNewSwapchainAndSwapchainSpecificStuff();
    }
    return true;
}

int main(int argc, char **argv)
{
    int done;
    const SDL_DisplayMode *mode;
    SDL_Event event;
    Uint64 then, now;
    Uint32 frames;
    int dw, dh;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    /* Set Vulkan parameters */
    state->window_flags |= SDL_WINDOW_VULKAN;
    state->skip_renderer = 1;

    if (!SDLTest_CommonDefaultArgs(state, argc, argv) || !SDLTest_CommonInit(state)) {
        SDLTest_CommonQuit(state);
        return 1;
    }

    mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    if (mode) {
        SDL_Log("Screen BPP    : %d", SDL_BITSPERPIXEL(mode->format));
    }
    SDL_GetWindowSize(state->windows[0], &dw, &dh);
    SDL_Log("Window Size   : %d,%d", dw, dh);
    SDL_GetWindowSizeInPixels(state->windows[0], &dw, &dh);
    SDL_Log("Draw Size     : %d,%d", dw, dh);
    SDL_Log("%s", "");

    initVulkan();

    /* Main render loop */
    frames = 0;
    then = SDL_GetTicks();
    done = 0;
    while (!done) {
        /* Check for events */
        frames++;
        while (SDL_PollEvent(&event)) {
            /* Need to destroy the swapchain before the window created
             * by SDL.
             */
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                destroySwapchainAndSwapchainSpecificStuff(true);
            }
            SDLTest_CommonEvent(state, &event, &done);
        }

        if (!done) {
            int i;
            for (i = 0; i < state->num_windows; ++i) {
                if (state->windows[i]) {
                    vulkanContext = &vulkanContexts[i];
                    render();
                }
            }
        }
    }

    /* Print out some timing information */
    now = SDL_GetTicks();
    if (now > then) {
        SDL_Log("%2.2f frames per second", ((double)frames * 1000) / (now - then));
    }

    shutdownVulkan(true);
    SDLTest_CommonQuit(state);
    return 0;
}

#endif
