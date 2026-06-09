/*
  Simple DirectMedia Layer
  Copyright , (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef SDL_PROC_OPTIONAL
#define SDL_PROC_OPTIONAL(ret, func, params) SDL_PROC(ret, func, params)
#endif

#define SDL_PROC_UNUSED(ret, func, params)

SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_Create, (OH_AudioStreamBuilder** builder, OH_AudioStream_Type type))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_Destroy, (OH_AudioStreamBuilder* builder))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_SetSamplingRate, (OH_AudioStreamBuilder* builder, int32_t rate))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_SetChannelCount, (OH_AudioStreamBuilder* builder, int32_t channelCount))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_SetSampleFormat, (OH_AudioStreamBuilder* builder, OH_AudioStream_SampleFormat format))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_SetLatencyMode, (OH_AudioStreamBuilder* builder, OH_AudioStream_LatencyMode latencyMode))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_SetChannelLayout, (OH_AudioStreamBuilder* builder, OH_AudioChannelLayout channelLayout))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_SetRendererInfo, (OH_AudioStreamBuilder* builder, OH_AudioStream_Usage usage))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_SetCapturerInfo, (OH_AudioStreamBuilder* builder, OH_AudioStream_SourceType sourceType))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_GenerateRenderer, (OH_AudioStreamBuilder* builder, OH_AudioRenderer** audioRenderer))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_GenerateCapturer, (OH_AudioStreamBuilder* builder, OH_AudioCapturer** audioCapturer))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_SetRendererWriteDataCallback, (OH_AudioStreamBuilder* builder, OH_AudioRenderer_OnWriteDataCallback callback, void* userData))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_SetRendererErrorCallback, (OH_AudioStreamBuilder* builder, OH_AudioRenderer_OnErrorCallback callback, void* userData))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_SetCapturerReadDataCallback, (OH_AudioStreamBuilder* builder, OH_AudioCapturer_OnReadDataCallback callback, void* userData))
SDL_PROC(OH_AudioStream_Result, OH_AudioStreamBuilder_SetCapturerErrorCallback, (OH_AudioStreamBuilder* builder, OH_AudioCapturer_OnErrorCallback callback, void* userData))
SDL_PROC(OH_AudioStream_Result, OH_AudioRenderer_Release, (OH_AudioRenderer* renderer))
SDL_PROC(OH_AudioStream_Result, OH_AudioRenderer_Start, (OH_AudioRenderer* renderer))
SDL_PROC(OH_AudioStream_Result, OH_AudioRenderer_Pause, (OH_AudioRenderer* renderer))
SDL_PROC(OH_AudioStream_Result, OH_AudioRenderer_Stop, (OH_AudioRenderer* renderer))
SDL_PROC(OH_AudioStream_Result, OH_AudioRenderer_GetSamplingRate, (OH_AudioRenderer* renderer, int32_t* rate))
SDL_PROC(OH_AudioStream_Result, OH_AudioRenderer_GetChannelCount, (OH_AudioRenderer* renderer, int32_t* channelCount))
SDL_PROC(OH_AudioStream_Result, OH_AudioRenderer_GetSampleFormat, (OH_AudioRenderer* renderer, OH_AudioStream_SampleFormat* sampleFormat))
SDL_PROC(OH_AudioStream_Result, OH_AudioRenderer_GetFrameSizeInCallback, (OH_AudioRenderer* renderer, int32_t* frameSize))
SDL_PROC(OH_AudioStream_Result, OH_AudioCapturer_Release, (OH_AudioCapturer* capturer))
SDL_PROC(OH_AudioStream_Result, OH_AudioCapturer_Start, (OH_AudioCapturer* capturer))
SDL_PROC(OH_AudioStream_Result, OH_AudioCapturer_Pause, (OH_AudioCapturer* capturer))
SDL_PROC(OH_AudioStream_Result, OH_AudioCapturer_Stop, (OH_AudioCapturer* capturer))
SDL_PROC(OH_AudioStream_Result, OH_AudioCapturer_GetSamplingRate, (OH_AudioCapturer* capturer, int32_t* rate))
SDL_PROC(OH_AudioStream_Result, OH_AudioCapturer_GetChannelCount, (OH_AudioCapturer* capturer, int32_t* channelCount))
SDL_PROC(OH_AudioStream_Result, OH_AudioCapturer_GetSampleFormat, (OH_AudioCapturer* capturer, OH_AudioStream_SampleFormat* sampleFormat))
SDL_PROC(OH_AudioStream_Result, OH_AudioCapturer_GetFrameSizeInCallback, (OH_AudioCapturer* capturer, int32_t* frameSize))

#undef SDL_PROC
#undef SDL_PROC_UNUSED
#undef SDL_PROC_OPTIONAL
