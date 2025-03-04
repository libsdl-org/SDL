#ifndef WGPU_H_
#define WGPU_H_

#include "webgpu.h"

typedef enum WGPUNativeSType {
    // Start at 0003 since that's allocated range for wgpu-native
    WGPUSType_DeviceExtras = 0x00030001,
    WGPUSType_NativeLimits = 0x00030002,
    WGPUSType_PipelineLayoutExtras = 0x00030003,
    WGPUSType_ShaderModuleGLSLDescriptor = 0x00030004,
    WGPUSType_InstanceExtras = 0x00030006,
    WGPUSType_BindGroupEntryExtras = 0x00030007,
    WGPUSType_BindGroupLayoutEntryExtras = 0x00030008,
    WGPUSType_QuerySetDescriptorExtras = 0x00030009,
    WGPUSType_SurfaceConfigurationExtras = 0x0003000A,
    WGPUNativeSType_Force32 = 0x7FFFFFFF
} WGPUNativeSType;

typedef enum WGPUNativeFeature {
    WGPUNativeFeature_PushConstants = 0x00030001,
    WGPUNativeFeature_TextureAdapterSpecificFormatFeatures = 0x00030002,
    WGPUNativeFeature_MultiDrawIndirect = 0x00030003,
    WGPUNativeFeature_MultiDrawIndirectCount = 0x00030004,
    WGPUNativeFeature_VertexWritableStorage = 0x00030005,
    WGPUNativeFeature_TextureBindingArray = 0x00030006,
    WGPUNativeFeature_SampledTextureAndStorageBufferArrayNonUniformIndexing = 0x00030007,
    WGPUNativeFeature_PipelineStatisticsQuery = 0x00030008,
    WGPUNativeFeature_StorageResourceBindingArray = 0x00030009,
    WGPUNativeFeature_PartiallyBoundBindingArray = 0x0003000A,
    WGPUNativeFeature_TextureFormat16bitNorm = 0x0003000B,
    WGPUNativeFeature_TextureCompressionAstcHdr = 0x0003000C,
    WGPUNativeFeature_MappablePrimaryBuffers = 0x0003000E,
    WGPUNativeFeature_BufferBindingArray = 0x0003000F,
    WGPUNativeFeature_UniformBufferAndStorageTextureArrayNonUniformIndexing = 0x00030010,
    // TODO: requires wgpu.h api change
    // WGPUNativeFeature_AddressModeClampToZero = 0x00030011,
    // WGPUNativeFeature_AddressModeClampToBorder = 0x00030012,
    // WGPUNativeFeature_PolygonModeLine = 0x00030013,
    // WGPUNativeFeature_PolygonModePoint = 0x00030014,
    // WGPUNativeFeature_ConservativeRasterization = 0x00030015,
    // WGPUNativeFeature_ClearTexture = 0x00030016,
    WGPUNativeFeature_SpirvShaderPassthrough = 0x00030017,
    // WGPUNativeFeature_Multiview = 0x00030018,
    WGPUNativeFeature_VertexAttribute64bit = 0x00030019,
    WGPUNativeFeature_TextureFormatNv12 = 0x0003001A,
    WGPUNativeFeature_RayTracingAccelerationStructure = 0x0003001B,
    WGPUNativeFeature_RayQuery = 0x0003001C,
    WGPUNativeFeature_ShaderF64 = 0x0003001D,
    WGPUNativeFeature_ShaderI16 = 0x0003001E,
    WGPUNativeFeature_ShaderPrimitiveIndex = 0x0003001F,
    WGPUNativeFeature_ShaderEarlyDepthTest = 0x00030020,
    WGPUNativeFeature_Subgroup = 0x00030021,
    WGPUNativeFeature_SubgroupVertex = 0x00030022,
    WGPUNativeFeature_SubgroupBarrier = 0x00030023,
    WGPUNativeFeature_TimestampQueryInsideEncoders = 0x00030024,
    WGPUNativeFeature_TimestampQueryInsidePasses = 0x00030025,
    WGPUNativeFeature_Force32 = 0x7FFFFFFF
} WGPUNativeFeature;

typedef enum WGPULogLevel {
    WGPULogLevel_Off = 0x00000000,
    WGPULogLevel_Error = 0x00000001,
    WGPULogLevel_Warn = 0x00000002,
    WGPULogLevel_Info = 0x00000003,
    WGPULogLevel_Debug = 0x00000004,
    WGPULogLevel_Trace = 0x00000005,
    WGPULogLevel_Force32 = 0x7FFFFFFF
} WGPULogLevel;

typedef WGPUFlags WGPUInstanceBackend;
static const WGPUInstanceBackend WGPUInstanceBackend_All = 0x00000000;
static const WGPUInstanceBackend WGPUInstanceBackend_Vulkan = 1 << 0;
static const WGPUInstanceBackend WGPUInstanceBackend_GL = 1 << 1;
static const WGPUInstanceBackend WGPUInstanceBackend_Metal = 1 << 2;
static const WGPUInstanceBackend WGPUInstanceBackend_DX12 = 1 << 3;
static const WGPUInstanceBackend WGPUInstanceBackend_DX11 = 1 << 4;
static const WGPUInstanceBackend WGPUInstanceBackend_BrowserWebGPU = 1 << 5;
// Vulkan, Metal, DX12 and BrowserWebGPU
static const WGPUInstanceBackend WGPUInstanceBackend_Primary = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5);
// GL and DX11
static const WGPUInstanceBackend WGPUInstanceBackend_Secondary = (1 << 1) | (1 << 4);
static const WGPUInstanceBackend WGPUInstanceBackend_Force32 = 0x7FFFFFFF;

typedef WGPUFlags WGPUInstanceFlag;
static const WGPUInstanceFlag WGPUInstanceFlag_Default = 0x00000000;
static const WGPUInstanceFlag WGPUInstanceFlag_Debug = 1 << 0;
static const WGPUInstanceFlag WGPUInstanceFlag_Validation = 1 << 1;
static const WGPUInstanceFlag WGPUInstanceFlag_DiscardHalLabels = 1 << 2;
static const WGPUInstanceFlag WGPUInstanceFlag_Force32 = 0x7FFFFFFF;

typedef enum WGPUDx12Compiler {
    WGPUDx12Compiler_Undefined = 0x00000000,
    WGPUDx12Compiler_Fxc = 0x00000001,
    WGPUDx12Compiler_Dxc = 0x00000002,
    WGPUDx12Compiler_Force32 = 0x7FFFFFFF
} WGPUDx12Compiler;

typedef enum WGPUGles3MinorVersion {
    WGPUGles3MinorVersion_Automatic = 0x00000000,
    WGPUGles3MinorVersion_Version0 = 0x00000001,
    WGPUGles3MinorVersion_Version1 = 0x00000002,
    WGPUGles3MinorVersion_Version2 = 0x00000003,
    WGPUGles3MinorVersion_Force32 = 0x7FFFFFFF
} WGPUGles3MinorVersion;

typedef enum WGPUPipelineStatisticName {
    WGPUPipelineStatisticName_VertexShaderInvocations = 0x00000000,
    WGPUPipelineStatisticName_ClipperInvocations = 0x00000001,
    WGPUPipelineStatisticName_ClipperPrimitivesOut = 0x00000002,
    WGPUPipelineStatisticName_FragmentShaderInvocations = 0x00000003,
    WGPUPipelineStatisticName_ComputeShaderInvocations = 0x00000004,
    WGPUPipelineStatisticName_Force32 = 0x7FFFFFFF
} WGPUPipelineStatisticName WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUNativeQueryType {
    WGPUNativeQueryType_PipelineStatistics = 0x00030000,
    WGPUNativeQueryType_Force32 = 0x7FFFFFFF
} WGPUNativeQueryType WGPU_ENUM_ATTRIBUTE;

typedef struct WGPUInstanceExtras {
    WGPUChainedStruct chain;
    WGPUInstanceBackend backends;
    WGPUInstanceFlag flags;
    WGPUDx12Compiler dx12ShaderCompiler;
    WGPUGles3MinorVersion gles3MinorVersion;
    WGPUStringView dxilPath;
    WGPUStringView dxcPath;
} WGPUInstanceExtras;

typedef struct WGPUDeviceExtras {
    WGPUChainedStruct chain;
    WGPUStringView tracePath;
} WGPUDeviceExtras;

typedef struct WGPUNativeLimits {
    /** This struct chain is used as mutable in some places and immutable in others. */
    WGPUChainedStructOut chain;
    uint32_t maxPushConstantSize;
    uint32_t maxNonSamplerBindings;
} WGPUNativeLimits;

typedef struct WGPUPushConstantRange {
    WGPUShaderStage stages;
    uint32_t start;
    uint32_t end;
} WGPUPushConstantRange;

typedef struct WGPUPipelineLayoutExtras {
    WGPUChainedStruct chain;
    size_t pushConstantRangeCount;
    WGPUPushConstantRange const * pushConstantRanges;
} WGPUPipelineLayoutExtras;

typedef uint64_t WGPUSubmissionIndex;

typedef struct WGPUShaderDefine {
    WGPUStringView name;
    WGPUStringView value;
} WGPUShaderDefine;

typedef struct WGPUShaderModuleGLSLDescriptor {
    WGPUChainedStruct chain;
    WGPUShaderStage stage;
    WGPUStringView code;
    uint32_t defineCount;
    WGPUShaderDefine * defines;
} WGPUShaderModuleGLSLDescriptor;

typedef struct WGPUShaderModuleDescriptorSpirV {
    WGPUStringView label;
    uint32_t sourceSize;
    uint32_t const * source;
} WGPUShaderModuleDescriptorSpirV;

typedef struct WGPURegistryReport {
   size_t numAllocated;
   size_t numKeptFromUser;
   size_t numReleasedFromUser;
   size_t elementSize;
} WGPURegistryReport;

typedef struct WGPUHubReport {
    WGPURegistryReport adapters;
    WGPURegistryReport devices;
    WGPURegistryReport queues;
    WGPURegistryReport pipelineLayouts;
    WGPURegistryReport shaderModules;
    WGPURegistryReport bindGroupLayouts;
    WGPURegistryReport bindGroups;
    WGPURegistryReport commandBuffers;
    WGPURegistryReport renderBundles;
    WGPURegistryReport renderPipelines;
    WGPURegistryReport computePipelines;
    WGPURegistryReport pipelineCaches;
    WGPURegistryReport querySets;
    WGPURegistryReport buffers;
    WGPURegistryReport textures;
    WGPURegistryReport textureViews;
    WGPURegistryReport samplers;
} WGPUHubReport;

typedef struct WGPUGlobalReport {
    WGPURegistryReport surfaces;
    WGPUHubReport hub;
} WGPUGlobalReport;

typedef struct WGPUInstanceEnumerateAdapterOptions {
    WGPUChainedStruct const * nextInChain;
    WGPUInstanceBackend backends;
} WGPUInstanceEnumerateAdapterOptions;

typedef struct WGPUBindGroupEntryExtras {
    WGPUChainedStruct chain;
    WGPUBuffer const * buffers;
    size_t bufferCount;
    WGPUSampler const * samplers;
    size_t samplerCount;
    WGPUTextureView const * textureViews;
    size_t textureViewCount;
} WGPUBindGroupEntryExtras;

typedef struct WGPUBindGroupLayoutEntryExtras {
    WGPUChainedStruct chain;
    uint32_t count;
} WGPUBindGroupLayoutEntryExtras;

typedef struct WGPUQuerySetDescriptorExtras {
    WGPUChainedStruct chain;
    WGPUPipelineStatisticName const * pipelineStatistics;
    size_t pipelineStatisticCount;
} WGPUQuerySetDescriptorExtras WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUSurfaceConfigurationExtras {
    WGPUChainedStruct chain;
    uint32_t desiredMaximumFrameLatency;
} WGPUSurfaceConfigurationExtras WGPU_STRUCTURE_ATTRIBUTE;

typedef void (*WGPULogCallback)(WGPULogLevel level, WGPUStringView message, void * userdata);

typedef enum WGPUNativeTextureFormat {
    // From Features::TEXTURE_FORMAT_16BIT_NORM
    WGPUNativeTextureFormat_R16Unorm = 0x00030001,
    WGPUNativeTextureFormat_R16Snorm = 0x00030002,
    WGPUNativeTextureFormat_Rg16Unorm = 0x00030003,
    WGPUNativeTextureFormat_Rg16Snorm = 0x00030004,
    WGPUNativeTextureFormat_Rgba16Unorm = 0x00030005,
    WGPUNativeTextureFormat_Rgba16Snorm = 0x00030006,
    // From Features::TEXTURE_FORMAT_NV12
    WGPUNativeTextureFormat_NV12 = 0x00030007,
} WGPUNativeTextureFormat;

#ifdef __cplusplus
extern "C" {
#endif

void wgpuGenerateReport(WGPUInstance instance, WGPUGlobalReport * report);
size_t wgpuInstanceEnumerateAdapters(WGPUInstance instance, WGPU_NULLABLE WGPUInstanceEnumerateAdapterOptions const * options, WGPUAdapter * adapters);

WGPUSubmissionIndex wgpuQueueSubmitForIndex(WGPUQueue queue, size_t commandCount, WGPUCommandBuffer const * commands);

// Returns true if the queue is empty, or false if there are more queue submissions still in flight.
WGPUBool wgpuDevicePoll(WGPUDevice device, WGPUBool wait, WGPU_NULLABLE WGPUSubmissionIndex const * wrappedSubmissionIndex);
WGPUShaderModule wgpuDeviceCreateShaderModuleSpirV(WGPUDevice device, WGPUShaderModuleDescriptorSpirV const * descriptor);

void wgpuSetLogCallback(WGPULogCallback callback, void * userdata);

void wgpuSetLogLevel(WGPULogLevel level);

uint32_t wgpuGetVersion(void);

void wgpuRenderPassEncoderSetPushConstants(WGPURenderPassEncoder encoder, WGPUShaderStage stages, uint32_t offset, uint32_t sizeBytes, void const * data);
void wgpuComputePassEncoderSetPushConstants(WGPUComputePassEncoder encoder, uint32_t offset, uint32_t sizeBytes, void const * data);
void wgpuRenderBundleEncoderSetPushConstants(WGPURenderBundleEncoder encoder, WGPUShaderStage stages, uint32_t offset, uint32_t sizeBytes, void const * data);

void wgpuRenderPassEncoderMultiDrawIndirect(WGPURenderPassEncoder encoder, WGPUBuffer buffer, uint64_t offset, uint32_t count);
void wgpuRenderPassEncoderMultiDrawIndexedIndirect(WGPURenderPassEncoder encoder, WGPUBuffer buffer, uint64_t offset, uint32_t count);

void wgpuRenderPassEncoderMultiDrawIndirectCount(WGPURenderPassEncoder encoder, WGPUBuffer buffer, uint64_t offset, WGPUBuffer count_buffer, uint64_t count_buffer_offset, uint32_t max_count);
void wgpuRenderPassEncoderMultiDrawIndexedIndirectCount(WGPURenderPassEncoder encoder, WGPUBuffer buffer, uint64_t offset, WGPUBuffer count_buffer, uint64_t count_buffer_offset, uint32_t max_count);

void wgpuComputePassEncoderBeginPipelineStatisticsQuery(WGPUComputePassEncoder computePassEncoder, WGPUQuerySet querySet, uint32_t queryIndex);
void wgpuComputePassEncoderEndPipelineStatisticsQuery(WGPUComputePassEncoder computePassEncoder);
void wgpuRenderPassEncoderBeginPipelineStatisticsQuery(WGPURenderPassEncoder renderPassEncoder, WGPUQuerySet querySet, uint32_t queryIndex);
void wgpuRenderPassEncoderEndPipelineStatisticsQuery(WGPURenderPassEncoder renderPassEncoder);

void wgpuComputePassEncoderWriteTimestamp(WGPUComputePassEncoder computePassEncoder, WGPUQuerySet querySet, uint32_t queryIndex);
void wgpuRenderPassEncoderWriteTimestamp(WGPURenderPassEncoder renderPassEncoder, WGPUQuerySet querySet, uint32_t queryIndex);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
