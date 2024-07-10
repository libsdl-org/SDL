//
// This is a coccinelle semantic patch to ease migration of your project from SDL2 to SDL3.
//
// It generates a patch that you can apply to your project to build for SDL3. It does not
// handle conceptual API changes, but it automates API name changes and function parameter
// transformations.
//
// To install (native Ubuntu or using WSL on Windows):
//	sudo apt install coccinelle
//
// Apply the semantic patch to generate a patch file:
//	cd path/to/your/code
//	spatch --sp-file path/to/SDL_migration.cocci . >patch.txt
//
// A few options:
//   --c++=11            to parse cpp file
//   --max-width 200     to increase line width of generated source
//
// Apply the patch to your project:
//	patch -p1 <patch.txt
//
//
// #############
// In very short, a semantic patch is composed of two sub-blocks, like
//
// @@
// declaration
// @@
// rule / transformation
//
// So this file is a set of many semantic patches, mostly independent.


@ rule_audio_open @
expression e1, e2;
@@
- SDL_OpenAudio(e1, e2)
+ (g_audio_id = SDL_OpenAudioDevice(NULL, 0, e1, e2, 0)) > 0 ? 0 : -1

@ depends on rule_audio_open @
@@
{
+ /* FIXME MIGRATION: maybe move this to a global scope ? */
+ SDL_AudioDeviceID g_audio_id = -1;
...
SDL_OpenAudioDevice(...)
...
}

@@
@@
- SDL_LockAudio()
+ SDL_LockAudioDevice(g_audio_id)

@@
@@
- SDL_UnlockAudio()
+ SDL_UnlockAudioDevice(g_audio_id)

@@
@@
- SDL_CloseAudio(void)
+ SDL_CloseAudioDevice(g_audio_id)

@@
expression e;
@@
- SDL_PauseAudio(e)
+ e == SDL_TRUE ? SDL_PauseAudioDevice(g_audio_id) : SDL_PlayAudioDevice(g_audio_id)

@@
@@
- SDL_GetAudioStatus()
+ SDL_GetAudioDeviceStatus(g_audio_id)

@@
@@
- SDL_GetQueuedAudioSize(1)
+ SDL_GetQueuedAudioSize(g_audio_id)

@@
expression e1, e2;
@@
- SDL_QueueAudio(1, e1, e2)
+ SDL_QueueAudio(g_audio_id, e1, e2)




// SDL_EventState() - replaced with SDL_SetEventEnabled()
@@
expression e1;
@@
(
- SDL_EventState(e1, SDL_IGNORE)
+ SDL_SetEventEnabled(e1, SDL_FALSE)
|
- SDL_EventState(e1, SDL_DISABLE)
+ SDL_SetEventEnabled(e1, SDL_FALSE)
|
- SDL_EventState(e1, SDL_ENABLE)
+ SDL_SetEventEnabled(e1, SDL_TRUE)
|
- SDL_EventState(e1, SDL_QUERY)
+ SDL_EventEnabled(e1)
)

// SDL_GetEventState() - replaced with SDL_EventEnabled()
@@
expression e1;
@@
- SDL_GetEventState(e1)
+ SDL_EventEnabled(e1)

@@
expression e;
@@
- SDL_JoystickGetDevicePlayerIndex(e)
+ /* FIXME MIGRATION: check for valid instance */
+ SDL_GetJoystickInstancePlayerIndex(GetJoystickInstanceFromIndex(e))

@@
expression e;
@@
- SDL_JoystickIsVirtual(e)
+ /* FIXME MIGRATION: check for valid instance */
+ SDL_IsJoystickVirtual(GetJoystickInstanceFromIndex(e))

@@
expression e;
@@
- SDL_JoystickPathForIndex(e)
+ /* FIXME MIGRATION: check for valid instance */
+ SDL_GetJoystickInstancePath(GetJoystickInstanceFromIndex(e))

@@
expression e;
@@
- SDL_IsGameController(e)
+ /* FIXME MIGRATION: check for valid instance */
+ SDL_IsGamepad(GetJoystickInstanceFromIndex(e))

@@
expression e;
@@
- SDL_GameControllerMappingForDeviceIndex(e)
+ /* FIXME MIGRATION: check for valid instance */
+ SDL_GetGamepadInstanceMapping(GetJoystickInstanceFromIndex(e))

@@
expression e;
@@
- SDL_GameControllerNameForIndex(e)
+ /* FIXME MIGRATION: check for valid instance */
+ SDL_GetGamepadInstanceName(GetJoystickInstanceFromIndex(e))

@@
expression e;
@@
- SDL_GameControllerPathForIndex(e)
+ /* FIXME MIGRATION: check for valid instance */
+ SDL_GetGamepadInstancePath(GetJoystickInstanceFromIndex(e))

@@
expression e;
@@
- SDL_GameControllerTypeForIndex(e)
+ /* FIXME MIGRATION: check for valid instance */
+ SDL_GetGamepadInstanceType(GetJoystickInstanceFromIndex(e))


// SDL_Has3DNow() has been removed; there is no replacement.
@@
@@
+ /* FIXME MIGRATION: SDL_Has3DNow() has been removed; there is no replacement. */ 0
- SDL_Has3DNow()

// SDL_HINT_VIDEO_X11_XINERAMA (Xinerama no longer supported by the X11 backend)
@@
@@
+ /* FIXME MIGRATION: no longer support by the X11 backend */ NULL
- SDL_HINT_VIDEO_X11_XINERAMA

// SDL_HINT_VIDEO_X11_XVIDMODE (Xvidmode no longer supported by the X11 backend)
@@
@@
+ /* FIXME MIGRATION: no longer support by the X11 backend */ NULL
- SDL_HINT_VIDEO_X11_XVIDMODE

// SDL_HINT_VIDEO_X11_FORCE_EGL (use SDL_HINT_VIDEO_FORCE_EGL instead)
@@
@@
- SDL_HINT_VIDEO_X11_FORCE_EGL
+ SDL_HINT_VIDEO_FORCE_EGL

@@
@@
- SDL_HINT_AUDIODRIVER
+ SDL_HINT_AUDIO_DRIVER

@@
@@
- SDL_HINT_VIDEODRIVER
+ SDL_HINT_VIDEO_DRIVER

// SDL_GetRevisionNumber() has been removed from the API, it always returned 0 in SDL 2.0.
@@
@@
+ /* FIXME MIGRATION: SDL_GetRevisionNumber() removed */ 0
- SDL_GetRevisionNumber()

// SDL_RWread
@ rule_rwread @
expression e1, e2, e3, e4;
identifier i;
@@
(
   i = SDL_RWread(e1, e2,
-  e3, e4);
+  e3 * e4);
+  i = (i <= 0) ? 0 : i / e3;
|
   SDL_RWread(e1, e2,
-  e3, e4);
+  e3 * e4);
|
+  /* FIXME MIGRATION: double-check if you use the returned value of SDL_RWread() */
   SDL_RWread(e1, e2,
-  e3, e4)
+  e3 * e4)

)

// SDL_RWwrite
@ rule_rwwrite @
expression e1, e2, e3, e4;
identifier i;
@@
(
   i = SDL_RWwrite(e1, e2,
-  e3, e4);
+  e3 * e4);
+  i = (i <= 0) ? 0 : i / e3;
|
   SDL_RWwrite(e1, e2,
-  e3, e4);
+  e3 * e4);
|
+  /* FIXME MIGRATION: double-check if you use the returned value of SDL_RWwrite() */
   SDL_RWwrite(e1, e2,
-  e3, e4)
+  e3 * e4)
)

@ depends on rule_rwread || rule_rwwrite @
expression e;
@@
(
- e * 1
+ e
|
- e / 1
+ e
)

// SDL_SIMDAlloc(), SDL_SIMDFree() have been removed.
@@
expression e1;
@@
- SDL_SIMDAlloc(e1)
+ SDL_aligned_alloc(SDL_SIMDGetAlignment(), e1)

@@
expression e1;
@@
- SDL_SIMDFree(
+ SDL_aligned_free(
  e1)

// SDL_Vulkan_GetInstanceExtensions() no longer takes a window parameter.
@@
expression e1, e2, e3;
@@
  SDL_Vulkan_GetInstanceExtensions(
- e1,
  e2, e3)

// SDL_Vulkan_GetVkGetInstanceProcAddr() now returns `SDL_FunctionPointer` instead of `void *`, and should be cast to PFN_vkGetInstanceProcAddr.
@@
typedef PFN_vkGetInstanceProcAddr;
@@
(
  (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr()
|
+ (PFN_vkGetInstanceProcAddr)
  SDL_Vulkan_GetVkGetInstanceProcAddr()
)

// SDL_PauseAudioDevice / SDL_PlayAudioDevice
@@
expression e;
@@
(
- SDL_PauseAudioDevice(e, 1)
+ SDL_PauseAudioDevice(e)
|
- SDL_PauseAudioDevice(e, SDL_TRUE)
+ SDL_PauseAudioDevice(e)
|
- SDL_PauseAudioDevice(e, 0)
+ SDL_ResumeAudioDevice(e)
|
- SDL_PauseAudioDevice(e, SDL_FALSE)
+ SDL_ResumeAudioDevice(e)
)

@@
expression e, pause_on;
@@
- SDL_PauseAudioDevice(e, pause_on);
+ if (pause_on) {
+    SDL_PauseAudioDevice(e);
+ } else {
+    SDL_ResumeAudioDevice(e);
+ }


// Remove SDL_WINDOW_SHOWN
@@
expression e;
@@
(
- SDL_WINDOW_SHOWN | e
+ e
|
- SDL_WINDOW_SHOWN
+ 0
)


@@
// Remove parameter from SDL_ConvertSurface
expression e1, e2, e3;
@@
SDL_ConvertSurface(e1, e2
- ,e3)
+ )


@@
// Remove parameter from SDL_ConvertSurfaceFormat
expression e1, e2, e3;
@@
SDL_ConvertSurfaceFormat(e1, e2
- ,e3)
+ )


@@
// SDL_CreateRGBSurfaceWithFormat
// remove 'flags'
// remove 'depth'
// rename to SDL_CreateSurface
expression e1, e2, e3, e4, e5;
@@
- SDL_CreateRGBSurfaceWithFormat(e1, e2, e3, e4, e5)
+ SDL_CreateSurface(e2, e3, e5)


@@
// SDL_CreateRGBSurfaceWithFormat:
// remove 'depth'
// rename to SDL_CreateSurfaceFrom
expression e1, e2, e3, e4, e5, e6;
@@
- SDL_CreateRGBSurfaceWithFormatFrom(e1, e2, e3, e4, e5, e6)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e5, e6)



@@
// SDL_CreateRGBSurface : convert Masks to format
expression e1, e2, e3, e4, e5, e6, e7, e8, e9;

@@

(

// Generated for all formats:

- SDL_CreateRGBSurface(e1, e2, e3, 1, 0x00000000, 0x00000000, 0x00000000, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_INDEX1LSB)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 1, e4, 0x00000000, 0x00000000, 0x00000000, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_INDEX1LSB)

|

- SDL_CreateRGBSurface(e1, e2, e3, 1, 0x00000000, 0x00000000, 0x00000000, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_INDEX1MSB)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 1, e4, 0x00000000, 0x00000000, 0x00000000, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_INDEX1MSB)

|

- SDL_CreateRGBSurface(e1, e2, e3, 4, 0x00000000, 0x00000000, 0x00000000, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_INDEX4LSB)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 4, e4, 0x00000000, 0x00000000, 0x00000000, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_INDEX4LSB)

|

- SDL_CreateRGBSurface(e1, e2, e3, 4, 0x00000000, 0x00000000, 0x00000000, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_INDEX4MSB)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 4, e4, 0x00000000, 0x00000000, 0x00000000, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_INDEX4MSB)

|

- SDL_CreateRGBSurface(e1, e2, e3, 8, 0x00000000, 0x00000000, 0x00000000, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_INDEX8)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 8, e4, 0x00000000, 0x00000000, 0x00000000, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_INDEX8)

|

- SDL_CreateRGBSurface(e1, e2, e3, 8, 0x000000E0, 0x0000001C, 0x00000003, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_RGB332)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 8, e4, 0x000000E0, 0x0000001C, 0x00000003, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_RGB332)

|

- SDL_CreateRGBSurface(e1, e2, e3, 12, 0x00000F00, 0x000000F0, 0x0000000F, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_RGB444)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 12, e4, 0x00000F00, 0x000000F0, 0x0000000F, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_RGB444)

|

- SDL_CreateRGBSurface(e1, e2, e3, 15, 0x00007C00, 0x000003E0, 0x0000001F, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_RGB555)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 15, e4, 0x00007C00, 0x000003E0, 0x0000001F, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_RGB555)

|

- SDL_CreateRGBSurface(e1, e2, e3, 15, 0x0000001F, 0x000003E0, 0x00007C00, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_BGR555)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 15, e4, 0x0000001F, 0x000003E0, 0x00007C00, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_BGR555)

|

- SDL_CreateRGBSurface(e1, e2, e3, 16, 0x00000F00, 0x000000F0, 0x0000000F, 0x0000F000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_ARGB4444)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 16, e4, 0x00000F00, 0x000000F0, 0x0000000F, 0x0000F000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_ARGB4444)

|

- SDL_CreateRGBSurface(e1, e2, e3, 16, 0x0000F000, 0x00000F00, 0x000000F0, 0x0000000F)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_RGBA4444)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 16, e4, 0x0000F000, 0x00000F00, 0x000000F0, 0x0000000F)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_RGBA4444)

|

- SDL_CreateRGBSurface(e1, e2, e3, 16, 0x0000000F, 0x000000F0, 0x00000F00, 0x0000F000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_ABGR4444)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 16, e4, 0x0000000F, 0x000000F0, 0x00000F00, 0x0000F000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_ABGR4444)

|

- SDL_CreateRGBSurface(e1, e2, e3, 16, 0x000000F0, 0x00000F00, 0x0000F000, 0x0000000F)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_BGRA4444)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 16, e4, 0x000000F0, 0x00000F00, 0x0000F000, 0x0000000F)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_BGRA4444)

|

- SDL_CreateRGBSurface(e1, e2, e3, 16, 0x00007C00, 0x000003E0, 0x0000001F, 0x00008000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_ARGB1555)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 16, e4, 0x00007C00, 0x000003E0, 0x0000001F, 0x00008000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_ARGB1555)

|

- SDL_CreateRGBSurface(e1, e2, e3, 16, 0x0000F800, 0x000007C0, 0x0000003E, 0x00000001)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_RGBA5551)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 16, e4, 0x0000F800, 0x000007C0, 0x0000003E, 0x00000001)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_RGBA5551)

|

- SDL_CreateRGBSurface(e1, e2, e3, 16, 0x0000001F, 0x000003E0, 0x00007C00, 0x00008000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_ABGR1555)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 16, e4, 0x0000001F, 0x000003E0, 0x00007C00, 0x00008000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_ABGR1555)

|

- SDL_CreateRGBSurface(e1, e2, e3, 16, 0x0000003E, 0x000007C0, 0x0000F800, 0x00000001)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_BGRA5551)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 16, e4, 0x0000003E, 0x000007C0, 0x0000F800, 0x00000001)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_BGRA5551)

|

- SDL_CreateRGBSurface(e1, e2, e3, 16, 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_RGB565)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 16, e4, 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_RGB565)

|

- SDL_CreateRGBSurface(e1, e2, e3, 16, 0x0000001F, 0x000007E0, 0x0000F800, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_BGR565)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 16, e4, 0x0000001F, 0x000007E0, 0x0000F800, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_BGR565)

|

- SDL_CreateRGBSurface(e1, e2, e3, 24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_RGB24)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 24, e4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_RGB24)

|

- SDL_CreateRGBSurface(e1, e2, e3, 24, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_BGR24)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 24, e4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_BGR24)

|

- SDL_CreateRGBSurface(e1, e2, e3, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_XRGB8888)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_XRGB8888)

|

- SDL_CreateRGBSurface(e1, e2, e3, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_RGBX8888)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_RGBX8888)

|

- SDL_CreateRGBSurface(e1, e2, e3, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_XBGR8888)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_XBGR8888)

|

- SDL_CreateRGBSurface(e1, e2, e3, 32, 0x0000FF00, 0x00FF0000, 0xFF000000, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_BGRX8888)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0x0000FF00, 0x00FF0000, 0xFF000000, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_BGRX8888)

|

- SDL_CreateRGBSurface(e1, e2, e3, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_ARGB8888)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_ARGB8888)

|

- SDL_CreateRGBSurface(e1, e2, e3, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_RGBA8888)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_RGBA8888)

|

- SDL_CreateRGBSurface(e1, e2, e3, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_ABGR8888)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_ABGR8888)

|

- SDL_CreateRGBSurface(e1, e2, e3, 32, 0x0000FF00, 0x00FF0000, 0xFF000000, 0x000000FF)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_BGRA8888)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0x0000FF00, 0x00FF0000, 0xFF000000, 0x000000FF)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_BGRA8888)

|

- SDL_CreateRGBSurface(e1, e2, e3, 32, 0x3FF00000, 0x000FFC00, 0x000003FF, 0xC0000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_ARGB2101010)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0x3FF00000, 0x000FFC00, 0x000003FF, 0xC0000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_ARGB2101010)

|

// End Generated


- SDL_CreateRGBSurface(e1, e2, e3, e4->BitsPerPixel, e4->Rmask, e4->Gmask, e4->Bmask, e4->Amask)
+ SDL_CreateSurface(e2, e3, e4->format)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, e4->BitsPerPixel, e5, e4->Rmask, e4->Gmask, e4->Bmask, e4->Amask)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e5, e4->format)

|

-SDL_CreateRGBSurface(e1, e2, e3, e4, e5, e6, e7, e8)
+SDL_CreateSurface(e2, e3, SDL_MasksToPixelFormatEnum(e4, e5, e6, e7, e8))

|

-SDL_CreateRGBSurfaceFrom(e1, e2, e3, e4, e5, e6, e7, e8, e9)
+SDL_CreateSurfaceFrom(e1, e2, e3, e5, SDL_MasksToPixelFormatEnum(e4, e6, e7, e8, e9))

)

@@
// SDL_CreateRenderer:
// 2nd argument changed from int (default=-1) to const char* (default=NULL)
expression e1, e3;
int e2;
@@

(

-SDL_CreateRenderer(e1, -1, e3)
+SDL_CreateRenderer(e1, NULL, e3)

|

-SDL_CreateRenderer(e1, e2, e3)
+SDL_CreateRenderer(e1, SDL_GetRenderDriver(e2), e3)

)

// Renaming of SDL_oldnames.h

@@
@@
- SDL_AudioStreamAvailable
+ SDL_GetAudioStreamAvailable
  (...)
@@
@@
- SDL_AudioStreamClear
+ SDL_ClearAudioStream
  (...)
@@
@@
- SDL_AudioStreamFlush
+ SDL_FlushAudioStream
  (...)
@@
@@
- SDL_AudioStreamGet
+ SDL_GetAudioStreamData
  (...)
@@
@@
- SDL_AudioStreamPut
+ SDL_PutAudioStreamData
  (...)
@@
@@
- SDL_FreeAudioStream
+ SDL_DestroyAudioStream
  (...)
@@
@@
- SDL_FreeWAV
+ SDL_free
  (...)
@@
@@
- SDL_NewAudioStream
+ SDL_CreateAudioStream
  (...)
@@
@@
- SDL_CONTROLLERAXISMOTION
+ SDL_EVENT_GAMEPAD_AXIS_MOTION
@@
@@
- SDL_CONTROLLERBUTTONDOWN
+ SDL_EVENT_GAMEPAD_BUTTON_DOWN
@@
@@
- SDL_CONTROLLERBUTTONUP
+ SDL_EVENT_GAMEPAD_BUTTON_UP
@@
@@
- SDL_CONTROLLERDEVICEADDED
+ SDL_EVENT_GAMEPAD_ADDED
@@
@@
- SDL_CONTROLLERDEVICEREMAPPED
+ SDL_EVENT_GAMEPAD_REMAPPED
@@
@@
- SDL_CONTROLLERDEVICEREMOVED
+ SDL_EVENT_GAMEPAD_REMOVED
@@
@@
- SDL_CONTROLLERSENSORUPDATE
+ SDL_EVENT_GAMEPAD_SENSOR_UPDATE
@@
@@
- SDL_CONTROLLERTOUCHPADDOWN
+ SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN
@@
@@
- SDL_CONTROLLERTOUCHPADMOTION
+ SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION
@@
@@
- SDL_CONTROLLERTOUCHPADUP
+ SDL_EVENT_GAMEPAD_TOUCHPAD_UP
@@
typedef SDL_ControllerAxisEvent, SDL_GamepadAxisEvent;
@@
- SDL_ControllerAxisEvent
+ SDL_GamepadAxisEvent
@@
typedef SDL_ControllerButtonEvent, SDL_GamepadButtonEvent;
@@
- SDL_ControllerButtonEvent
+ SDL_GamepadButtonEvent
@@
typedef SDL_ControllerDeviceEvent, SDL_GamepadDeviceEvent;
@@
- SDL_ControllerDeviceEvent
+ SDL_GamepadDeviceEvent
@@
typedef SDL_ControllerSensorEvent, SDL_GamepadSensorEvent;
@@
- SDL_ControllerSensorEvent
+ SDL_GamepadSensorEvent
@@
typedef SDL_ControllerTouchpadEvent, SDL_GamepadTouchpadEvent;
@@
- SDL_ControllerTouchpadEvent
+ SDL_GamepadTouchpadEvent
@@
@@
- SDL_CONTROLLER_AXIS_INVALID
+ SDL_GAMEPAD_AXIS_INVALID
@@
@@
- SDL_CONTROLLER_AXIS_LEFTX
+ SDL_GAMEPAD_AXIS_LEFTX
@@
@@
- SDL_CONTROLLER_AXIS_LEFTY
+ SDL_GAMEPAD_AXIS_LEFTY
@@
@@
- SDL_CONTROLLER_AXIS_MAX
+ SDL_GAMEPAD_AXIS_MAX
@@
@@
- SDL_CONTROLLER_AXIS_RIGHTX
+ SDL_GAMEPAD_AXIS_RIGHTX
@@
@@
- SDL_CONTROLLER_AXIS_RIGHTY
+ SDL_GAMEPAD_AXIS_RIGHTY
@@
@@
- SDL_CONTROLLER_AXIS_TRIGGERLEFT
+ SDL_GAMEPAD_AXIS_LEFT_TRIGGER
@@
@@
- SDL_CONTROLLER_AXIS_TRIGGERRIGHT
+ SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
@@
@@
- SDL_CONTROLLER_BINDTYPE_AXIS
+ SDL_GAMEPAD_BINDTYPE_AXIS
@@
@@
- SDL_CONTROLLER_BINDTYPE_BUTTON
+ SDL_GAMEPAD_BINDTYPE_BUTTON
@@
@@
- SDL_CONTROLLER_BINDTYPE_HAT
+ SDL_GAMEPAD_BINDTYPE_HAT
@@
@@
- SDL_CONTROLLER_BINDTYPE_NONE
+ SDL_GAMEPAD_BINDTYPE_NONE
@@
@@
- SDL_CONTROLLER_BUTTON_A
+ SDL_GAMEPAD_BUTTON_A
@@
@@
- SDL_CONTROLLER_BUTTON_B
+ SDL_GAMEPAD_BUTTON_B
@@
@@
- SDL_CONTROLLER_BUTTON_BACK
+ SDL_GAMEPAD_BUTTON_BACK
@@
@@
- SDL_CONTROLLER_BUTTON_DPAD_DOWN
+ SDL_GAMEPAD_BUTTON_DPAD_DOWN
@@
@@
- SDL_CONTROLLER_BUTTON_DPAD_LEFT
+ SDL_GAMEPAD_BUTTON_DPAD_LEFT
@@
@@
- SDL_CONTROLLER_BUTTON_DPAD_RIGHT
+ SDL_GAMEPAD_BUTTON_DPAD_RIGHT
@@
@@
- SDL_CONTROLLER_BUTTON_DPAD_UP
+ SDL_GAMEPAD_BUTTON_DPAD_UP
@@
@@
- SDL_CONTROLLER_BUTTON_GUIDE
+ SDL_GAMEPAD_BUTTON_GUIDE
@@
@@
- SDL_CONTROLLER_BUTTON_INVALID
+ SDL_GAMEPAD_BUTTON_INVALID
@@
@@
- SDL_CONTROLLER_BUTTON_LEFTSHOULDER
+ SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
@@
@@
- SDL_CONTROLLER_BUTTON_LEFTSTICK
+ SDL_GAMEPAD_BUTTON_LEFT_STICK
@@
@@
- SDL_CONTROLLER_BUTTON_MAX
+ SDL_GAMEPAD_BUTTON_MAX
@@
@@
- SDL_CONTROLLER_BUTTON_MISC1
+ SDL_GAMEPAD_BUTTON_MISC1
@@
@@
- SDL_CONTROLLER_BUTTON_PADDLE1
+ SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1
@@
@@
- SDL_CONTROLLER_BUTTON_PADDLE2
+ SDL_GAMEPAD_BUTTON_LEFT_PADDLE1
@@
@@
- SDL_CONTROLLER_BUTTON_PADDLE3
+ SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2
@@
@@
- SDL_CONTROLLER_BUTTON_PADDLE4
+ SDL_GAMEPAD_BUTTON_LEFT_PADDLE2
@@
@@
- SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
+ SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
@@
@@
- SDL_CONTROLLER_BUTTON_RIGHTSTICK
+ SDL_GAMEPAD_BUTTON_RIGHT_STICK
@@
@@
- SDL_CONTROLLER_BUTTON_START
+ SDL_GAMEPAD_BUTTON_START
@@
@@
- SDL_CONTROLLER_BUTTON_TOUCHPAD
+ SDL_GAMEPAD_BUTTON_TOUCHPAD
@@
@@
- SDL_CONTROLLER_BUTTON_X
+ SDL_GAMEPAD_BUTTON_X
@@
@@
- SDL_CONTROLLER_BUTTON_Y
+ SDL_GAMEPAD_BUTTON_Y
@@
@@
- SDL_CONTROLLER_TYPE_AMAZON_LUNA
+ SDL_GAMEPAD_TYPE_AMAZON_LUNA
@@
@@
- SDL_CONTROLLER_TYPE_GOOGLE_STADIA
+ SDL_GAMEPAD_TYPE_GOOGLE_STADIA
@@
@@
- SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT
+ SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT
@@
@@
- SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR
+ SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR
@@
@@
- SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT
+ SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT
@@
@@
- SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO
+ SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO
@@
@@
- SDL_CONTROLLER_TYPE_NVIDIA_SHIELD
+ SDL_GAMEPAD_TYPE_NVIDIA_SHIELD
@@
@@
- SDL_CONTROLLER_TYPE_PS3
+ SDL_GAMEPAD_TYPE_PS3
@@
@@
- SDL_CONTROLLER_TYPE_PS4
+ SDL_GAMEPAD_TYPE_PS4
@@
@@
- SDL_CONTROLLER_TYPE_PS5
+ SDL_GAMEPAD_TYPE_PS5
@@
@@
- SDL_CONTROLLER_TYPE_UNKNOWN
+ SDL_GAMEPAD_TYPE_UNKNOWN
@@
@@
- SDL_CONTROLLER_TYPE_VIRTUAL
+ SDL_GAMEPAD_TYPE_VIRTUAL
@@
@@
- SDL_CONTROLLER_TYPE_XBOX360
+ SDL_GAMEPAD_TYPE_XBOX360
@@
@@
- SDL_CONTROLLER_TYPE_XBOXONE
+ SDL_GAMEPAD_TYPE_XBOXONE
@@
typedef SDL_GameController, SDL_Gamepad;
@@
- SDL_GameController
+ SDL_Gamepad
@@
@@
- SDL_GameControllerAddMapping
+ SDL_AddGamepadMapping
  (...)
@@
@@
- SDL_GameControllerAddMappingsFromFile
+ SDL_AddGamepadMappingsFromFile
  (...)
@@
@@
- SDL_GameControllerAddMappingsFromRW
+ SDL_AddGamepadMappingsFromIO
  (...)
@@
typedef SDL_GameControllerAxis, SDL_GamepadAxis;
@@
- SDL_GameControllerAxis
+ SDL_GamepadAxis
@@
typedef SDL_GameControllerBindType, SDL_GamepadBindingType;
@@
- SDL_GameControllerBindType
+ SDL_GamepadBindingType
@@
typedef SDL_GameControllerButton, SDL_GamepadButton;
@@
- SDL_GameControllerButton
+ SDL_GamepadButton
@@
@@
- SDL_GameControllerClose
+ SDL_CloseGamepad
  (...)
@@
@@
- SDL_GameControllerFromInstanceID
+ SDL_GetGamepadFromInstanceID
  (...)
@@
@@
- SDL_GameControllerFromPlayerIndex
+ SDL_GetGamepadFromPlayerIndex
  (...)
@@
@@
- SDL_GameControllerGetAppleSFSymbolsNameForAxis
+ SDL_GetGamepadAppleSFSymbolsNameForAxis
  (...)
@@
@@
- SDL_GameControllerGetAppleSFSymbolsNameForButton
+ SDL_GetGamepadAppleSFSymbolsNameForButton
  (...)
@@
@@
- SDL_GameControllerGetAttached
+ SDL_GamepadConnected
  (...)
@@
@@
- SDL_GameControllerGetAxis
+ SDL_GetGamepadAxis
  (...)
@@
@@
- SDL_GameControllerGetAxisFromString
+ SDL_GetGamepadAxisFromString
  (...)
@@
@@
- SDL_GameControllerGetButton
+ SDL_GetGamepadButton
  (...)
@@
@@
- SDL_GameControllerGetButtonFromString
+ SDL_GetGamepadButtonFromString
  (...)
@@
@@
- SDL_GameControllerGetFirmwareVersion
+ SDL_GetGamepadFirmwareVersion
  (...)
@@
@@
- SDL_GameControllerGetJoystick
+ SDL_GetGamepadJoystick
  (...)
@@
@@
- SDL_GameControllerGetNumTouchpadFingers
+ SDL_GetGamepadNumTouchpadFingers
  (...)
@@
@@
- SDL_GameControllerGetNumTouchpads
+ SDL_GetGamepadNumTouchpads
  (...)
@@
@@
- SDL_GameControllerGetPlayerIndex
+ SDL_GetGamepadPlayerIndex
  (...)
@@
@@
- SDL_GameControllerGetProduct
+ SDL_GetGamepadProduct
  (...)
@@
@@
- SDL_GameControllerGetProductVersion
+ SDL_GetGamepadProductVersion
  (...)
@@
@@
- SDL_GameControllerGetSensorData
+ SDL_GetGamepadSensorData
  (...)
@@
@@
- SDL_GameControllerGetSensorDataRate
+ SDL_GetGamepadSensorDataRate
  (...)
@@
@@
- SDL_GameControllerGetSerial
+ SDL_GetGamepadSerial
  (...)
@@
@@
- SDL_GameControllerGetStringForAxis
+ SDL_GetGamepadStringForAxis
  (...)
@@
@@
- SDL_GameControllerGetStringForButton
+ SDL_GetGamepadStringForButton
  (...)
@@
@@
- SDL_GameControllerGetTouchpadFinger
+ SDL_GetGamepadTouchpadFinger
  (...)
@@
@@
- SDL_GameControllerGetType
+ SDL_GetGamepadType
  (...)
@@
@@
- SDL_GameControllerGetVendor
+ SDL_GetGamepadVendor
  (...)
@@
@@
- SDL_GameControllerHasAxis
+ SDL_GamepadHasAxis
  (...)
@@
@@
- SDL_GameControllerHasButton
+ SDL_GamepadHasButton
  (...)
@@
@@
- SDL_GameControllerHasSensor
+ SDL_GamepadHasSensor
  (...)
@@
@@
- SDL_GameControllerIsSensorEnabled
+ SDL_GamepadSensorEnabled
  (...)
@@
@@
- SDL_GameControllerMapping
+ SDL_GetGamepadMapping
  (...)
@@
@@
- SDL_GameControllerMappingForGUID
+ SDL_GetGamepadMappingForGUID
  (...)
@@
@@
- SDL_GameControllerName
+ SDL_GetGamepadName
  (...)
@@
@@
- SDL_GameControllerOpen
+ SDL_OpenGamepad
  (...)
@@
@@
- SDL_GameControllerPath
+ SDL_GetGamepadPath
  (...)
@@
@@
- SDL_GameControllerRumble
+ SDL_RumbleGamepad
  (...)
@@
@@
- SDL_GameControllerRumbleTriggers
+ SDL_RumbleGamepadTriggers
  (...)
@@
@@
- SDL_GameControllerSendEffect
+ SDL_SendGamepadEffect
  (...)
@@
@@
- SDL_GameControllerSetLED
+ SDL_SetGamepadLED
  (...)
@@
@@
- SDL_GameControllerSetPlayerIndex
+ SDL_SetGamepadPlayerIndex
  (...)
@@
@@
- SDL_GameControllerSetSensorEnabled
+ SDL_SetGamepadSensorEnabled
  (...)
@@
@@
- SDL_GameControllerType
+ SDL_GamepadType
  (...)
@@
@@
- SDL_GameControllerUpdate
+ SDL_UpdateGamepads
  (...)
@@
@@
- SDL_INIT_GAMECONTROLLER
+ SDL_INIT_GAMEPAD
@ rule_init_noparachute @
@@
- SDL_INIT_NOPARACHUTE
+ 0
@@
@@
- SDL_JOYSTICK_TYPE_GAMECONTROLLER
+ SDL_JOYSTICK_TYPE_GAMEPAD
@@
@@
- SDL_JoystickAttachVirtualEx
+ SDL_AttachVirtualJoystick
  (...)
@@
@@
- SDL_JoystickClose
+ SDL_CloseJoystick
  (...)
@@
@@
- SDL_JoystickCurrentPowerLevel
+ SDL_GetJoystickPowerLevel
  (...)
@@
@@
- SDL_JoystickDetachVirtual
+ SDL_DetachVirtualJoystick
  (...)
@@
@@
- SDL_JoystickFromInstanceID
+ SDL_GetJoystickFromInstanceID
  (...)
@@
@@
- SDL_JoystickFromPlayerIndex
+ SDL_GetJoystickFromPlayerIndex
  (...)
@@
@@
- SDL_JoystickGetAttached
+ SDL_JoystickConnected
  (...)
@@
@@
- SDL_JoystickGetAxis
+ SDL_GetJoystickAxis
  (...)
@@
@@
- SDL_JoystickGetAxisInitialState
+ SDL_GetJoystickAxisInitialState
  (...)
@@
@@
- SDL_JoystickGetButton
+ SDL_GetJoystickButton
  (...)
@@
@@
- SDL_JoystickGetFirmwareVersion
+ SDL_GetJoystickFirmwareVersion
  (...)
@@
@@
- SDL_JoystickGetGUID
+ SDL_GetJoystickGUID
  (...)
@@
@@
- SDL_JoystickGetGUIDFromString
+ SDL_GetJoystickGUIDFromString
  (...)
@@
@@
- SDL_JoystickGetGUIDString
+ SDL_GetJoystickGUIDString
  (...)
@@
@@
- SDL_JoystickGetHat
+ SDL_GetJoystickHat
  (...)
@@
@@
- SDL_JoystickGetPlayerIndex
+ SDL_GetJoystickPlayerIndex
  (...)
@@
@@
- SDL_JoystickGetProduct
+ SDL_GetJoystickProduct
  (...)
@@
@@
- SDL_JoystickGetProductVersion
+ SDL_GetJoystickProductVersion
  (...)
@@
@@
- SDL_JoystickGetSerial
+ SDL_GetJoystickSerial
  (...)
@@
@@
- SDL_JoystickGetType
+ SDL_GetJoystickType
  (...)
@@
@@
- SDL_JoystickGetVendor
+ SDL_GetJoystickVendor
  (...)
@@
@@
- SDL_JoystickInstanceID
+ SDL_GetJoystickInstanceID
  (...)
@@
@@
- SDL_JoystickName
+ SDL_GetJoystickName
  (...)
@@
@@
- SDL_JoystickNumAxes
+ SDL_GetNumJoystickAxes
  (...)
@@
@@
- SDL_JoystickNumButtons
+ SDL_GetNumJoystickButtons
  (...)
@@
@@
- SDL_JoystickNumHats
+ SDL_GetNumJoystickHats
  (...)
@@
@@
- SDL_JoystickOpen
+ SDL_OpenJoystick
  (...)
@@
@@
- SDL_JoystickPath
+ SDL_GetJoystickPath
  (...)
@@
@@
- SDL_JoystickRumble
+ SDL_RumbleJoystick
  (...)
@@
@@
- SDL_JoystickRumbleTriggers
+ SDL_RumbleJoystickTriggers
  (...)
@@
@@
- SDL_JoystickSendEffect
+ SDL_SendJoystickEffect
  (...)
@@
@@
- SDL_JoystickSetLED
+ SDL_SetJoystickLED
  (...)
@@
@@
- SDL_JoystickSetPlayerIndex
+ SDL_SetJoystickPlayerIndex
  (...)
@@
@@
- SDL_JoystickSetVirtualAxis
+ SDL_SetJoystickVirtualAxis
  (...)
@@
@@
- SDL_JoystickSetVirtualButton
+ SDL_SetJoystickVirtualButton
  (...)
@@
@@
- SDL_JoystickSetVirtualHat
+ SDL_SetJoystickVirtualHat
  (...)
@@
@@
- SDL_JoystickUpdate
+ SDL_UpdateJoysticks
  (...)
@@
@@
- SDL_IsScreenKeyboardShown
+ SDL_ScreenKeyboardShown
  (...)
@@
@@
- SDL_IsTextInputActive
+ SDL_TextInputActive
  (...)
@@
@@
- SDL_IsTextInputShown
+ SDL_TextInputShown
  (...)
@@
@@
- KMOD_ALT
+ SDL_KMOD_ALT
@@
@@
- KMOD_CAPS
+ SDL_KMOD_CAPS
@@
@@
- KMOD_CTRL
+ SDL_KMOD_CTRL
@@
@@
- KMOD_GUI
+ SDL_KMOD_GUI
@@
@@
- KMOD_LALT
+ SDL_KMOD_LALT
@@
@@
- KMOD_LCTRL
+ SDL_KMOD_LCTRL
@@
@@
- KMOD_LGUI
+ SDL_KMOD_LGUI
@@
@@
- KMOD_LSHIFT
+ SDL_KMOD_LSHIFT
@@
@@
- KMOD_MODE
+ SDL_KMOD_MODE
@@
@@
- KMOD_NONE
+ SDL_KMOD_NONE
@@
@@
- KMOD_NUM
+ SDL_KMOD_NUM
@@
@@
- KMOD_RALT
+ SDL_KMOD_RALT
@@
@@
- KMOD_RCTRL
+ SDL_KMOD_RCTRL
@@
@@
- KMOD_RGUI
+ SDL_KMOD_RGUI
@@
@@
- KMOD_RSHIFT
+ SDL_KMOD_RSHIFT
@@
@@
- KMOD_SCROLL
+ SDL_KMOD_SCROLL
@@
@@
- KMOD_SHIFT
+ SDL_KMOD_SHIFT
@@
@@
- SDL_FreeCursor
+ SDL_DestroyCursor
  (...)
@@
@@
- SDL_AllocFormat
+ SDL_GetPixelFormatDetails
  (...)
@@
@@
- SDL_AllocPalette
+ SDL_CreatePalette
  (...)
@@
@@
- SDL_FreePalette
+ SDL_DestroyPalette
  (...)
@@
@@
- SDL_MasksToPixelFormatEnum
+ SDL_GetPixelFormatForMasks
  (...)
@@
@@
- SDL_PixelFormatEnumToMasks
+ SDL_GetMasksForPixelFormat
  (...)
@@
@@
- SDL_EncloseFPoints
+ SDL_GetRectEnclosingPointsFloat
  (...)
@@
@@
- SDL_EnclosePoints
+ SDL_GetRectEnclosingPoints
  (...)
@@
@@
- SDL_FRectEmpty
+ SDL_RectEmptyFloat
  (...)
@@
@@
- SDL_FRectEquals
+ SDL_RectsEqualFloat
  (...)
@@
@@
- SDL_FRectEqualsEpsilon
+ SDL_RectsEqualEpsilon
  (...)
@@
@@
- SDL_HasIntersection
+ SDL_HasRectIntersection
  (...)
@@
@@
- SDL_HasIntersectionF
+ SDL_HasRectIntersectionFloat
  (...)
@@
@@
- SDL_IntersectFRect
+ SDL_GetRectIntersectionFloat
  (...)
@@
@@
- SDL_IntersectFRectAndLine
+ SDL_GetRectAndLineIntersectionFloat
  (...)
@@
@@
- SDL_IntersectRect
+ SDL_GetRectIntersection
  (...)
@@
@@
- SDL_IntersectRectAndLine
+ SDL_GetRectAndLineIntersection
  (...)
@@
@@
- SDL_PointInFRect
+ SDL_PointInRectFloat
  (...)
@@
@@
- SDL_RectEquals
+ SDL_RectsEqual
  (...)
@@
@@
- SDL_UnionFRect
+ SDL_GetRectUnionFloat
  (...)
@@
@@
- SDL_UnionRect
+ SDL_GetRectUnion
  (...)
@@
@@
- SDL_RenderCopyExF
+ SDL_RenderTextureRotated
  (...)
@@
@@
- SDL_RenderCopyF
+ SDL_RenderTexture
  (...)
@@
@@
- SDL_RenderDrawLineF
+ SDL_RenderLine
  (...)
@@
@@
- SDL_RenderDrawLinesF
+ SDL_RenderLines
  (...)
@@
@@
- SDL_RenderDrawPointF
+ SDL_RenderPoint
  (...)
@@
@@
- SDL_RenderDrawPointsF
+ SDL_RenderPoints
  (...)
@@
@@
- SDL_RenderDrawRectF
+ SDL_RenderRect
  (...)
@@
@@
- SDL_RenderDrawRectsF
+ SDL_RenderRects
  (...)
@@
@@
- SDL_RenderFillRectF
+ SDL_RenderFillRect
  (...)
@@
@@
- SDL_RenderFillRectsF
+ SDL_RenderFillRects
  (...)
@@
@@
- SDL_RenderGetClipRect
+ SDL_GetRenderClipRect
  (...)
@@
SDL_Renderer *renderer;
int *e1;
int *e2;
@@
- SDL_RenderGetLogicalSize(renderer, e1, e2)
+ SDL_GetRenderLogicalPresentation(renderer, e1, e2, NULL, NULL)
@@
@@
- SDL_RenderGetMetalCommandEncoder
+ SDL_GetRenderMetalCommandEncoder
  (...)
@@
@@
- SDL_RenderGetMetalLayer
+ SDL_GetRenderMetalLayer
  (...)
@@
@@
- SDL_RenderGetScale
+ SDL_GetRenderScale
  (...)
@@
@@
- SDL_RenderGetViewport
+ SDL_GetRenderViewport
  (...)
@@
@@
- SDL_RenderGetWindow
+ SDL_GetRenderWindow
  (...)
@@
@@
- SDL_RenderIsClipEnabled
+ SDL_RenderClipEnabled
  (...)
@@
@@
- SDL_RenderSetClipRect
+ SDL_SetRenderClipRect
  (...)
@@
SDL_Renderer *renderer;
expression e1;
expression e2;
@@
(
- SDL_RenderSetLogicalSize(renderer, 0, 0)
+ SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED, SDL_ScaleModeNearest)
|
- SDL_RenderSetLogicalSize(renderer, e1, e2)
+ SDL_SetRenderLogicalPresentation(renderer, e1, e2, SDL_LOGICAL_PRESENTATION_LETTERBOX, SDL_ScaleModeLinear)
)
@@
@@
- SDL_RenderSetScale
+ SDL_SetRenderScale
  (...)
@@
@@
- SDL_RenderSetVSync
+ SDL_SetRenderVSync
  (...)
@@
@@
- SDL_RenderSetViewport
+ SDL_SetRenderViewport
  (...)
@@
@@
- RW_SEEK_CUR
+ SDL_IO_SEEK_CUR
@@
@@
- RW_SEEK_END
+ SDL_IO_SEEK_END
@@
@@
- RW_SEEK_SET
+ SDL_IO_SEEK_SET
@@
@@
- SDL_SensorClose
+ SDL_CloseSensor
  (...)
@@
@@
- SDL_SensorFromInstanceID
+ SDL_GetSensorFromInstanceID
  (...)
@@
@@
- SDL_SensorGetData
+ SDL_GetSensorData
  (...)
@@
@@
- SDL_SensorGetInstanceID
+ SDL_GetSensorInstanceID
  (...)
@@
@@
- SDL_SensorGetName
+ SDL_GetSensorName
  (...)
@@
@@
- SDL_SensorGetNonPortableType
+ SDL_GetSensorNonPortableType
  (...)
@@
@@
- SDL_SensorGetType
+ SDL_GetSensorType
  (...)
@@
@@
- SDL_SensorOpen
+ SDL_OpenSensor
  (...)
@@
@@
- SDL_SensorUpdate
+ SDL_UpdateSensors
  (...)
@@
@@
- SDL_FillRect
+ SDL_FillSurfaceRect
  (...)
@@
@@
- SDL_FillRects
+ SDL_FillSurfaceRects
  (...)
@@
@@
- SDL_FreeSurface
+ SDL_DestroySurface
  (...)
@@
@@
- SDL_GetClipRect
+ SDL_GetSurfaceClipRect
  (...)
@@
@@
- SDL_GetColorKey
+ SDL_GetSurfaceColorKey
  (...)
@@
@@
- SDL_HasColorKey
+ SDL_SurfaceHasColorKey
  (...)
@@
@@
- SDL_HasSurfaceRLE
+ SDL_SurfaceHasRLE
  (...)
@@
@@
- SDL_LowerBlit
+ SDL_BlitSurfaceUnchecked
  (...)
@@
expression e1, e2, e3, e4;
@@
- SDL_LowerBlitScaled(e1, e2, e3, e4)
+ SDL_BlitSurfaceUncheckedScaled(e1, e2, e3, e4, SDL_SCALEMODE_NEAREST)
@@
@@
- SDL_SetClipRect
+ SDL_SetSurfaceClipRect
  (...)
@@
@@
- SDL_SetColorKey
+ SDL_SetSurfaceColorKey
  (...)
@@
@@
- SDL_UpperBlit
+ SDL_BlitSurface
  (...)
@@
expression e1, e2, e3, e4;
@@
- SDL_UpperBlitScaled(e1, e2, e3, e4)
+ SDL_BlitSurfaceScaled(e1, e2, e3, e4, SDL_SCALEMODE_NEAREST)
@@
@@
- SDL_RenderGetD3D11Device
+ SDL_GetRenderD3D11Device
  (...)
@@
@@
- SDL_RenderGetD3D9Device
+ SDL_GetRenderD3D9Device
  (...)
@@
@@
- SDL_GetTicks64
+ SDL_GetTicks
  (...)
@@
@@
- SDL_GetPointDisplayIndex
+ SDL_GetDisplayForPoint
  (...)
@@
@@
- SDL_GetRectDisplayIndex
+ SDL_GetDisplayForRect
  (...)
@ depends on rule_init_noparachute @
expression e;
@@
- e | 0
+ e
@@
@@
- SDL_FIRSTEVENT
+ SDL_EVENT_FIRST
@@
@@
- SDL_QUIT
+ SDL_EVENT_QUIT
@@
@@
- SDL_APP_TERMINATING
+ SDL_EVENT_TERMINATING
@@
@@
- SDL_APP_LOWMEMORY
+ SDL_EVENT_LOW_MEMORY
@@
@@
- SDL_APP_WILLENTERBACKGROUND
+ SDL_EVENT_WILL_ENTER_BACKGROUND
@@
@@
- SDL_APP_DIDENTERBACKGROUND
+ SDL_EVENT_DID_ENTER_BACKGROUND
@@
@@
- SDL_APP_WILLENTERFOREGROUND
+ SDL_EVENT_WILL_ENTER_FOREGROUND
@@
@@
- SDL_APP_DIDENTERFOREGROUND
+ SDL_EVENT_DID_ENTER_FOREGROUND
@@
@@
- SDL_LOCALECHANGED
+ SDL_EVENT_LOCALE_CHANGED
@@
@@
- SDL_DISPLAYEVENT_ORIENTATION
+ SDL_EVENT_DISPLAY_ORIENTATION
@@
@@
- SDL_DISPLAYEVENT_CONNECTED
+ SDL_EVENT_DISPLAY_CONNECTED
@@
@@
- SDL_DISPLAYEVENT_DISCONNECTED
+ SDL_EVENT_DISPLAY_DISCONNECTED
@@
@@
- SDL_DISPLAYEVENT_MOVED
+ SDL_EVENT_DISPLAY_MOVED
@@
@@
- SDL_DISPLAYEVENT_FIRST
+ SDL_EVENT_DISPLAY_FIRST
@@
@@
- SDL_DISPLAYEVENT_LAST
+ SDL_EVENT_DISPLAY_LAST
@@
@@
- SDL_SYSWMEVENT
+ SDL_EVENT_SYSWM
@@
@@
- SDL_WINDOWEVENT_SHOWN
+ SDL_EVENT_WINDOW_SHOWN
@@
@@
- SDL_WINDOWEVENT_HIDDEN
+ SDL_EVENT_WINDOW_HIDDEN
@@
@@
- SDL_WINDOWEVENT_EXPOSED
+ SDL_EVENT_WINDOW_EXPOSED
@@
@@
- SDL_WINDOWEVENT_MOVED
+ SDL_EVENT_WINDOW_MOVED
@@
@@
- SDL_WINDOWEVENT_RESIZED
+ SDL_EVENT_WINDOW_RESIZED
@@
@@
- SDL_WINDOWEVENT_SIZE_CHANGED
+ SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
@@
@@
- SDL_WINDOWEVENT_MINIMIZED
+ SDL_EVENT_WINDOW_MINIMIZED
@@
@@
- SDL_WINDOWEVENT_MAXIMIZED
+ SDL_EVENT_WINDOW_MAXIMIZED
@@
@@
- SDL_WINDOWEVENT_RESTORED
+ SDL_EVENT_WINDOW_RESTORED
@@
@@
- SDL_WINDOWEVENT_ENTER
+ SDL_EVENT_WINDOW_MOUSE_ENTER
@@
@@
- SDL_WINDOWEVENT_LEAVE
+ SDL_EVENT_WINDOW_MOUSE_LEAVE
@@
@@
- SDL_WINDOWEVENT_FOCUS_GAINED
+ SDL_EVENT_WINDOW_FOCUS_GAINED
@@
@@
- SDL_WINDOWEVENT_FOCUS_LOST
+ SDL_EVENT_WINDOW_FOCUS_LOST
@@
@@
- SDL_WINDOWEVENT_CLOSE
+ SDL_EVENT_WINDOW_CLOSE_REQUESTED
@@
@@
- SDL_WINDOWEVENT_TAKE_FOCUS
+ SDL_EVENT_WINDOW_TAKE_FOCUS
@@
@@
- SDL_WINDOWEVENT_HIT_TEST
+ SDL_EVENT_WINDOW_HIT_TEST
@@
@@
- SDL_WINDOWEVENT_ICCPROF_CHANGED
+ SDL_EVENT_WINDOW_ICCPROF_CHANGED
@@
@@
- SDL_WINDOWEVENT_DISPLAY_CHANGED
+ SDL_EVENT_WINDOW_DISPLAY_CHANGED
@@
@@
- SDL_WINDOWEVENT_FIRST
+ SDL_EVENT_WINDOW_FIRST
@@
@@
- SDL_WINDOWEVENT_LAST
+ SDL_EVENT_WINDOW_LAST
@@
@@
- SDL_KEYDOWN
+ SDL_EVENT_KEY_DOWN
@@
@@
- SDL_KEYUP
+ SDL_EVENT_KEY_UP
@@
@@
- SDL_TEXTEDITING
+ SDL_EVENT_TEXT_EDITING
@@
@@
- SDL_TEXTINPUT
+ SDL_EVENT_TEXT_INPUT
@@
@@
- SDL_KEYMAPCHANGED
+ SDL_EVENT_KEYMAP_CHANGED
@@
@@
- SDL_TEXTEDITING_EXT
+ SDL_EVENT_TEXT_EDITING_EXT
@@
@@
- SDL_MOUSEMOTION
+ SDL_EVENT_MOUSE_MOTION
@@
@@
- SDL_MOUSEBUTTONDOWN
+ SDL_EVENT_MOUSE_BUTTON_DOWN
@@
@@
- SDL_MOUSEBUTTONUP
+ SDL_EVENT_MOUSE_BUTTON_UP
@@
@@
- SDL_MOUSEWHEEL
+ SDL_EVENT_MOUSE_WHEEL
@@
@@
- SDL_JOYAXISMOTION
+ SDL_EVENT_JOYSTICK_AXIS_MOTION
@@
@@
- SDL_JOYBALLMOTION
+ SDL_EVENT_JOYSTICK_BALL_MOTION
@@
@@
- SDL_JOYHATMOTION
+ SDL_EVENT_JOYSTICK_HAT_MOTION
@@
@@
- SDL_JOYBUTTONDOWN
+ SDL_EVENT_JOYSTICK_BUTTON_DOWN
@@
@@
- SDL_JOYBUTTONUP
+ SDL_EVENT_JOYSTICK_BUTTON_UP
@@
@@
- SDL_JOYDEVICEADDED
+ SDL_EVENT_JOYSTICK_ADDED
@@
@@
- SDL_JOYDEVICEREMOVED
+ SDL_EVENT_JOYSTICK_REMOVED
@@
@@
- SDL_JOYBATTERYUPDATED
+ SDL_EVENT_JOYSTICK_BATTERY_UPDATED
@@
@@
- SDL_FINGERDOWN
+ SDL_EVENT_FINGER_DOWN
@@
@@
- SDL_FINGERUP
+ SDL_EVENT_FINGER_UP
@@
@@
- SDL_FINGERMOTION
+ SDL_EVENT_FINGER_MOTION
@@
@@
- SDL_CLIPBOARDUPDATE
+ SDL_EVENT_CLIPBOARD_UPDATE
@@
@@
- SDL_DROPFILE
+ SDL_EVENT_DROP_FILE
@@
@@
- SDL_DROPTEXT
+ SDL_EVENT_DROP_TEXT
@@
@@
- SDL_DROPBEGIN
+ SDL_EVENT_DROP_BEGIN
@@
@@
- SDL_DROPCOMPLETE
+ SDL_EVENT_DROP_COMPLETE
@@
@@
- SDL_AUDIODEVICEADDED
+ SDL_EVENT_AUDIO_DEVICE_ADDED
@@
@@
- SDL_AUDIODEVICEREMOVED
+ SDL_EVENT_AUDIO_DEVICE_REMOVED
@@
@@
- SDL_SENSORUPDATE
+ SDL_EVENT_SENSOR_UPDATE
@@
@@
- SDL_RENDER_TARGETS_RESET
+ SDL_EVENT_RENDER_TARGETS_RESET
@@
@@
- SDL_RENDER_DEVICE_RESET
+ SDL_EVENT_RENDER_DEVICE_RESET
@@
@@
- SDL_POLLSENTINEL
+ SDL_EVENT_POLL_SENTINEL
@@
@@
- SDL_USEREVENT
+ SDL_EVENT_USER
@@
@@
- SDL_LASTEVENT
+ SDL_EVENT_LAST
@@
@@
- SDL_WINDOW_INPUT_GRABBED
+ SDL_WINDOW_MOUSE_GRABBED
@@
@@
- SDL_GetWindowDisplayIndex
+ SDL_GetDisplayForWindow
  (...)
@@
@@
- SDL_SetWindowDisplayMode
+ SDL_SetWindowFullscreenMode
  (...)
@@
@@
- SDL_GetWindowDisplayMode
+ SDL_GetWindowFullscreenMode
  (...)
@@
@@
- SDL_GetClosestDisplayMode
+ SDL_GetClosestFullscreenDisplayMode
  (...)
@@
@@
- SDL_GetRendererOutputSize
+ SDL_GetCurrentRenderOutputSize
  (...)
@@
@@
- SDL_RenderWindowToLogical
+ SDL_RenderCoordinatesFromWindow
  (...)
@@
@@
- SDL_RenderLogicalToWindow
+ SDL_RenderCoordinatesToWindow
  (...)
@@
symbol SDL_ScaleModeNearest;
@@
- SDL_ScaleModeNearest
+ SDL_SCALEMODE_NEAREST
@@
symbol SDL_ScaleModeLinear;
@@
- SDL_ScaleModeLinear
+ SDL_SCALEMODE_LINEAR
@@
symbol SDL_ScaleModeBest;
@@
- SDL_ScaleModeBest
+ SDL_SCALEMODE_BEST
@@
@@
- SDL_RenderCopy
+ SDL_RenderTexture
  (...)
@@
@@
- SDL_RenderCopyEx
+ SDL_RenderTextureRotated
  (...)
@@
SDL_Renderer *renderer;
constant c1;
constant c2;
constant c3;
constant c4;
expression e1;
expression e2;
expression e3;
expression e4;
@@
- SDL_RenderDrawLine(renderer,
+ SDL_RenderLine(renderer,
(
  c1
|
- e1
+ (float)e1
)
  ,
(
  c2
|
- e2
+ (float)e2
)
  ,
(
  c3
|
- e3
+ (float)e3
)
  ,
(
  c4
|
- e4
+ (float)e4
)
  )
@@
@@
- SDL_RenderDrawLines
+ SDL_RenderLines
  (...)
@@
SDL_Renderer *renderer;
constant c1;
constant c2;
expression e1;
expression e2;
@@
- SDL_RenderDrawPoint(renderer,
+ SDL_RenderPoint(renderer,
(
  c1
|
- e1
+ (float)e1
)
  ,
(
  c2
|
- e2
+ (float)e2
)
  )
@@
@@
- SDL_RenderDrawPoints
+ SDL_RenderPoints
  (...)
@@
@@
- SDL_RenderDrawRect
+ SDL_RenderRect
  (...)
@@
@@
- SDL_RenderDrawRects
+ SDL_RenderRects
  (...)
@@
@@
- SDL_GL_GetDrawableSize
+ SDL_GetWindowSizeInPixels
  (...)
@@
@@
- SDL_Metal_GetDrawableSize
+ SDL_GetWindowSizeInPixels
  (...)
@@
@@
- SDL_Vulkan_GetDrawableSize
+ SDL_GetWindowSizeInPixels
  (...)
@@
@@
- SDL_IsScreenSaverEnabled
+ SDL_ScreenSaverEnabled
  (...)
@@
SDL_Event e1;
@@
- e1.caxis
+ e1.gaxis
@@
SDL_Event *e1;
@@
- e1->caxis
+ e1->gaxis
@@
SDL_Event e1;
@@
- e1.cbutton
+ e1.gbutton
@@
SDL_Event *e1;
@@
- e1->cbutton
+ e1->gbutton
@@
SDL_Event e1;
@@
- e1.cdevice
+ e1.gdevice
@@
SDL_Event *e1;
@@
- e1->cdevice
+ e1->gdevice
@@
SDL_Event e1;
@@
- e1.ctouchpad
+ e1.gtouchpad
@@
SDL_Event *e1;
@@
- e1->ctouchpad
+ e1->gtouchpad
@@
SDL_Event e1;
@@
- e1.csensor
+ e1.gsensor
@@
SDL_Event *e1;
@@
- e1->csensor
+ e1->gsensor
@@
SDL_Event e1;
@@
- e1.wheel.mouseX
+ e1.wheel.mouse_x
@@
SDL_Event *e1;
@@
- e1->wheel.mouseX
+ e1->wheel.mouse_x
@@
SDL_Event e1;
@@
- e1.wheel.mouseY
+ e1.wheel.mouse_y
@@
SDL_Event *e1;
@@
- e1->wheel.mouseY
+ e1->wheel.mouse_y
@@
SDL_Event e1;
@@
- e1.tfinger.touchId
+ e1.tfinger.touchID
@@
SDL_Event *e1;
@@
- e1->tfinger.touchId
+ e1->tfinger.touchID
@@
SDL_Event e1;
@@
- e1.tfinger.fingerId
+ e1.tfinger.fingerID
@@
SDL_Event *e1;
@@
- e1->tfinger.fingerId
+ e1->tfinger.fingerID
@@
expression e1, e2, e3, e4;
@@
- SDL_CreateWindow(e1, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, e2, e3, e4)
+ SDL_CreateWindow(e1, e2, e3, e4)
@@
expression e1, e2, e3, e4, e5, e6;
@@
- SDL_CreateWindow(e1, e2, e3, e4, e5, e6)
+ SDL_CreateWindowWithPosition(e1, e2, e3, e4, e5, e6)
@@
expression e1, e2, e3, e4;
constant c1, c2;
@@
- SDL_CreateShapedWindow(e1, c1, c2, e2, e3, e4)
+ SDL_CreateShapedWindow(e1, e2, e3, e4)
@@
typedef SDL_atomic_t, SDL_AtomicInt;
@@
- SDL_atomic_t
+ SDL_AtomicInt
@@
@@
- SDL_SemWait
+ SDL_WaitSemaphore
  (...)
@@
@@
- SDL_SemTryWait
+ SDL_TryWaitSemaphore
  (...)
@@
@@
- SDL_SemWaitTimeout
+ SDL_WaitSemaphoreTimeout
  (...)
@@
@@
- SDL_SemPost
+ SDL_PostSemaphore
  (...)
@@
@@
- SDL_SemValue
+ SDL_GetSemaphoreValue
  (...)
@@
@@
- SDL_CreateCond
+ SDL_CreateCondition
  (...)
@@
@@
- SDL_DestroyCond
+ SDL_DestroyCondition
  (...)
@@
@@
- SDL_CondSignal
+ SDL_SignalCondition
  (...)
@@
@@
- SDL_CondBroadcast
+ SDL_BroadcastCondition
  (...)
@@
@@
- SDL_CondWait
+ SDL_WaitCondition
  (...)
@@
@@
- SDL_CondWaitTimeout
+ SDL_WaitConditionTimeout
  (...)
@@
typedef SDL_mutex, SDL_Mutex;
@@
- SDL_mutex
+ SDL_Mutex
@@
typedef SDL_sem, SDL_Semaphore;
@@
- SDL_sem
+ SDL_Semaphore
@@
typedef SDL_cond, SDL_Condition;
@@
- SDL_cond
+ SDL_Condition
@@
@@
- AUDIO_F32
+ SDL_AUDIO_F32LE
@@
@@
- AUDIO_F32LSB
+ SDL_AUDIO_F32LE
@@
@@
- AUDIO_F32MSB
+ SDL_AUDIO_F32BE
@@
@@
- AUDIO_F32SYS
+ SDL_AUDIO_F32
@@
@@
- AUDIO_S16
+ SDL_AUDIO_S16LE
@@
@@
- AUDIO_S16LSB
+ SDL_AUDIO_S16LE
@@
@@
- AUDIO_S16MSB
+ SDL_AUDIO_S16BE
@@
@@
- AUDIO_S16SYS
+ SDL_AUDIO_S16
@@
@@
- AUDIO_S32
+ SDL_AUDIO_S32LE
@@
@@
- AUDIO_S32LSB
+ SDL_AUDIO_S32LE
@@
@@
- AUDIO_S32MSB
+ SDL_AUDIO_S32BE
@@
@@
- AUDIO_S32SYS
+ SDL_AUDIO_S32
@@
@@
- AUDIO_S8
+ SDL_AUDIO_S8
@@
@@
- AUDIO_U8
+ SDL_AUDIO_U8
@@
@@
- SDL_WINDOW_ALLOW_HIGHDPI
+ SDL_WINDOW_HIGH_PIXEL_DENSITY
@@
@@
- SDL_TLSCreate
+ SDL_CreateTLS
  (...)
@@
@@
- SDL_TLSGet
+ SDL_GetTLS
  (...)
@@
@@
- SDL_TLSSet
+ SDL_SetTLS
  (...)
@@
@@
- SDL_TLSCleanup
+ SDL_CleanupTLS
  (...)
@@
@@
- SDL_GetDisplayOrientation
+ SDL_GetDisplayCurrentOrientation
  (...)
@@
@@
- SDL_WINDOW_SKIP_TASKBAR
+ SDL_WINDOW_UTILITY
@@
@@
- SDL_PIXELFORMAT_BGR444
+ SDL_PIXELFORMAT_XBGR4444
@@
@@
- SDL_PIXELFORMAT_BGR555
+ SDL_PIXELFORMAT_XBGR1555
@@
@@
- SDL_PIXELFORMAT_BGR888
+ SDL_PIXELFORMAT_XBGR8888
@@
@@
- SDL_PIXELFORMAT_RGB444
+ SDL_PIXELFORMAT_XRGB4444
@@
@@
- SDL_PIXELFORMAT_RGB555
+ SDL_PIXELFORMAT_XRGB1555
@@
@@
- SDL_PIXELFORMAT_RGB888
+ SDL_PIXELFORMAT_XRGB8888
@@
@@
- SDL_strtokr
+ SDL_strtok_r
  (...)
@@
@@
- SDL_ReadLE16
+ SDL_ReadU16LE
  (...)
@@
@@
- SDL_ReadLE32
+ SDL_ReadU32LE
  (...)
@@
@@
- SDL_ReadBE32
+ SDL_ReadU32BE
  (...)
@@
@@
- SDL_ReadBE16
+ SDL_ReadU16BE
  (...)
@@
@@
- SDL_ReadLE64
+ SDL_ReadU64LE
  (...)
@@
@@
- SDL_ReadBE64
+ SDL_ReadU64BE
  (...)
@@
@@
- SDL_WriteLE16
+ SDL_WriteU16LE
  (...)
@@
@@
- SDL_WriteBE16
+ SDL_WriteU16BE
  (...)
@@
@@
- SDL_WriteLE32
+ SDL_WriteU32LE
  (...)
@@
@@
- SDL_WriteBE32
+ SDL_WriteU32BE
  (...)
@@
@@
- SDL_WriteLE64
+ SDL_WriteU64LE
  (...)
@@
@@
- SDL_WriteBE64
+ SDL_WriteU64BE
  (...)
@@
expression e, n;
@@
- SDL_GetWindowData(e, n)
+ SDL_GetProperty(SDL_GetWindowProperties(e), n)
@@
expression e, n, v;
@@
- SDL_SetWindowData(e, n, v)
+ SDL_SetProperty(SDL_GetWindowProperties(e), n, v, NULL, NULL)
@@
expression w, i, s;
@@
- SDL_Vulkan_CreateSurface(w, i, s)
+ SDL_Vulkan_CreateSurface(w, i, NULL, s)
@@
@@
- SDL_RenderFlush
+ SDL_FlushRenderer
  (...)
@@
@@
- SDL_CONTROLLERSTEAMHANDLEUPDATED
+ SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED
@@
@@
- SDL_GameControllerGetSteamHandle
+ SDL_GetGamepadSteamHandle
  (...)
@@
expression e1, e2, e3, e4;
@@
- SDL_SoftStretch(e1, e2, e3, e4)
+ SDL_SoftStretch(e1, e2, e3, e4, SDL_SCALEMODE_NEAREST)
@@
expression e1, e2, e3, e4;
@@
- SDL_SoftStretchLinear(e1, e2, e3, e4)
+ SDL_SoftStretch(e1, e2, e3, e4, SDL_SCALEMODE_LINEAR)
@@
@@
- SDL_HapticClose
+ SDL_CloseHaptic
  (...)
@@
@@
- SDL_HapticOpen
+ SDL_OpenHaptic
  (...)
@@
@@
- SDL_HapticOpenFromMouse
+ SDL_OpenHapticFromMouse
  (...)
@@
@@
- SDL_HapticOpenFromJoystick
+ SDL_OpenHapticFromJoystick
  (...)
@@
@@
- SDL_MouseIsHaptic
+ SDL_IsMouseHaptic
  (...)
@@
@@
- SDL_JoystickIsHaptic
+ SDL_IsJoystickHaptic
  (...)
@@
@@
- SDL_HapticNumEffects
+ SDL_GetMaxHapticEffects
  (...)
@@
@@
- SDL_HapticNumEffectsPlaying
+ SDL_GetMaxHapticEffectsPlaying
  (...)
@@
@@
- SDL_HapticQuery
+ SDL_GetHapticFeatures
  (...)
@@
@@
- SDL_HapticNumAxes
+ SDL_GetNumHapticAxes
  (...)
@@
@@
- SDL_HapticNewEffect
+ SDL_CreateHapticEffect
  (...)
@@
@@
- SDL_HapticUpdateEffect
+ SDL_UpdateHapticEffect
  (...)
@@
@@
- SDL_HapticRunEffect
+ SDL_RunHapticEffect
  (...)
@@
@@
- SDL_HapticStopEffect
+ SDL_StopHapticEffect
  (...)
@@
@@
- SDL_HapticDestroyEffect
+ SDL_DestroyHapticEffect
  (...)
@@
@@
- SDL_HapticGetEffectStatus
+ SDL_GetHapticEffectStatus
  (...)
@@
@@
- SDL_HapticSetGain
+ SDL_SetHapticGain
  (...)
@@
@@
- SDL_HapticSetAutocenter
+ SDL_SetHapticAutocenter
  (...)
@@
@@
- SDL_HapticPause
+ SDL_PauseHaptic
  (...)
@@
@@
- SDL_HapticUnpause
+ SDL_ResumeHaptic
  (...)
@@
@@
- SDL_HapticStopAll
+ SDL_StopHapticEffects
  (...)
@@
@@
- SDL_HapticRumbleInit
+ SDL_InitHapticRumble
  (...)
@@
@@
- SDL_HapticRumblePlay
+ SDL_PlayHapticRumble
  (...)
@@
@@
- SDL_HapticRumbleStop
+ SDL_StopHapticRumble
  (...)
@@
@@
- SDL_AtomicTryLock
+ SDL_TryLockSpinlock
  (...)
@@
@@
- SDL_AtomicLock
+ SDL_LockSpinlock
  (...)
@@
@@
- SDL_AtomicUnlock
+ SDL_UnlockSpinlock
  (...)
@@
@@
- SDL_AtomicCAS
+ SDL_AtomicCompareAndSwap
  (...)
@@
@@
- SDL_AtomicCASPtr
+ SDL_AtomicCompareAndSwapPointer
  (...)
@@
@@
- SDL_ThreadID
+ SDL_GetCurrentThreadID
  (...)
@@
@@
- SDL_threadID
+ SDL_ThreadID
  (...)
@@
@@
- SDL_HasWindowSurface
+ SDL_WindowHasSurface
  (...)
@@
SDL_PixelFormat e1;
@@
- e1.BitsPerPixel
+ e1.bits_per_pixel
@@
SDL_PixelFormat *e1;
@@
- e1->BitsPerPixel
+ e1->bits_per_pixel
@@
SDL_PixelFormat e1;
@@
- e1.BytesPerPixel
+ e1.bytes_per_pixel
@@
SDL_PixelFormat *e1;
@@
- e1->BytesPerPixel
+ e1->bytes_per_pixel
@@
SDL_MessageBoxButtonData e1;
@@
- e1.buttonid
+ e1.buttonID
@@
SDL_MessageBoxButtonData *e1;
@@
- e1->buttonid
+ e1->buttonID
@@
SDL_GamepadBinding e1;
@@
- e1.inputType
+ e1.input_type
@@
SDL_GamepadBinding *e1;
@@
- e1->inputType
+ e1->input_type
@@
SDL_GamepadBinding e1;
@@
- e1.outputType
+ e1.output_type
@@
SDL_GamepadBinding *e1;
@@
- e1->outputType
+ e1->output_type
@@
@@
- SDL_HINT_ALLOW_TOPMOST
+ SDL_HINT_WINDOW_ALLOW_TOPMOST
@@
@@
- SDL_HINT_DIRECTINPUT_ENABLED
+ SDL_HINT_JOYSTICK_DIRECTINPUT
@@
@@
- SDL_HINT_GDK_TEXTINPUT_DEFAULT
+ SDL_HINT_GDK_TEXTINPUT_DEFAULT_TEXT
@@
@@
- SDL_HINT_JOYSTICK_GAMECUBE_RUMBLE_BRAKE
+ SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE_RUMBLE_BRAKE
@@
@@
- SDL_HINT_LINUX_DIGITAL_HATS
+ SDL_HINT_JOYSTICK_LINUX_DIGITAL_HATS
@@
@@
- SDL_HINT_LINUX_HAT_DEADZONES
+ SDL_HINT_JOYSTICK_LINUX_HAT_DEADZONES
@@
@@
- SDL_HINT_LINUX_JOYSTICK_CLASSIC
+ SDL_HINT_JOYSTICK_LINUX_CLASSIC
@@
@@
- SDL_HINT_LINUX_JOYSTICK_DEADZONES
+ SDL_HINT_JOYSTICK_LINUX_DEADZONES
@@
@@
- SDL_HINT_PS2_DYNAMIC_VSYNC
+ SDL_HINT_RENDER_PS2_DYNAMIC_VSYNC
@@
@@
- SDL_JoystickNumBalls
+ SDL_GetNumJoystickBalls
  (...)
@@
@@
- SDL_JoystickGetBall
+ SDL_GetJoystickBall
  (...)
@@
@@
- SDL_RWclose
+ SDL_CloseIO
  (...)
@@
@@
- SDL_RWread
+ SDL_ReadIO
  (...)
@@
@@
- SDL_RWwrite
+ SDL_WriteIO
  (...)
@@
@@
- SDL_RWtell
+ SDL_TellIO
  (...)
@@
@@
- SDL_RWsize
+ SDL_SizeIO
  (...)
@@
@@
- SDL_RWseek
+ SDL_SeekIO
  (...)
@@
@@
- SDL_LoadBMP_RW
+ SDL_LoadBMP_IO
  (...)
@@
@@
- SDL_LoadWAV_RW
+ SDL_LoadWAV_IO
  (...)
@@
@@
- SDL_SaveBMP_RW
+ SDL_SaveBMP_IO
  (...)
@@
@@
- SDL_RWFromFile
+ SDL_IOFromFile
  (...)
@@
@@
- SDL_RWFromMem
+ SDL_IOFromMem
  (...)
@@
@@
- SDL_RWFromConstMem
+ SDL_IOFromConstMem
  (...)
@@
typedef SDL_RWops, SDL_IOStream;
@@
- SDL_RWops
+ SDL_IOStream
@@
@@
- SDL_LogGetOutputFunction
+ SDL_GetLogOutputFunction
  (...)
@@
@@
- SDL_LogSetOutputFunction
+ SDL_SetLogOutputFunction
  (...)
@@
typedef SDL_eventaction, SDL_EventAction;
@@
- SDL_eventaction
+ SDL_EventAction
@@
typedef SDL_RendererFlip, SDL_FlipMode;
@@
- SDL_RendererFlip
+ SDL_FlipMode
@@
typedef SDL_Colour, SDL_Color;
@@
- SDL_Colour
+ SDL_Color
@@
@@
- SDL_WinRTGetFSPathUTF8
+ SDL_WinRTGetFSPath
  (...)
@@
@@
- SDL_iPhoneSetAnimationCallback
+ SDL_iOSSetAnimationCallback
  (...)
@@
@@
- SDL_iPhoneSetEventPump
+ SDL_iOSSetEventPump
  (...)
@@
@@
- SDL_COMPILEDVERSION
+ SDL_VERSION
@@
@@
- SDL_PATCHLEVEL
+ SDL_MICRO_VERSION
@@
@@
- SDL_TABLESIZE
+ SDL_arraysize
@@
@@
- SDLK_QUOTE
+ SDLK_APOSTROPHE
@@
@@
- SDLK_BACKQUOTE
+ SDLK_GRAVE
@@
@@
- SDLK_QUOTEDBL
+ SDLK_DBLAPOSTROPHE
@@
@@
- SDL_LogSetAllPriority
+ SDL_SetLogPriorities
  (...)
@@
@@
- SDL_LogSetPriority
+ SDL_SetLogPriority
  (...)
@@
@@
- SDL_LogGetPriority
+ SDL_GetLogPriority
  (...)
@@
@@
- SDL_LogResetPriorities
+ SDL_ResetLogPriorities
  (...)
@@
@@
- SDL_SIMDGetAlignment
+ SDL_GetSIMDAlignment
  (...)
@@
@@
- SDL_MixAudioFormat
+ SDL_MixAudio
  (...)
@@
@@
- SDL_BlitScaled
+ SDL_BlitSurfaceScaled
  (...)
@@
@@
- SDL_SYSTEM_CURSOR_ARROW
+ SDL_SYSTEM_CURSOR_DEFAULT
@@
@@
- SDL_SYSTEM_CURSOR_IBEAM
+ SDL_SYSTEM_CURSOR_TEXT
@@
@@
- SDL_SYSTEM_CURSOR_WAITARROW
+ SDL_SYSTEM_CURSOR_PROGRESS
@@
@@
- SDL_SYSTEM_CURSOR_SIZENWSE
+ SDL_SYSTEM_CURSOR_NWSE_RESIZE
@@
@@
- SDL_SYSTEM_CURSOR_SIZENESW
+ SDL_SYSTEM_CURSOR_NESW_RESIZE
@@
@@
- SDL_SYSTEM_CURSOR_SIZEWE
+ SDL_SYSTEM_CURSOR_EW_RESIZE
@@
@@
- SDL_SYSTEM_CURSOR_SIZENS
+ SDL_SYSTEM_CURSOR_NS_RESIZE
@@
@@
- SDL_SYSTEM_CURSOR_SIZEALL
+ SDL_SYSTEM_CURSOR_MOVE
@@
@@
- SDL_SYSTEM_CURSOR_NO
+ SDL_SYSTEM_CURSOR_NOT_ALLOWED
@@
@@
- SDL_SYSTEM_CURSOR_HAND
+ SDL_SYSTEM_CURSOR_POINTER
@@
@@
- SDL_SYSTEM_CURSOR_WINDOW_TOPLEFT
+ SDL_SYSTEM_CURSOR_NW_RESIZE
@@
@@
- SDL_SYSTEM_CURSOR_WINDOW_TOP
+ SDL_SYSTEM_CURSOR_N_RESIZE
@@
@@
- SDL_SYSTEM_CURSOR_WINDOW_TOPRIGHT
+ SDL_SYSTEM_CURSOR_NE_RESIZE
@@
@@
- SDL_SYSTEM_CURSOR_WINDOW_RIGHT
+ SDL_SYSTEM_CURSOR_E_RESIZE
@@
@@
- SDL_SYSTEM_CURSOR_WINDOW_BOTTOMRIGHT
+ SDL_SYSTEM_CURSOR_SE_RESIZE
@@
@@
- SDL_SYSTEM_CURSOR_WINDOW_BOTTOM
+ SDL_SYSTEM_CURSOR_S_RESIZE
@@
@@
- SDL_SYSTEM_CURSOR_WINDOW_BOTTOMLEFT
+ SDL_SYSTEM_CURSOR_SW_RESIZE
@@
@@
- SDL_SYSTEM_CURSOR_WINDOW_LEFT
+ SDL_SYSTEM_CURSOR_W_RESIZE
@@
@@
- SDL_SwapLE16
+ SDL_Swap16LE
  (...)
@@
@@
- SDL_SwapLE32
+ SDL_Swap32LE
  (...)
@@
@@
- SDL_SwapBE16
+ SDL_Swap16BE
  (...)
@@
@@
- SDL_SwapBE32
+ SDL_Swap32BE
  (...)
@@
@@
- SDL_SwapLE64
+ SDL_Swap64LE
  (...)
@@
@@
- SDL_SwapBE64
+ SDL_Swap64BE
  (...)
@@
@@
- SDL_SCANCODE_AUDIOMUTE
+ SDL_SCANCODE_MUTE
@@
@@
- SDLK_AUDIOMUTE
+ SDLK_MUTE
@@
@@
- SDL_SCANCODE_EJECT
+ SDL_SCANCODE_MEDIA_EJECT
@@
@@
- SDLK_EJECT
+ SDLK_MEDIA_EJECT
@@
@@
- SDL_SCANCODE_AUDIONEXT
+ SDL_SCANCODE_MEDIA_NEXT_TRACK
@@
@@
- SDLK_AUDIONEXT
+ SDLK_MEDIA_NEXT_TRACK
@@
@@
- SDL_SCANCODE_AUDIOPREV
+ SDL_SCANCODE_MEDIA_PREVIOUS_TRACK
@@
@@
- SDLK_AUDIOPREV
+ SDLK_MEDIA_PREVIOUS_TRACK
@@
@@
- SDL_SCANCODE_AUDIOSTOP
+ SDL_SCANCODE_MEDIA_STOP
@@
@@
- SDLK_AUDIOSTOP
+ SDLK_MEDIA_STOP
@@
@@
- SDL_SCANCODE_AUDIOPLAY
+ SDL_SCANCODE_MEDIA_PLAY
@@
@@
- SDLK_AUDIOPLAY
+ SDLK_MEDIA_PLAY
@@
@@
- SDL_SCANCODE_AUDIOREWIND
+ SDL_SCANCODE_MEDIA_REWIND
@@
@@
- SDLK_AUDIOREWIND
+ SDLK_MEDIA_REWIND
@@
@@
- SDL_SCANCODE_AUDIOFASTFORWARD
+ SDL_SCANCODE_MEDIA_FAST_FORWARD
@@
@@
- SDLK_AUDIOFASTFORWARD
+ SDLK_MEDIA_FAST_FORWARD
@@
@@
- SDL_SCANCODE_MEDIASELECT
+ SDL_SCANCODE_MEDIA_SELECT
@@
@@
- SDLK_MEDIASELECT
+ SDLK_MEDIA_SELECT
@@
@@
- SDLK_a
+ SDLK_A
@@
@@
- SDLK_b
+ SDLK_B
@@
@@
- SDLK_c
+ SDLK_C
@@
@@
- SDLK_d
+ SDLK_D
@@
@@
- SDLK_e
+ SDLK_E
@@
@@
- SDLK_f
+ SDLK_F
@@
@@
- SDLK_g
+ SDLK_G
@@
@@
- SDLK_h
+ SDLK_H
@@
@@
- SDLK_i
+ SDLK_I
@@
@@
- SDLK_j
+ SDLK_J
@@
@@
- SDLK_k
+ SDLK_K
@@
@@
- SDLK_l
+ SDLK_L
@@
@@
- SDLK_m
+ SDLK_M
@@
@@
- SDLK_n
+ SDLK_N
@@
@@
- SDLK_o
+ SDLK_O
@@
@@
- SDLK_p
+ SDLK_P
@@
@@
- SDLK_q
+ SDLK_Q
@@
@@
- SDLK_r
+ SDLK_R
@@
@@
- SDLK_s
+ SDLK_S
@@
@@
- SDLK_t
+ SDLK_T
@@
@@
- SDLK_u
+ SDLK_U
@@
@@
- SDLK_v
+ SDLK_V
@@
@@
- SDLK_w
+ SDLK_W
@@
@@
- SDLK_x
+ SDLK_X
@@
@@
- SDLK_y
+ SDLK_Y
@@
@@
- SDLK_z
+ SDLK_Z
@@
typedef SDL_PixelFormat, SDL_PackedPixelDetails;
@@
- SDL_PixelFormat
+ SDL_PixelFormatDetails
@@
@@
- SDL_ConvertSurfaceFormat
+ SDL_ConvertSurface
  (...)
@@
@@
- SDL_PREALLOC
+ SDL_SURFACE_PREALLOCATED
@@
@@
- SDL_SIMD_ALIGNED
+ SDL_SURFACE_SIMD_ALIGNED
@@
@@
- SDL_GL_DeleteContext
+ SDL_GL_DestroyContext
  (...)
