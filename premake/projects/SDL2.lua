-- Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>
--
-- This software is provided 'as-is', without any express or implied
-- warranty.  In no event will the authors be held liable for any damages
-- arising from the use of this software.
--
-- Permission is granted to anyone to use this software for any purpose,
-- including commercial applications, and to alter it and redistribute it
-- freely.
--
-- Meta-build system using premake created and maintained by
-- Benjamin Henning <b.henning@digipen.edu>

--[[
SDL2.lua

	This file provides the project definition for the entire SDL2 library, on all
	platforms supported by the meta-build system. That includes Windows, MinGW,
	Cygwin, Mac OS X, iOS, and Linux. This project is responsible for setting up
	the source trees and the complicated dependencies required to build the
	final SDL2 library. In order to simplify this process, the library is split
	into several different segments. Each segment focuses on a different
	dependency and series of configurations which are thrown into the generated
	config header file, used to build this project.
]]

SDL_project "SDL2"
	 SDL_isos "windows|mingw" -- all other bindings should be a shared library
		SDL_kind "SharedLib"
	SDL_isos "macosx|ios" -- macosx employs a static linking
		SDL_kind "StaticLib"
	-- the way premake generates project dependencies and how that affects linkage
	-- makes it difficult to use shared libraries on Linux. Cygwin has issues
	-- binding to GetProcAddress, so a static library is an easy fix.
	SDL_isos "linux|cygwin"
		SDL_kind "StaticLib"

	SDL_language "C++"
	SDL_sourcedir "../src"
	-- primary platforms
	SDL_isos "ios"
		SDL_platforms { "iOS" }
	SDL_isnotos "ios"
		SDL_platforms { "native" }
	-- additional platforms
	SDL_isos "macosx"
		SDL_platforms { "universal" }
	SDL_isos "windows|mingw"
		SDL_defines { "_WINDOWS" }

	-- Following is the dependency tree for SDL2
	-- (no SDL_os call means platform-independent)

	-- The core and minimal of the SDL2 library. This will not quite build
	-- standalone, but it's doable with a bit of tweaking to build this using the
	-- minimal configuration header. This is a good start to adding SDL support to
	-- new platforms.
	SDL_config
	{
		["SDL_AUDIO_DRIVER_DISK"] = 1,
		["SDL_AUDIO_DRIVER_DUMMY"] = 1,
		["SDL_VIDEO_DRIVER_DUMMY"] = 1
	}
	SDL_paths
	{
		"/",
		"/atomic/",
		"/audio/",
		"/audio/disk/",
		"/audio/dummy/",
		"/cpuinfo/",
		"/dynapi/",
		"/events/",
		"/file/",
		"/haptic/",
		"/joystick/",
		"/power/",
		"/render/",
		"/render/software/",
		"/stdlib/",
		"/thread/",
		"/timer/",
		"/video/",
		"/video/dummy/"
	}

	-- SDL2 on Windows
	SDL_dependency "windows"
		SDL_os "windows|mingw"
		SDL_links { "imm32", "oleaut32", "winmm", "version" }
		-- these are the links that Visual Studio includes by default
		SDL_links { "kernel32", "user32", "gdi32", "winspool",
			"comdlg32", "advapi32", "shell32", "ole32",
			"oleaut32", "uuid", "odbc32", "odbccp32" }
		SDL_config
		{
			["SDL_LOADSO_WINDOWS"] = 1,
			["SDL_THREAD_WINDOWS"] = 1,
			["SDL_TIMER_WINDOWS"] = 1,
			["SDL_VIDEO_DRIVER_WINDOWS"] = 1,
			["SDL_POWER_WINDOWS"] = 1,
			["SDL_AUDIO_DRIVER_WINMM"] = 1,
			["SDL_FILESYSTEM_WINDOWS"] = 1
		}
		SDL_paths
		{
			"/audio/winmm/",
			"/core/windows/",
			"/libm/",
			"/loadso/windows/",
			"/power/windows/",
			"/thread/windows/",
			"/timer/windows/",
			"/video/windows/",
			"/filesystem/windows/"
		}
		SDL_files
		{
			-- these files have to be specified uniquely to avoid double
			-- and incorrect linking
			"/thread/generic/SDL_syscond.c",
			"/thread/generic/SDL_sysmutex_c.h"
		}

	-- DirectX dependency
	SDL_dependency "directx"
		SDL_os "windows|mingw"
		SDL_depfunc "DirectX"
		SDL_config
		{
			["SDL_AUDIO_DRIVER_DSOUND"] = 1,
			["SDL_AUDIO_DRIVER_XAUDIO2"] = 1,
			["SDL_JOYSTICK_DINPUT"] = 1,
			["SDL_HAPTIC_DINPUT"] = 1,
			["SDL_VIDEO_RENDER_D3D"] = 1
		}
		SDL_paths
		{
			"/audio/directsound/",
			"/audio/xaudio2/",
			"/render/direct3d/",
			-- these two depend on Xinput
			"/haptic/windows/",
			"/joystick/windows/",
		}
	-- in case DirectX was not found
	SDL_dependency "notdirectx"
		SDL_os "windows|mingw"
		SDL_notdepfunc "DirectX"
		SDL_config
		{
			-- enable dummy systems (same as disabling them)
			["SDL_HAPTIC_DUMMY"] = 1,
			["SDL_JOYSTICK_DUMMY"] = 1
		}
		SDL_paths
		{
			-- since we don't have Xinput
			"/haptic/dummy/",
			"/joystick/dummy/",
		}

	-- OpenGL dependency
	SDL_dependency "opengl"
		SDL_depfunc "OpenGL"
		SDL_config
		{
			["SDL_VIDEO_OPENGL"] = 1,
			["SDL_VIDEO_RENDER_OGL"] = 1
		}
		SDL_paths { "/render/opengl/" }
	-- WGL dependency for OpenGL on Windows
	SDL_dependency "opengl-windows"
		SDL_os "windows|mingw"
		SDL_depfunc "OpenGL"
		SDL_config { ["SDL_VIDEO_OPENGL_WGL"] = 1 }
	-- GLX dependency for OpenGL on Linux
	SDL_dependency "opengl-linux"
		SDL_os "linux"
		SDL_depfunc "OpenGL"
		SDL_config { ["SDL_VIDEO_OPENGL_GLX"] = 1 }

	-- SDL2 on Mac OS X
	SDL_dependency "macosx"
		SDL_os "macosx"
		SDL_config
		{
			["SDL_AUDIO_DRIVER_COREAUDIO"] = 1,
			["SDL_JOYSTICK_IOKIT"] = 1,
			["SDL_HAPTIC_IOKIT"] = 1,
			["SDL_LOADSO_DLOPEN"] = 1,
			["SDL_THREAD_PTHREAD"] = 1,
			["SDL_THREAD_PTHREAD_RECURSIVE_MUTEX"] = 1,
			["SDL_TIMER_UNIX"] = 1,
			["SDL_VIDEO_DRIVER_COCOA"] = 1,
			["SDL_POWER_MACOSX"] = 1,
			["SDL_FILESYSTEM_COCOA"] = 1
		}
		SDL_paths
		{
			"/audio/coreaudio/",
			"/file/cocoa/",
			"/haptic/darwin/",
			"/joystick/darwin/",
			"/loadso/dlopen/",
			"/power/macosx/",
			"/render/opengl/",
			"/thread/pthread/",
			"/timer/unix/",
			"/video/cocoa/",
			"/video/x11/",
			"/filesystem/cocoa/"
		}
		SDL_links
		{
			"CoreVideo.framework",
			"AudioToolbox.framework",
			"AudioUnit.framework",
			"Cocoa.framework",
			"CoreAudio.framework",
			"IOKit.framework",
			"Carbon.framework",
			"ForceFeedback.framework",
			"CoreFoundation.framework"
		}

	-- Linux dependency: DLOpen
	SDL_dependency "linux-dlopen"
		SDL_os "linux"
		SDL_depfunc "DLOpen"
		SDL_paths { "/loadso/dlopen/" }
		SDL_config { ["SDL_LOADSO_DLOPEN"] = 1 }
	-- Linux dependency: ALSA
	SDL_dependency "linux-alsa"
		SDL_os "linux"
		SDL_depfunc "ALSA"
		SDL_paths { "/audio/alsa/" }
		SDL_config
		{
			["SDL_AUDIO_DRIVER_ALSA"] = 1,
			["SDL_AUDIO_DRIVER_ALSA_DYNAMIC"] = '"libasound.so"'
		}
	-- Linux dependency: PulseAudio
	SDL_dependency "linux-pulseaudio"
		SDL_os "linux"
		SDL_depfunc "PulseAudio"
		SDL_paths { "/audio/pulseaudio/" }
		SDL_config
		{
			["SDL_AUDIO_DRIVER_PULSEAUDIO"] = 1,
			["SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC"] = '"libpulse-simple.so"'
		}
	-- Linux dependency: ESD
	SDL_dependency "linux-esd"
		SDL_os "linux"
		SDL_depfunc "ESD"
		SDL_paths { "/audio/esd/" }
		SDL_config
		{
			["SDL_AUDIO_DRIVER_ESD"] = 1,
			["SDL_AUDIO_DRIVER_ESD_DYNAMIC"] = '"libesd.so"'
		}
	-- Linux dependency: NAS
	SDL_dependency "linux-nas"
		SDL_os "linux"
		SDL_depfunc "NAS"
		SDL_paths { "/audio/nas/" }
		SDL_config
		{
			["SDL_AUDIO_DRIVER_NAS"] = 1,
			["SDL_AUDIO_DRIVER_NAS_DYNAMIC"] = '"libaudio.so"'
		}
	-- Linux dependency: OSS
	SDL_dependency "linux-oss"
		SDL_os "linux"
		SDL_depfunc "OSS"
		SDL_paths { "/audio/dsp/" }
		SDL_config { ["SDL_AUDIO_DRIVER_OSS"] = 1 }
	-- Linux dependency: X11
	SDL_dependency "linux-x11"
		SDL_os "linux"
		SDL_depfunc "X11"
		SDL_paths { "/video/x11/" }
		SDL_config
		{
			["SDL_VIDEO_DRIVER_X11"] = 1,
			["SDL_VIDEO_DRIVER_X11_DYNAMIC"] = '"libX11.so"',
			["SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT"] = '"libXext.so"',
			["SDL_VIDEO_DRIVER_X11_DYNAMIC_XCURSOR"] = '"libXcursor.so"',
			["SDL_VIDEO_DRIVER_X11_DYNAMIC_XINERAMA"] = '"libXinerama.so"',
			["SDL_VIDEO_DRIVER_X11_DYNAMIC_XINPUT2"] = '"libXi.so"',
			["SDL_VIDEO_DRIVER_X11_DYNAMIC_XRANDR"] = '"libXrandr.so"',
			["SDL_VIDEO_DRIVER_X11_DYNAMIC_XSS"] = '"libXss.so"',
			["SDL_VIDEO_DRIVER_X11_DYNAMIC_XVIDMODE"] = '"libXxf86vm.so"',
			["SDL_VIDEO_DRIVER_X11_XCURSOR"] = 1,
			["SDL_VIDEO_DRIVER_X11_XDBE"] = 1,
			["SDL_VIDEO_DRIVER_X11_XINERAMA"] = 1,
			["SDL_VIDEO_DRIVER_X11_XINPUT2"] = 1,
			["SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH"] = 1,
			["SDL_VIDEO_DRIVER_X11_XRANDR"] = 1,
			["SDL_VIDEO_DRIVER_X11_XSCRNSAVER"] = 1,
			["SDL_VIDEO_DRIVER_X11_XSHAPE"] = 1,
			["SDL_VIDEO_DRIVER_X11_XVIDMODE"] = 1,
			["SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS"] = 1,
			["SDL_VIDEO_DRIVER_X11_CONST_PARAM_XEXTADDDISPLAY"] = 1,
			["SDL_VIDEO_DRIVER_X11_HAS_XKBKEYCODETOKEYSYM"] = 1
		}
	-- SDL2 on Linux
	SDL_dependency "linux"
		SDL_os "linux"
		SDL_depfunc "DBus"
		SDL_config
		{
			["SDL_INPUT_LINUXEV"] = 1,
			["SDL_JOYSTICK_LINUX"] = 1,
			["SDL_HAPTIC_LINUX"] = 1,
			["SDL_THREAD_PTHREAD"] = 1,
			["SDL_THREAD_PTHREAD_RECURSIVE_MUTEX"] = 1,
			["SDL_TIMER_UNIX"] = 1,
			["SDL_POWER_LINUX"] = 1,
			["SDL_FILESYSTEM_UNIX"] = 1,
		}
		SDL_paths
		{
			"/haptic/linux/",
			"/joystick/linux/",
			"/power/linux/",
			"/thread/pthread/",
			"/timer/unix/",
			"/filesystem/unix/"
		}
		SDL_links
		{
			"m",
			"pthread",
			"rt"
		}

	-- SDL2 on Cygwin (not quite working yet)
	SDL_dependency "cygwin"
		SDL_os "cygwin"
		SDL_config
		{
			['SDL_JOYSTICK_DISABLED'] = 1,
			['SDL_HAPTIC_DISABLED'] = 1,
			['SDL_LOADSO_DLOPEN'] = 1,
			['SDL_THREAD_PTHREAD'] = 1,
			['SDL_THREAD_PTHREAD_RECURSIVE_MUTEX'] = 1,
			['SDL_TIMER_UNIX'] = 1,
			['SDL_FILESYSTEM_UNIX'] = 1,
			['SDL_POWER_LINUX'] = 1
		}
		SDL_paths
		{
			"/loadso/dlopen/",
			"/power/linux/",
			"/render/opengl/",
			"/thread/pthread/",
			"/timer/unix/",
			"/filesystem/unix/",
			"/libm/"
		}

	-- SDL2 on iOS
	SDL_dependency "iphoneos"
		SDL_os "ios"
		SDL_config
		{
			["SDL_AUDIO_DRIVER_COREAUDIO"] = 1,
			["SDL_JOYSTICK_DISABLED"] = 0,
			["SDL_HAPTIC_DISABLED"] = 1,
			["SDL_LOADSO_DISABLED"] = 1,
			["SDL_THREAD_PTHREAD"] = 1,
			["SDL_THREAD_PTHREAD_RECURSIVE_MUTEX"] = 1,
			["SDL_TIMER_UNIX"] = 1,
			["SDL_VIDEO_DRIVER_UIKIT"] = 1,
			["SDL_VIDEO_OPENGL_ES"] = 1,
			["SDL_VIDEO_RENDER_OGL_ES"] = 1,
			["SDL_VIDEO_RENDER_OGL_ES2"] = 1,
			["SDL_POWER_UIKIT"] = 1,
			["SDL_IPHONE_KEYBOARD"] = 1,
			["SDL_FILESYSTEM_COCOA"] = 1
		}
		SDL_paths
		{
			"/audio/coreaudio/",
			"/file/cocoa/",
			"/joystick/iphoneos/",
			"/loadso/dlopen/",
			"/power/uikit/",
			"/render/opengles/",
			"/render/opengles2/",
			"/thread/pthread/",
			"/timer/unix/",
			"/video/uikit/",
			"/filesystem/cocoa/"
		}
		SDL_links
		{
			"$(SDKROOT)/AudioToolbox.framework",
			"$(SDKROOT)/QuartzCore.framework",
			"$(SDKROOT)/OpenGLES.framework",
			"$(SDKROOT)/CoreGraphics.framework",
			"$(SDKROOT)/UIKit.framework",
			"$(SDKROOT)/Foundation.framework",
			"$(SDKROOT)/CoreAudio.framework",
			"$(SDKROOT)/CoreMotion.framework"
		}
