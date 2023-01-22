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
//   --max-width 200     to increase line witdth of generated source
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
// So this file is a set of many semantic patches, mostly independant.


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

@@
@@
- M_PI
+ SDL_PI_D

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
+ (PFN_vkGetInstanceProcAddr)
  SDL_Vulkan_GetVkGetInstanceProcAddr()


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
+ SDL_PlayAudioDevice(e)
|
- SDL_PauseAudioDevice(e, SDL_FALSE)
+ SDL_PlayAudioDevice(e)
)

@@
expression e, pause_on;
@@
- SDL_PauseAudioDevice(e, pause_on);
+ if (pause_on) {
+    SDL_PauseAudioDevice(e);
+ } else {
+    SDL_PlayAudioDevice(e);
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
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_RGB888)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_RGB888)

|

- SDL_CreateRGBSurface(e1, e2, e3, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_RGBX8888)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_RGBX8888)

|

- SDL_CreateRGBSurface(e1, e2, e3, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000)
+ SDL_CreateSurface(e2, e3, SDL_PIXELFORMAT_BGR888)

|

- SDL_CreateRGBSurfaceFrom(e1, e2, e3, 32, e4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000)
+ SDL_CreateSurfaceFrom(e1, e2, e3, e4, SDL_PIXELFORMAT_BGR888)

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
expression e1, e2, e3;
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
+ SDL_GAMEPADAXISMOTION
@@
@@
- SDL_CONTROLLERBUTTONDOWN
+ SDL_GAMEPADBUTTONDOWN
@@
@@
- SDL_CONTROLLERBUTTONUP
+ SDL_GAMEPADBUTTONUP
@@
@@
- SDL_CONTROLLERDEVICEADDED
+ SDL_GAMEPADADDED
@@
@@
- SDL_CONTROLLERDEVICEREMAPPED
+ SDL_GAMEPADREMAPPED
@@
@@
- SDL_CONTROLLERDEVICEREMOVED
+ SDL_GAMEPADREMOVED
@@
@@
- SDL_CONTROLLERSENSORUPDATE
+ SDL_GAMEPADSENSORUPDATE
@@
@@
- SDL_CONTROLLERTOUCHPADDOWN
+ SDL_GAMEPADTOUCHPADDOWN
@@
@@
- SDL_CONTROLLERTOUCHPADMOTION
+ SDL_GAMEPADTOUCHPADMOTION
@@
@@
- SDL_CONTROLLERTOUCHPADUP
+ SDL_GAMEPADTOUCHPADUP
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
+ SDL_GAMEPAD_BUTTON_PADDLE1
@@
@@
- SDL_CONTROLLER_BUTTON_PADDLE2
+ SDL_GAMEPAD_BUTTON_PADDLE2
@@
@@
- SDL_CONTROLLER_BUTTON_PADDLE3
+ SDL_GAMEPAD_BUTTON_PADDLE3
@@
@@
- SDL_CONTROLLER_BUTTON_PADDLE4
+ SDL_GAMEPAD_BUTTON_PADDLE4
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
+ SDL_AddGamepadMappingsFromRW
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
typedef SDL_GameControllerButtonBind, SDL_GamepadBinding;
@@
- SDL_GameControllerButtonBind
+ SDL_GamepadBinding
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
- SDL_GameControllerGetBindForAxis
+ SDL_GetGamepadBindForAxis
  (...)
@@
@@
- SDL_GameControllerGetBindForButton
+ SDL_GetGamepadBindForButton
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
- SDL_GameControllerHasLED
+ SDL_GamepadHasLED
  (...)
@@
@@
- SDL_GameControllerHasRumble
+ SDL_GamepadHasRumble
  (...)
@@
@@
- SDL_GameControllerHasRumbleTriggers
+ SDL_GamepadHasRumbleTriggers
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
- SDL_GameControllerMappingForIndex
+ SDL_GetGamepadMappingForIndex
  (...)
@@
@@
- SDL_GameControllerName
+ SDL_GetGamepadName
  (...)
@@
@@
- SDL_GameControllerNumMappings
+ SDL_GetNumGamepadMappings
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
- SDL_JoystickAttachVirtual
+ SDL_AttachVirtualJoystick
  (...)
@@
@@
- SDL_JoystickAttachVirtualEx
+ SDL_AttachVirtualJoystickEx
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
- KMOD_RESERVED
+ SDL_KMOD_RESERVED
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
+ SDL_CreatePixelFormat
  (...)
@@
@@
- SDL_AllocPalette
+ SDL_CreatePalette
  (...)
@@
@@
- SDL_FreeFormat
+ SDL_DestroyPixelFormat
  (...)
@@
@@
- SDL_FreePalette
+ SDL_DestroyPalette
  (...)
@@
@@
- SDL_MasksToPixelFormatEnum
+ SDL_GetPixelFormatEnumForMasks
  (...)
@@
@@
- SDL_PixelFormatEnumToMasks
+ SDL_GetMasksForPixelFormatEnum
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
@@
- SDL_RenderGetIntegerScale
+ SDL_GetRenderIntegerScale
  (...)
@@
@@
- SDL_RenderGetLogicalSize
+ SDL_GetRenderLogicalSize
  (...)
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
@@
- SDL_RenderSetIntegerScale
+ SDL_SetRenderIntegerScale
  (...)
@@
@@
- SDL_RenderSetLogicalSize
+ SDL_SetRenderLogicalSize
  (...)
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
+ SDL_RW_SEEK_CUR
@@
@@
- RW_SEEK_END
+ SDL_RW_SEEK_END
@@
@@
- RW_SEEK_SET
+ SDL_RW_SEEK_SET
@@
@@
- SDL_AllocRW
+ SDL_CreateRW
  (...)
@@
@@
- SDL_FreeRW
+ SDL_DestroyRW
  (...)
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
@@
- SDL_LowerBlitScaled
+ SDL_BlitSurfaceUncheckedScaled
  (...)
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
@@
- SDL_UpperBlitScaled
+ SDL_BlitSurfaceScaled
  (...)
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
+ SDL_GetDisplayIndexForPoint
  (...)
@@
@@
- SDL_GetRectDisplayIndex
+ SDL_GetDisplayIndexForRect
  (...)
@ depends on rule_init_noparachute @
expression e;
@@
- e | 0
+ e
