// swift-tools-version: 6.4;(experimentalCGen)
// The swift-tools-version declares the minimum version of Swift required to build this package.

/*
 Simple DirectMedia Layer
 Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

import Foundation
import PackageDescription
import System


// MARK: - SDL Target Types

extension Target {
	static func sdlTestExecutable(name: String, sources: [String]? = nil, additionalDependencies: [Dependency] = [], additionalSources: [String] = [], additionalCSettings: [CSetting] = [], additionalLinkerSettings: [LinkerSetting] = []) -> Target {
		.executableTarget(
			name: name,
			dependencies: [
				.target(name: "SimpleDirectMediaLayer"),
				.target(name: "SDL3_test"),
			] + additionalDependencies,
			path: "test",
			sources: (sources ?? [name + ".c"]) + additionalSources,
			cSettings: [
				.unsafeFlags(["-include", "\(Context.packageDirectory)/swift/Sources/SimpleDirectMediaLayer/include/SDL3/SDL_revision.h"]),
				.define("HAVE_BUILD_CONFIG"),	// CMake defines it for all tests even though only used by some
				.define("HAVE_SIGNAL_H"),
				.headerSearchPath("../src/video/khronos"),
			] + additionalCSettings,
			linkerSettings: additionalLinkerSettings,
		)
	}

	static func sdlExampleExecutable(name: String, sources: [String], additionalDependencies: [Dependency] = []) -> Target {
		.executableTarget(
			name: name,
			dependencies: [
				.target(name: "SimpleDirectMediaLayer"),
			] + additionalDependencies,
			path: "examples",
			sources: sources,
		)
	}

	static func sdlTarget(name: String, dependencies: [Target.Dependency] = [], additionalExcludes: [String] = [], sources: [String], copyResources: [String]? = nil, publicHeadersPath: String? = nil, additionalCSettings: [CSetting] = [], additionalLinkerSettings: [LinkerSetting] = [], plugins: [Target.PluginUsage]? = nil) -> Target {
		let publicHeadersPath = publicHeadersPath ?? "swift/Sources/\(name)/include"
		let includedPaths = sources + (copyResources ?? [])

		return .target(
			name: name,
			dependencies: dependencies,
			path: ".",
			exclude: createExcludePaths(for: ".", keeping: includedPaths) + additionalExcludes,
			sources: sources,
			resources: copyResources?.map { .copy($0) },
			publicHeadersPath: publicHeadersPath,
			cSettings: [
				.headerSearchPath("include"),
				.headerSearchPath("swift/Sources/include/build_config"),
				.headerSearchPath("include/build_config"),
				.headerSearchPath("src"),
				.unsafeFlags(["-fno-modules"]),
				.unsafeFlags(["-idirafter", "\(Context.packageDirectory)/src/video/khronos"]),
			] + additionalCSettings + TraitDescription.allCSettings,
			linkerSettings: additionalLinkerSettings,
			plugins: plugins
		)
	}
}


// MARK: - Directory Contents and Exclude Paths

func contentsOfDirectory(path: String, relativeTo basePath: String = Context.packageDirectory, files: Bool = false, withExtensions extensions: [String]? = nil, directories: Bool = false, except: [String] = []) -> [String] {
	do {
		let path = FilePath(path)
		let basePath = FilePath(basePath)

		precondition(path.isRelative, "\"\(path)\" is not a relative path")
		precondition(basePath.isAbsolute, "\"\(basePath)\" is not an absolute path")

		guard let fullPath = basePath.lexicallyResolving(path) else {
			fatalError("\"\(path)\" attempts to escape basePath")
		}
		guard let fullPathURL = URL(filePath: fullPath) else {
			fatalError("Creating URL for full path \"\(fullPath)\" failed.")
		}
		let urls = try FileManager.default.contentsOfDirectory(at: fullPathURL, includingPropertiesForKeys: [.isDirectoryKey], options: .skipsHiddenFiles)

		return try urls
			.filter {
				!except.contains($0.lastPathComponent)
			}
			.filter {
				if try $0.resourceValues(forKeys: [.isDirectoryKey]).isDirectory! {
					directories
				} else {
					files && (extensions?.contains($0.pathExtension) ?? true)
				}
			}
			.map {
				guard var filePath = FilePath($0) else {
					fatalError("Creating FilePath for URL \"\($0)\" failed.")
				}
				guard filePath.removePrefix(basePath) else {
					fatalError("Unable to remove prefix \"\(basePath)\" from \"\(filePath)\"")
				}

				return filePath.string
			}
	} catch {
		fatalError(error.localizedDescription)
	}
}

func createExcludePaths(for directoryPath: String, relativeTo basePath: String = Context.packageDirectory, keeping paths: [String] = []) -> [String] {
	do {
		let directoryPath = FilePath(directoryPath)
		let basePath = FilePath(basePath)
		let paths = paths.map { FilePath($0) }

		precondition(directoryPath.isRelative, "\"\(directoryPath)\" is not a relative path")
		precondition(basePath.isAbsolute, "\"\(basePath)\" is not an absolute path")
		precondition(paths.allSatisfy { $0.isRelative }, "paths contains non relative paths")

		guard let fullDirectoryPath = basePath.lexicallyResolving(directoryPath) else {
			fatalError("\"\(directoryPath)\" attempts to escape basePath")
		}
		guard let fullDirectoryPathURL = URL(filePath: fullDirectoryPath) else {
			fatalError("Creating URL for full path \"\(fullDirectoryPath)\" failed.")
		}
		guard try fullDirectoryPathURL.resourceValues(forKeys: [.isDirectoryKey]).isDirectory! else {
			fatalError("\"\(fullDirectoryPathURL)\" is not a directory")
		}
		let fullPaths = paths.map {
			guard let path = fullDirectoryPath.lexicallyResolving($0) else {
				fatalError("\"\($0)\" attempts to escape \"\(fullDirectoryPath)\"")
			}

			return path
		}
		guard let enumerator = FileManager.default.enumerator(at: fullDirectoryPathURL, includingPropertiesForKeys: [.isDirectoryKey], options: .skipsHiddenFiles, errorHandler: { url, error in
			fatalError("Error \(error.localizedDescription) while enumerating \"\(url)\"")
		}) else {
			fatalError("Unable to get enumerator of directory at \"\(fullDirectoryPathURL)\"")
		}
		var filePaths: [FilePath] = []

		for case let url as URL in enumerator {
			let isDirectory = try url.resourceValues(forKeys: [.isDirectoryKey]).isDirectory!
			guard var urlPath = FilePath(url) else {
				fatalError("Creating FilePath for URL \"\(url)\" failed.")
			}

			if fullPaths.contains(where: { $0 == urlPath }) {
				if isDirectory {
					enumerator.skipDescendants()
				}
			} else if !fullPaths.contains(where: { $0.starts(with: urlPath) }) {
				if isDirectory {
					enumerator.skipDescendants()
				}
				guard urlPath.removePrefix(fullDirectoryPath) else {
					fatalError("Unable to remove \"\(fullDirectoryPath)\" from start of \"\(urlPath)\"")
				}
				filePaths.append(urlPath)
			}
		}

		return filePaths.map(\.string)
	} catch {
		fatalError(error.localizedDescription)
	}
}


// MARK: - TraitDescriptions

struct TraitDescription {
	let name: String
	let description: String
	let enabledTraits: Set<String>
	let isDefault: Bool
	let cSettingDefines: [CSettingDefine]

	var cSettings: [CSetting] {
		cSettingDefines.map { .define($0.name, to: $0.value, .when(platforms: $0.condition?.platforms, configuration: $0.condition?.configuration, traits: [name]))}
	}

	init(name: String, description: String, enabledTraits: Set<String> = [], isDefault: Bool = false, cSettingDefines: [CSettingDefine]) {
		self.name = name
		self.description = description
		self.enabledTraits = enabledTraits
		self.isDefault = isDefault
		self.cSettingDefines = cSettingDefines
	}

	static let allTraitDescriptions = [
		enableDefaultAudio,
		enableAudio,
		enableAudioDriverCoreAudio,
		enableAudioDriverDisk,
		enableAudioDriverDummy,

		enableDefaultCamera,
		enableCamera,
		enableCameraDriverCoreMedia,
		enableCameraDriverDummy,

		enableDefaultDialog,
		enableDialog,

		enableDefaultGPU,
		enableGPU,
		enableGPUOpenXR,

		enableDefaultHaptic,
		enableHaptic,

		enableDefaultHIDAPI,
		enableHIDAPI,
		enableHIDAPILibUSB,
		enableHIDAPILibUSBShared,

		enableDefaultJoystick,
		enableJoystick,
		enableJoystickDriverDummy,
		enableJoystickDriverHIDAPI,
		enableJoystickDriverIOKit,
		enableJoystickDriverMFI,
		enableJoystickDriverVirtual,

		enableDefaultNotification,
		enableNotification,

		enableDefaultPower,
		enablePower,

		enableDefaultRender,
		enableRender,
		enableRenderGPU,
		enableRenderMetal,
		enableRenderVulkan,

		enableDefaultSensor,
		enableSensor,

		enableDefaultTray,
		enableTray,

		enableDefaultVideo,
		enableVideo,
		enableVideoDriverCocoa,
		enableVideoDriverDummy,
		enableVideoDriverOffscreen,
		enableVideoOpenGL,
		enableVideoOpenGLES,
		enableVideoMetal,
		enableVideoVulkan,

		enableLeanAndMean,
		enableSteamStorage,
		enableTestFFmpeg,
	]

	static var allTraits: [Trait] {
		allTraitDescriptions.map { .trait(name: $0.name, description: $0.description, enabledTraits: $0.enabledTraits) }
	}

	static var defaultEnabledTraits: Trait {
		.default(
			enabledTraits: allTraitDescriptions.reduce(into: Set<String>()) { partialResult, traitDescription in
				if traitDescription.isDefault {
					partialResult.insert(traitDescription.name)
				}
			}
		)
	}

	static var allCSettings: [CSetting] {
		allTraitDescriptions.reduce(into: []) { partialResult, traitDescription in
			partialResult.append(contentsOf: traitDescription.cSettings)
		}
	}

	struct CSettingDefine {
		struct Condition {
			let platforms: [Platform]?
			let configuration: BuildConfiguration?

			static func when(platforms: [Platform]) -> Self {
				.init(platforms: platforms, configuration: nil)
			}

			static func when(configuration: BuildConfiguration) -> Self {
				.init(platforms: nil, configuration: configuration)
			}

			static func when(platforms: [Platform], configuration: BuildConfiguration) -> Self {
				.init(platforms: platforms, configuration: configuration)
			}
		}

		let name: String
		let value: String?
		let condition: Condition?

		static func define(_ name: String, to value: String? = nil, _ condition: Condition? = nil) -> CSettingDefine {
			.init(name: name, value: value, condition: condition)
		}


		// Audio Subsystem Defines
		static let sdlSwiftPMAudioEnabled = "SDL_SWIFTPM_AUDIO_ENABLED"
		static let sdlSwiftPMAudioDriverCoreAudioEnabled = "SDL_SWIFTPM_AUDIO_DRIVER_COREAUDIO_ENABLED"
		static let sdlSwiftPMAudioDriverDiskEnabled = "SDL_SWIFTPM_AUDIO_DRIVER_DISK_ENABLED"
		static let sdlSwiftPMAudioDriverDummyEnabled = "SDL_SWIFTPM_AUDIO_DRIVER_DUMMY_ENABLED"

		// Camera Subsystem Defines
		static let sdlSwiftPMCameraEnabled = "SDL_SWIFTPM_CAMERA_ENABLED"
		static let sdlSwiftPMCameraDriverCoreMediaEnabled = "SDL_SWIFTPM_CAMERA_DRIVER_COREMEDIA_ENABLED"
		static let sdlSwiftPMCameraDriverDummyEnabled = "SDL_SWIFTPM_CAMERA_DRIVER_DUMMY_ENABLED"

		// Dialog Subsystem Defines
		static let sdlSwiftPMDialogEnabled = "SDL_SWIFTPM_DIALOG_ENABLED"

		// GPU Subsystem Defines
		static let sdlSwiftPMGPUEnabled = "SDL_SWIFTPM_GPU_ENABLED"
		static let sdlSwiftPMGPUOpenXREnabled = "SDL_SWIFTPM_GPU_OPENXR_ENABLED"

		// Haptic Subsystem Defines
		static let sdlSwiftPMHapticEnabled = "SDL_SWIFTPM_HAPTIC_ENABLED"

		// HIDAPI Subsystem Defines
		static let sdlSwiftPMHIDAPIEnabled = "SDL_SWIFTPM_HIDAPI_ENABLED"
		static let sdlSwiftPMHIDAPILibUSBEnabled = "SDL_SWIFTPM_HIDAPI_LIBUSB_ENABLED"
		static let sdlSwiftPMHIDAPILibUSBSharedEnabled = "SDL_SWIFTPM_HIDAPI_LIBUSB_SHARED_ENABLED"

		// Joystick Subsystem Defines
		static let sdlSwiftPMJoystickEnabled = "SDL_SWIFTPM_JOYSTICK_ENABLED"
		static let sdlSwiftPMJoystickDriverDummyEnabled = "SDL_SWIFTPM_JOYSTICK_DRIVER_DUMMY_ENABLED"
		static let sdlSwiftPMJoystickDriverHIDAPIEnabled = "SDL_SWIFTPM_JOYSTICK_DRIVER_HIDAPI_ENABLED"
		static let sdlSwiftPMJoystickDriverIOKitEnabled = "SDL_SWIFTPM_JOYSTICK_DRIVER_IOKIT_ENABLED"
		static let sdlSwiftPMJoystickDriverMFIEnabled = "SDL_SWIFTPM_JOYSTICK_DRIVER_MFI_ENABLED"
		static let sdlSwiftPMJoystickDriverVirtualEnabled = "SDL_SWIFTPM_JOYSTICK_DRIVER_VIRTUAL_ENABLED"

		// Notification Subsystem Defines
		static let sdlSwiftPMNotificationEnabled = "SDL_SWIFTPM_NOTIFICATION_ENABLED"

		// Power Subsystem Defines
		static let sdlSwiftPMPowerEnabled = "SDL_SWIFTPM_POWER_ENABLED"

		// Render Subsystem Defines
		static let sdlSwiftPMRenderEnabled = "SDL_SWIFTPM_RENDER_ENABLED"
		static let sdlSwiftPMRenderGPUEnabled = "SDL_SWIFTPM_RENDER_GPU_ENABLED"
		static let sdlSwiftPMRenderMetalEnabled = "SDL_SWIFTPM_RENDER_METAL_ENABLED"
		static let sdlSwiftPMRenderVulkanEnabled = "SDL_SWIFTPM_RENDER_VULKAN_ENABLED"

		// Sensor Subsystem Defines
		static let sdlSwiftPMSensorEnabled = "SDL_SWIFTPM_SENSOR_ENABLED"

		// Tray Subsystem Defines
		static let sdlSwiftPMTrayEnabled = "SDL_SWIFTPM_TRAY_ENABLED"

		// Video Subsystem Defines
		static let sdlSwiftPMVideoEnabled = "SDL_SWIFTPM_VIDEO_ENABLED"
		static let sdlSwiftPMVideoDriverCocoaEnabled = "SDL_SWIFTPM_VIDEO_DRIVER_COCOA_ENABLED"
		static let sdlSwiftPMVideoDriverDummyEnabled = "SDL_SWIFTPM_VIDEO_DRIVER_DUMMY_ENABLED"
		static let sdlSwiftPMVideoDriverOffscreenEnabled = "SDL_SWIFTPM_VIDEO_DRIVER_OFFSCREEN_ENABLED"
		static let sdlSwiftPMVideoMetalEnabled = "SDL_SWIFTPM_VIDEO_METAL_ENABLED"
		static let sdlSwiftPMVideoOpenGLEnabled = "SDL_SWIFTPM_VIDEO_OPENGL_ENABLED"
		static let sdlSwiftPMVideoOpenGLESEnabled = "SDL_SWIFTPM_VIDEO_OPENGLES_ENABLED"
		static let sdlSwiftPMVideoVulkanEnabled = "SDL_SWIFTPM_VIDEO_VULKAN_ENABLED"

		// Additional Defines
		static let sdlSwiftPMLeanAndMeanEnabled = "SDL_SWIFTPM_LEAN_AND_MEAN_ENABLED"
		static let sdlSwiftPMSteamStorageEnabled = "SDL_SWIFTPM_STEAM_STORAGE_ENABLED"
		static let sdlSwiftPMTestFFmpegEnabled = "SDL_SWIFTPM_TEST_FFMPEG_ENABLED"
	}


	// Audio Subsystem Traits

	static let enableDefaultAudio = TraitDescription(
		name: "EnableDefaultAudio",
		description: "Enable the audio subsystem and each platform's default audio drivers (CMake: default option values).",
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMAudioEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMAudioDriverCoreAudioEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMAudioDriverDiskEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMAudioDriverDummyEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
		]
	)
	static let enableAudio = TraitDescription(
		name: "EnableAudio",
		description: "Enable the audio subsystem (CMake: SDL_AUDIO=ON).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMAudioEnabled)]
	)
	static let enableAudioDriverCoreAudio = TraitDescription(
		name: "EnableAudioDriverCoreAudio",
		description: "Enable the CoreAudio driver for the audio subsystem (CMake: no separate option).",
		enabledTraits: [enableAudio.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMAudioDriverCoreAudioEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS]))],
	)
	static let enableAudioDriverDisk = TraitDescription(
		name: "EnableAudioDriverDisk",
		description: "Enable the disk writer driver for the audio subsystem (CMake: SDL_DISKAUDIO=ON).",
		enabledTraits: [enableAudio.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMAudioDriverDiskEnabled)],
	)
	static let enableAudioDriverDummy = TraitDescription(
		name: "EnableAudioDriverDummy",
		description: "Enable the dummy driver for the audio subsystem (CMake: SDL_DUMMYAUDIO=ON).",
		enabledTraits: [enableAudio.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMAudioDriverDummyEnabled)],
	)


	// Camera Subsystem Traits

	static let enableDefaultCamera = TraitDescription(
		name: "EnableDefaultCamera",
		description: "Enable the camera subsystem and each platform's default camera drivers (CMake: default option values).",
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMCameraEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMCameraDriverCoreMediaEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS])),
			.define(CSettingDefine.sdlSwiftPMCameraDriverDummyEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
		]
	)
	static let enableCamera = TraitDescription(
		name: "EnableCamera",
		description: "Enable the camera subsystem (CMake: SDL_CAMERA=ON).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMCameraEnabled)]
	)
	static let enableCameraDriverCoreMedia = TraitDescription(
		name: "EnableCameraDriverCoreMedia",
		description: "Enable the CoreMedia driver for the camera subsystem (CMake: no separate option).",
		enabledTraits: [enableCamera.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMCameraDriverCoreMediaEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS]))],
	)
	static let enableCameraDriverDummy = TraitDescription(
		name: "EnableCameraDriverDummy",
		description: "Enable the dummy driver for the camera subsystem (CMake: SDL_DUMMYCAMERA=ON).",
		enabledTraits: [enableCamera.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMCameraDriverDummyEnabled)],
	)


	// Dialog Subsystem Traits

	static let enableDefaultDialog = TraitDescription(
		name: "EnableDefaultDialog",
		description: "Enable the dialog subsystem with each platform's default backend (CMake: default option values).",
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMDialogEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
		]
	)
	static let enableDialog = TraitDescription(
		name: "EnableDialog",
		description: "Enable the dialog subsystem (CMake: SDL_DIALOG=ON).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMDialogEnabled)]
	)


	// GPU Subsystem Traits

	static let enableDefaultGPU = TraitDescription(
		name: "EnableDefaultGPU",
		description: "Enable the GPU subsystem and each platform's default GPU drivers (CMake: default option values).",
		enabledTraits: [enableDefaultVideo.name],
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMGPUEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
		]
	)
	static let enableGPU = TraitDescription(
		name: "EnableGPU",
		description: "Enable the GPU subsystem (CMake: SDL_GPU=ON).",
		enabledTraits: [enableVideo.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMGPUEnabled)]
	)
	static let enableGPUOpenXR = TraitDescription(
		name: "EnableGPUOpenXR",
		description: "Enable OpenXR support for the GPU subsystem (CMake: SDL_GPU_OPENXR=ON).",
		enabledTraits: [enableGPU.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMGPUOpenXREnabled)],
	)


	// Haptic Subsystem Traits

	static let enableDefaultHaptic = TraitDescription(
		name: "EnableDefaultHaptic",
		description: "Enable the haptic subsystem with each platform's default backend (CMake: default option values).",
		enabledTraits: [enableDefaultJoystick.name],
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMHapticEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
		]
	)
	static let enableHaptic = TraitDescription(
		name: "EnableHaptic",
		description: "Enable the haptic subsystem (CMake: SDL_HAPTIC=ON).",
		enabledTraits: [enableJoystick.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMHapticEnabled)]
	)


	// HIDAPI Subsystem Traits

	static let enableDefaultHIDAPI = TraitDescription(
		name: "EnableDefaultHIDAPI",
		description: "Enable the HIDAPI subsystem on each platform where it is on by default (CMake: default option values).",
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMHIDAPIEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS])),
		]
	)
	static let enableHIDAPI = TraitDescription(
		name: "EnableHIDAPI",
		description: "Enable the HIDAPI subsystem (CMake: SDL_HIDAPI=ON).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMHIDAPIEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS]))]
	)
	static let enableHIDAPILibUSB = TraitDescription(
		name: "EnableHIDAPILibUSB",
		description: "Link libusb at build time for low level joystick drivers (CMake: SDL_HIDAPI_LIBUSB=ON, SDL_HIDAPI_LIBUSB_SHARED=OFF).",
		enabledTraits: [enableHIDAPI.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMHIDAPILibUSBEnabled, .when(platforms: [.macOS]))]
	)
	static let enableHIDAPILibUSBShared = TraitDescription(
		name: "EnableHIDAPILibUSBShared",
		description: "Dynamically load libusb at runtime for low level joystick drivers (CMake: SDL_HIDAPI_LIBUSB=ON, SDL_HIDAPI_LIBUSB_SHARED=ON).",
		enabledTraits: [enableHIDAPI.name],
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMHIDAPILibUSBEnabled, .when(platforms: [.macOS])),
			.define(CSettingDefine.sdlSwiftPMHIDAPILibUSBSharedEnabled, .when(platforms: [.macOS])),
		]
	)


	// Joystick Subsystem Traits

	static let enableDefaultJoystick = TraitDescription(
		name: "EnableDefaultJoystick",
		description: "Enable the joystick subsystem and each platform's default joystick drivers (CMake: default option values).",
		enabledTraits: [enableDefaultHIDAPI.name],
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMJoystickEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMJoystickDriverHIDAPIEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS])),
			.define(CSettingDefine.sdlSwiftPMJoystickDriverIOKitEnabled, .when(platforms: [.macOS])),
			.define(CSettingDefine.sdlSwiftPMJoystickDriverMFIEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMJoystickDriverVirtualEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
		]
	)
	static let enableJoystick = TraitDescription(
		name: "EnableJoystick",
		description: "Enable the joystick subsystem (CMake: SDL_JOYSTICK=ON).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMJoystickEnabled)]
	)
	static let enableJoystickDriverDummy = TraitDescription(
		name: "EnableJoystickDriverDummy",
		description: "Enable the dummy driver for the joystick subsystem (CMake: no separate option).",
		enabledTraits: [enableJoystick.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMJoystickDriverDummyEnabled)],
	)
	static let enableJoystickDriverHIDAPI = TraitDescription(
		name: "EnableJoystickDriverHIDAPI",
		description: "Enable the HIDAPI driver for the joystick subsystem (CMake: SDL_HIDAPI_JOYSTICK=ON).",
		enabledTraits: [enableHIDAPI.name, enableJoystick.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMJoystickDriverHIDAPIEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS]))],
	)
	static let enableJoystickDriverIOKit = TraitDescription(
		name: "EnableJoystickDriverIOKit",
		description: "Enable the IOKit driver for the joystick subsystem (CMake: no separate option).",
		enabledTraits: [enableJoystick.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMJoystickDriverIOKitEnabled, .when(platforms: [.macOS]))],
	)
	static let enableJoystickDriverMFI = TraitDescription(
		name: "EnableJoystickDriverMFI",
		description: "Enable the GameController (MFI) driver for the joystick subsystem (CMake: no separate option).",
		enabledTraits: [enableJoystick.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMJoystickDriverMFIEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS]))],
	)
	static let enableJoystickDriverVirtual = TraitDescription(
		name: "EnableJoystickDriverVirtual",
		description: "Enable the virtual driver for the joystick subsystem (CMake: SDL_VIRTUAL_JOYSTICK=ON).",
		enabledTraits: [enableJoystick.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMJoystickDriverVirtualEnabled)],
	)


	// Notification Subsystem Traits

	static let enableDefaultNotification = TraitDescription(
		name: "EnableDefaultNotification",
		description: "Enable the notification subsystem with each platform's default backend (CMake: default option values).",
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMNotificationEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
		]
	)
	static let enableNotification = TraitDescription(
		name: "EnableNotification",
		description: "Enable the notification subsystem (CMake: SDL_NOTIFICATION=ON).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMNotificationEnabled)]
	)


	// Power Subsystem Traits

	static let enableDefaultPower = TraitDescription(
		name: "EnableDefaultPower",
		description: "Enable the power subsystem with each platform's default backend (CMake: default option values).",
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMPowerEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
		]
	)
	static let enablePower = TraitDescription(
		name: "EnablePower",
		description: "Enable the power subsystem (CMake: SDL_POWER=ON).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMPowerEnabled)]
	)


	// Render Subsystem Traits

	static let enableDefaultRender = TraitDescription(
		name: "EnableDefaultRender",
		description: "Enable the render subsystem and each platform's default render drivers (CMake: default option values).",
		enabledTraits: [enableDefaultGPU.name, enableDefaultVideo.name],
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMRenderEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMRenderGPUEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMRenderMetalEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
		]
	)
	static let enableRender = TraitDescription(
		name: "EnableRender",
		description: "Enable the render subsystem (CMake: SDL_RENDER=ON).",
		enabledTraits: [enableVideo.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMRenderEnabled)]
	)
	static let enableRenderGPU = TraitDescription(
		name: "EnableRenderGPU",
		description: "Enable the GPU driver for the render subsystem (CMake: SDL_RENDER_GPU=ON).",
		enabledTraits: [enableRender.name, enableGPU.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMRenderGPUEnabled)]
	)
	static let enableRenderMetal = TraitDescription(
		name: "EnableRenderMetal",
		description: "Enable the Metal driver for the render subsystem (CMake: SDL_RENDER_METAL=ON).",
		enabledTraits: [enableRender.name, enableVideoMetal.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMRenderMetalEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS]))]
	)
	static let enableRenderVulkan = TraitDescription(
		name: "EnableRenderVulkan",
		description: "Enable the Vulkan driver for the render subsystem (CMake: SDL_RENDER_VULKAN=ON).",
		enabledTraits: [enableRender.name, enableVideoVulkan.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMRenderVulkanEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS]))]
	)


	// Sensor Subsystem Traits

	static let enableDefaultSensor = TraitDescription(
		name: "EnableDefaultSensor",
		description: "Enable the sensor subsystem with each platform's default backend (CMake: default option values).",
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMSensorEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
		]
	)
	static let enableSensor = TraitDescription(
		name: "EnableSensor",
		description: "Enable the sensor subsystem (CMake: SDL_SENSOR=ON).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMSensorEnabled)]
	)


	// Tray Subsystem Traits

	static let enableDefaultTray = TraitDescription(
		name: "EnableDefaultTray",
		description: "Enable the tray subsystem with each platform's default backend (CMake: default option values).",
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMTrayEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
		]
	)
	static let enableTray = TraitDescription(
		name: "EnableTray",
		description: "Enable the tray subsystem (CMake: SDL_TRAY=ON).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMTrayEnabled)]
	)


	// Video Subsystem Traits

	static let enableDefaultVideo = TraitDescription(
		name: "EnableDefaultVideo",
		description: "Enable the video subsystem and each platform's default video drivers (CMake: default option values).",
		isDefault: true,
		cSettingDefines: [
			.define(CSettingDefine.sdlSwiftPMVideoEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMVideoDriverCocoaEnabled, .when(platforms: [.macOS])),
			.define(CSettingDefine.sdlSwiftPMVideoDriverDummyEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMVideoDriverOffscreenEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMVideoMetalEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			.define(CSettingDefine.sdlSwiftPMVideoOpenGLEnabled, .when(platforms: [.macOS])),
			.define(CSettingDefine.sdlSwiftPMVideoOpenGLESEnabled, .when(platforms: [.iOS, .tvOS])),
		]
	)
	static let enableVideo = TraitDescription(
		name: "EnableVideo",
		description: "Enable the video subsystem (CMake: SDL_VIDEO=ON).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMVideoEnabled)]
	)
	static let enableVideoDriverCocoa = TraitDescription(
		name: "EnableVideoDriverCocoa",
		description: "Enable the Cocoa driver for the video subsystem (CMake: SDL_COCOA=ON).",
		enabledTraits: [enableVideo.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMVideoDriverCocoaEnabled, .when(platforms: [.macOS]))]
	)
	static let enableVideoDriverDummy = TraitDescription(
		name: "EnableVideoDriverDummy",
		description: "Enable the dummy driver for the video subsystem (CMake: SDL_DUMMYVIDEO=ON).",
		enabledTraits: [enableVideo.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMVideoDriverDummyEnabled)],
	)
	static let enableVideoDriverOffscreen = TraitDescription(
		name: "EnableVideoDriverOffscreen",
		description: "Enable the offscreen driver for the video subsystem (CMake: SDL_OFFSCREEN=ON).",
		enabledTraits: [enableVideo.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMVideoDriverOffscreenEnabled)],
	)
	static let enableVideoMetal = TraitDescription(
		name: "EnableVideoMetal",
		description: "Enable Metal support for the video subsystem (CMake: SDL_METAL=ON).",
		enabledTraits: [enableVideo.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMVideoMetalEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS]))],
	)
	static let enableVideoOpenGL = TraitDescription(
		name: "EnableVideoOpenGL",
		description: "Enable OpenGL support for the video subsystem (CMake: SDL_OPENGL=ON).",
		enabledTraits: [enableVideo.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMVideoOpenGLEnabled, .when(platforms: [.macOS]))],
	)
	static let enableVideoOpenGLES = TraitDescription(
		name: "EnableVideoOpenGLES",
		description: "Enable OpenGL ES support for the video subsystem (CMake: SDL_OPENGLES=ON).",
		enabledTraits: [enableVideo.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMVideoOpenGLESEnabled, .when(platforms: [.iOS, .macOS, .tvOS]))],
	)
	static let enableVideoVulkan = TraitDescription(
		name: "EnableVideoVulkan",
		description: "Enable Vulkan support for the video subsystem (CMake: SDL_VULKAN=ON).",
		enabledTraits: [enableVideo.name],
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMVideoVulkanEnabled, .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS]))],
	)


	// Additional Traits

	static let enableLeanAndMean = TraitDescription(
		name: "EnableLeanAndMean",
		description: "Build a lean SDL library with reduced graphics functionality (CMake: SDL_LEAN_AND_MEAN=ON).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMLeanAndMeanEnabled)],
	)
	static let enableSteamStorage = TraitDescription(
		name: "EnableSteamStorage",
		description: "Enable the Steam user storage backend (CMake: no separate option).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMSteamStorageEnabled, .when(platforms: [.macOS]))],
	)
	static let enableTestFFmpeg = TraitDescription(
		name: "EnableTestFFmpeg",
		description: "Enable the FFmpeg test which requires FFmpeg 5.1 or greater to build and link against (CMake: no separate option).",
		cSettingDefines: [.define(CSettingDefine.sdlSwiftPMTestFFmpegEnabled)],
	)
}


// MARK: - Other CSettings

// Matches CMake's BUILD_DEPENDENT for tests
let buildDependentCSettings: [CSetting] = [
	.headerSearchPath("../src"),
	.headerSearchPath("../swift/Sources/include/build_config"),
	.unsafeFlags(["-idirafter", "\(Context.packageDirectory)/include/build_config"]),
] + TraitDescription.allCSettings


// MARK: - Package

let package = Package(
	name: "SimpleDirectMediaLayer",
	platforms: [
		.iOS(.v15),
		.macCatalyst(.v15),
		.macOS(.v13),
		.tvOS(.v15),
		.visionOS(.v26),
		.watchOS("99.0"),
	],
	products: [
		.library(name: "SimpleDirectMediaLayer", targets: ["SimpleDirectMediaLayer"]),
		.library(name: "SimpleDirectMediaLayerStatic", type: .static, targets: ["SimpleDirectMediaLayer"]),
		.library(name: "SimpleDirectMediaLayerDynamic", type: .dynamic, targets: ["SimpleDirectMediaLayerDynamic"]),
		.library(name: "SDL3_test", type: .static, targets: ["SDL3_test"]),
	],
	traits: Set(TraitDescription.allTraits + [TraitDescription.defaultEnabledTraits]),
	dependencies: [
		.package(url: "https://github.com/swiftlang/swift-subprocess", from: "1.0.0"),
		.package(url: "https://github.com/apple/swift-system", from: "1.8.1"),
	],
	targets: [


		// MARK: - Public Libraries

		.sdlTarget(
			name: "SimpleDirectMediaLayer",
			dependencies: [
				// A hack to build the revision header first

				.target(name: "revision_header"),


				// Platform Dependencies

				.target(name: "iOS", condition: .when(platforms: [.iOS])),
				.target(name: "macCatalyst", condition: .when(platforms: [.macCatalyst])),
				.target(name: "macOS", condition: .when(platforms: [.macOS])),
				.target(name: "tvOS", condition: .when(platforms: [.tvOS])),
				.target(name: "visionOS", condition: .when(platforms: [.visionOS])),


				// Subsystem Dependencies

				.target(name: "audio", condition: .when(traits: [TraitDescription.enableAudio.name])),
				.target(name: "camera", condition: .when(traits: [TraitDescription.enableCamera.name])),
				.target(name: "dialog", condition: .when(traits: [TraitDescription.enableDialog.name])),
				.target(name: "gpu", condition: .when(traits: [TraitDescription.enableGPU.name])),
				.target(name: "haptic", condition: .when(traits: [TraitDescription.enableHaptic.name])),
				.target(name: "hidapi", condition: .when(traits: [TraitDescription.enableHIDAPI.name])),
				.target(name: "joystick", condition: .when(traits: [TraitDescription.enableJoystick.name])),
				.target(name: "notification", condition: .when(traits: [TraitDescription.enableNotification.name])),
				.target(name: "power", condition: .when(traits: [TraitDescription.enablePower.name])),
				.target(name: "render", condition: .when(traits: [TraitDescription.enableRender.name])),
				.target(name: "sensor", condition: .when(traits: [TraitDescription.enableSensor.name])),
				.target(name: "tray", condition: .when(traits: [TraitDescription.enableTray.name])),
				.target(name: "video", condition: .when(traits: [TraitDescription.enableVideo.name])),


				// Additional Dependencies

				.target(name: "storage_steam", condition: .when(platforms: [.macOS], traits: [TraitDescription.enableSteamStorage.name])),

			],
			additionalExcludes:
				contentsOfDirectory(path: "src/atomic", directories: true)
				+ contentsOfDirectory(path: "src/audio", directories: true)
				+ contentsOfDirectory(path: "src/camera", directories: true)
				+ contentsOfDirectory(path: "src/core", directories: true)
				+ contentsOfDirectory(path: "src/cpuinfo", directories: true)
				+ contentsOfDirectory(path: "src/dialog", directories: true, except: ["dummy"])
				+ contentsOfDirectory(path: "src/dynapi", files: true, withExtensions: ["exports", "sym", "py"], directories: true)
				+ contentsOfDirectory(path: "src/events", directories: true)
				+ contentsOfDirectory(path: "src/filesystem", directories: true)
				+ contentsOfDirectory(path: "src/gpu", directories: true, except: ["xr"])
				+ contentsOfDirectory(path: "src/haptic", directories: true, except: ["dummy"])
				+ contentsOfDirectory(path: "src/hidapi", files: true, withExtensions: ["txt", "md", "am", "ac", "build", ""], directories: true)
				+ contentsOfDirectory(path: "src/io", directories: true, except: ["generic"])
				+ contentsOfDirectory(path: "src/joystick", files: true, withExtensions: ["sh", "py"], directories: true, except: ["dummy"])
				+ contentsOfDirectory(path: "src/libm", directories: true)
				+ contentsOfDirectory(path: "src/loadso", directories: true)
				+ contentsOfDirectory(path: "src/locale", directories: true)
				+ contentsOfDirectory(path: "src/main", directories: true, except: ["generic"])
				+ contentsOfDirectory(path: "src/misc", directories: true)
				+ contentsOfDirectory(path: "src/notification", directories: true)
				+ contentsOfDirectory(path: "src/power", directories: true)
				+ contentsOfDirectory(path: "src/process", directories: true, except: ["dummy"])
				+ contentsOfDirectory(path: "src/render", directories: true, except: ["software"])
				+ contentsOfDirectory(path: "src/sensor", directories: true, except: ["dummy"])
				+ contentsOfDirectory(path: "src/stdlib", files: true, withExtensions: ["masm"], directories: true)
				+ contentsOfDirectory(path: "src/storage", directories: true, except: ["generic"])
				+ contentsOfDirectory(path: "src/thread", directories: true)
				+ contentsOfDirectory(path: "src/time", directories: true)
				+ contentsOfDirectory(path: "src/timer", directories: true)
				+ contentsOfDirectory(path: "src/tray", directories: true, except: ["dummy"])
				+ contentsOfDirectory(path: "src/video", files: true, withExtensions: ["pl"], directories: true, except: ["yuv2rgb"])
				+ contentsOfDirectory(path: "src/video/yuv2rgb", files: true, withExtensions: ["md", ""])
				+ [
					"src/dialog/SDL_dialog_utils.c",
					"src/test",
				],
			sources: [
				"src",
				"swift/Sources/SimpleDirectMediaLayer/src",
			],
			publicHeadersPath: "include",
			additionalCSettings: [
				.headerSearchPath("swift/Sources/SimpleDirectMediaLayer/include"),
				.unsafeFlags(["-include", "\(Context.packageDirectory)/swift/Sources/SimpleDirectMediaLayer/include/SDL3/SDL_revision.h"]),
			],
			additionalLinkerSettings: [
				.linkedLibrary("usb-1.0", .when(platforms: [.macOS], traits: [TraitDescription.enableHIDAPILibUSB.name]))
			],
		),

		// We need a wrapper target for this to use the exported symbols list properly
		.sdlTarget(
			name: "SimpleDirectMediaLayerDynamic",
			dependencies: [
				.target(name: "SimpleDirectMediaLayer")
			],
			sources: [
				"swift/Sources/SimpleDirectMediaLayerDynamic/src"
			],
			publicHeadersPath: "include",
			additionalLinkerSettings: [
				.unsafeFlags(["-Xlinker", "-exported_symbols_list", "-Xlinker", "\(Context.packageDirectory)/src/dynapi/SDL_dynapi.exports"]),
			],
		),

		// This is the SDL3_test library. It is not part of SDL, so it remains a normal target.
		.target(
			name: "SDL3_test",
			dependencies: [
				.target(name: "SimpleDirectMediaLayer")
			],
			path: ".",
			exclude: createExcludePaths(for: ".", keeping: ["src/test"]),
			sources: [
				"src/test"
			],
			publicHeadersPath: "swift/Sources/SDL3_test/include",
		),


		// MARK: - Platform Targets

		.sdlTarget(
			name: "iOS",
			dependencies: [
				.target(name: "uikit"),


				// Audio Subsystem Default Dependencies

				.target(name: "audio", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_coreaudio", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_disk", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),


				// Camera Subsystem Default Dependencies

				.target(name: "camera", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),
				.target(name: "camera_driver_coremedia", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),
				.target(name: "camera_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),

				
				// Dialog Subsystem Default Dependencies

				.target(name: "dialog", condition: .when(traits: [TraitDescription.enableDefaultDialog.name])),


				// GPU Subsystem Default Dependencies

				.target(name: "gpu", condition: .when(traits: [TraitDescription.enableDefaultGPU.name])),
				.target(name: "gpu_metal", condition: .when(traits: [TraitDescription.enableDefaultGPU.name])),


				// Haptic Subsystem Default Dependencies

				.target(name: "haptic", condition: .when(traits: [TraitDescription.enableDefaultHaptic.name])),


				// HIDAPI Subsystem Default Dependencies

				.target(name: "hidapi", condition: .when(traits: [TraitDescription.enableDefaultHIDAPI.name])),


				// Joystick Subsystem Default Dependencies

				.target(name: "joystick", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_hidapi", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_mfi", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_virtual", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),


				// Notification Subsystem Default Dependencies

				.target(name: "notification", condition: .when(traits: [TraitDescription.enableDefaultNotification.name])),


				// Power Subsystem Default Dependencies

				.target(name: "power", condition: .when(traits: [TraitDescription.enableDefaultPower.name])),


				// Render Subsystem Default Dependencies

				.target(name: "render", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_gpu", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_metal", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_opengles", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_opengles2", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),

				
				// Sensor Subsystem Default Dependencies

				.target(name: "sensor", condition: .when(traits: [TraitDescription.enableDefaultSensor.name])),


				// Tray Subsystem Default Dependencies

				.target(name: "tray", condition: .when(traits: [TraitDescription.enableDefaultTray.name])),


				// Video Subsystem Default Dependencies

				.target(name: "video", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_offscreen", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_uikit", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
			],
			sources: [
				"swift/Sources/iOS/src"
			],
		),

		.sdlTarget(
			name: "macCatalyst",
			dependencies: [
				.target(name: "uikit"),


				// Audio Subsystem Default Dependencies

				.target(name: "audio", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_coreaudio", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_disk", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),


				// Camera Subsystem Default Dependencies

				.target(name: "camera", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),
				.target(name: "camera_driver_coremedia", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),
				.target(name: "camera_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),


				// Dialog Subsystem Default Dependencies

				.target(name: "dialog", condition: .when(traits: [TraitDescription.enableDefaultDialog.name])),


				// GPU Subsystem Default Dependencies

				.target(name: "gpu", condition: .when(traits: [TraitDescription.enableDefaultGPU.name])),
				.target(name: "gpu_metal", condition: .when(traits: [TraitDescription.enableDefaultGPU.name])),


				// Haptic Subsystem Default Dependencies

				.target(name: "haptic", condition: .when(traits: [TraitDescription.enableDefaultHaptic.name])),


				// HIDAPI Subsystem Default Dependencies

				.target(name: "hidapi", condition: .when(traits: [TraitDescription.enableDefaultHIDAPI.name])),


				// Joystick Subsystem Default Dependencies

				.target(name: "joystick", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_hidapi", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_mfi", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_virtual", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),


				// Notification Subsystem Default Dependencies

				.target(name: "notification", condition: .when(traits: [TraitDescription.enableDefaultNotification.name])),


				// Power Subsystem Default Dependencies

				.target(name: "power", condition: .when(traits: [TraitDescription.enableDefaultPower.name])),


				// Render Subsystem Default Dependencies

				.target(name: "render", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_gpu", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_metal", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),


				// Sensor Subsystem Default Dependencies

				.target(name: "sensor", condition: .when(traits: [TraitDescription.enableDefaultSensor.name])),


				// Tray Subsystem Default Dependencies

				.target(name: "tray", condition: .when(traits: [TraitDescription.enableDefaultTray.name])),


				// Video Subsystem Default Dependencies

				.target(name: "video", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_offscreen", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_uikit", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
			],
			sources: [
				"swift/Sources/macCatalyst/src"
			],
		),

		.sdlTarget(
			name: "macOS",
			dependencies: [
				.target(name: "apple"),
				.target(name: "posix"),


				// Audio Subsystem Default Dependencies

				.target(name: "audio", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_coreaudio", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_disk", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),


				// Camera Subsystem Default Dependencies

				.target(name: "camera", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),
				.target(name: "camera_driver_coremedia", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),
				.target(name: "camera_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),


				// Dialog Subsystem Default Dependencies

				.target(name: "dialog", condition: .when(traits: [TraitDescription.enableDefaultDialog.name])),


				// GPU Subsystem Default Dependencies

				.target(name: "gpu", condition: .when(traits: [TraitDescription.enableDefaultGPU.name])),
				.target(name: "gpu_metal", condition: .when(traits: [TraitDescription.enableDefaultGPU.name])),


				// Haptic Subsystem Default Dependencies

				.target(name: "haptic", condition: .when(traits: [TraitDescription.enableDefaultHaptic.name])),


				// HIDAPI Subsystem Default Dependencies

				.target(name: "hidapi", condition: .when(traits: [TraitDescription.enableDefaultHIDAPI.name])),


				// Joystick Subsystem Default Dependencies

				.target(name: "joystick", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_hidapi", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_iokit", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_mfi", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_virtual", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),


				// Notification Subsystem Default Dependencies

				.target(name: "notification", condition: .when(traits: [TraitDescription.enableDefaultNotification.name])),


				// Power Subsystem Default Dependencies

				.target(name: "power", condition: .when(traits: [TraitDescription.enableDefaultPower.name])),


				// Render Subsystem Default Dependencies

				.target(name: "render", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_gpu", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_metal", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_opengl", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),


				// Sensor Subsystem Default Dependencies

				.target(name: "sensor", condition: .when(traits: [TraitDescription.enableDefaultSensor.name])),


				// Tray Subsystem Default Dependencies

				.target(name: "tray", condition: .when(traits: [TraitDescription.enableDefaultTray.name])),


				// Video Subsystem Default Dependencies

				.target(name: "video", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_cocoa", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_offscreen", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
			],
			sources: [
				"src/misc/macos",
			],
			additionalLinkerSettings: [
				.linkedFramework("CoreServices", .when(platforms: [.macOS])),
			],
		),

		.sdlTarget(
			name: "tvOS",
			dependencies: [
				.target(name: "uikit"),


				// Audio Subsystem Default Dependencies

				.target(name: "audio", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_coreaudio", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_disk", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),


				// Camera Subsystem Default Dependencies

				.target(name: "camera", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),
				.target(name: "camera_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),


				// Dialog Subsystem Default Dependencies

				.target(name: "dialog", condition: .when(traits: [TraitDescription.enableDefaultDialog.name])),


				// GPU Subsystem Default Dependencies

				.target(name: "gpu", condition: .when(traits: [TraitDescription.enableDefaultGPU.name])),
				.target(name: "gpu_metal", condition: .when(traits: [TraitDescription.enableDefaultGPU.name])),


				// Haptic Subsystem Default Dependencies

				.target(name: "haptic", condition: .when(traits: [TraitDescription.enableDefaultHaptic.name])),


				// HIDAPI Subsystem Default Dependencies

				.target(name: "hidapi", condition: .when(traits: [TraitDescription.enableDefaultHIDAPI.name])),


				// Joystick Subsystem Default Dependencies

				.target(name: "joystick", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_hidapi", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_mfi"),	// The tvOS UIKit view needs SDL_AppleTVRemoteOpenedAsJoystick even when joystick is disabled
				.target(name: "joystick_driver_virtual", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),


				// Notification Subsystem Default Dependencies

				.target(name: "notification", condition: .when(traits: [TraitDescription.enableDefaultNotification.name])),


				// Power Subsystem Default Dependencies

				.target(name: "power", condition: .when(traits: [TraitDescription.enableDefaultPower.name])),


				// Render Subsystem Default Dependencies

				.target(name: "render", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_gpu", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_metal", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_opengles", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_opengles2", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),


				// Sensor Subsystem Default Dependencies

				.target(name: "sensor", condition: .when(traits: [TraitDescription.enableDefaultSensor.name])),


				// Tray Subsystem Default Dependencies

				.target(name: "tray", condition: .when(traits: [TraitDescription.enableDefaultTray.name])),


				// Video Subsystem Default Dependencies

				.target(name: "video", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_offscreen", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_uikit", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
			],
			sources: [
				"swift/Sources/tvOS/src"
			],
		),

		.sdlTarget(
			name: "visionOS",
			dependencies: [
				.target(name: "uikit"),


				// Audio Subsystem Default Dependencies

				.target(name: "audio", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_coreaudio", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_disk", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),
				.target(name: "audio_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultAudio.name])),


				// Camera Subsystem Default Dependencies

				.target(name: "camera", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),
				.target(name: "camera_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultCamera.name])),


				// Dialog Subsystem Default Dependencies

				.target(name: "dialog", condition: .when(traits: [TraitDescription.enableDefaultDialog.name])),


				// GPU Subsystem Default Dependencies

				.target(name: "gpu", condition: .when(traits: [TraitDescription.enableDefaultGPU.name])),
				.target(name: "gpu_metal", condition: .when(traits: [TraitDescription.enableDefaultGPU.name])),


				// Haptic Subsystem Default Dependencies

				.target(name: "haptic", condition: .when(traits: [TraitDescription.enableDefaultHaptic.name])),


				// Joystick Subsystem Default Dependencies

				.target(name: "joystick", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_mfi", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),
				.target(name: "joystick_driver_virtual", condition: .when(traits: [TraitDescription.enableDefaultJoystick.name])),


				// Notification Subsystem Default Dependencies

				.target(name: "notification", condition: .when(traits: [TraitDescription.enableDefaultNotification.name])),


				// Power Subsystem Default Dependencies

				.target(name: "power", condition: .when(traits: [TraitDescription.enableDefaultPower.name])),


				// Render Subsystem Default Dependencies

				.target(name: "render", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_gpu", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),
				.target(name: "render_metal", condition: .when(traits: [TraitDescription.enableDefaultRender.name])),


				// Sensor Subsystem Default Dependencies

				.target(name: "sensor", condition: .when(traits: [TraitDescription.enableDefaultSensor.name])),


				// Tray Subsystem Default Dependencies

				.target(name: "tray", condition: .when(traits: [TraitDescription.enableDefaultTray.name])),


				// Video Subsystem Default Dependencies

				.target(name: "video", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_dummy", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_offscreen", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
				.target(name: "video_driver_uikit", condition: .when(traits: [TraitDescription.enableDefaultVideo.name])),
			],
			sources: [
				"swift/Sources/visionOS/src"
			],
		),


		// MARK: - Platform Dependency Targets

		.sdlTarget(
			name: "apple",
			sources: [
				"src/filesystem/cocoa",
				"src/locale/macos",
			],
			additionalLinkerSettings: [
				.linkedFramework("Foundation", .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			],
		),

		.sdlTarget(
			name: "posix",
			sources: [
				"src/filesystem/posix",
				"src/loadso/dlopen",
				"src/process/posix",
				"src/thread/pthread",
				"src/time/unix",
				"src/timer/unix",
			],
		),

		.sdlTarget(
			name: "uikit",
			dependencies: [
				.target(name: "apple"),
				.target(name: "posix"),

				// UIKit video driver is always linked to provide required symbols in main
				.target(name: "video_driver_uikit"),
			],
			sources: [
				"src/main/ios",
				"src/misc/ios",
			],
			additionalLinkerSettings: [
				.linkedFramework("QuartzCore", .when(platforms: [.iOS, .macCatalyst, .tvOS, .visionOS])),
				.linkedFramework("UIKit", .when(platforms: [.iOS, .macCatalyst, .tvOS, .visionOS])),
			],
		),


		// MARK: - Subsystem Targets

		// Audio Subsystem Targets

		.sdlTarget(
			name: "audio",
			dependencies: [
				.target(name: "audio_driver_coreaudio", condition: .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS], traits: [TraitDescription.enableAudioDriverCoreAudio.name])),
				.target(name: "audio_driver_disk", condition: .when(traits: [TraitDescription.enableAudioDriverDisk.name])),
				.target(name: "audio_driver_dummy", condition: .when(traits: [TraitDescription.enableAudioDriverDummy.name])),
			],
			sources: ["swift/Sources/audio/src"],
		),
		.sdlTarget(
			name: "audio_driver_coreaudio",
			sources: ["src/audio/coreaudio"],
			additionalLinkerSettings: [
				.linkedFramework("AudioToolbox", .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
				.linkedFramework("AVFoundation", .when(platforms: [.iOS, .macCatalyst, .tvOS, .visionOS])),
				.linkedFramework("CoreAudio", .when(platforms: [.macOS]))
			]
		),
		.sdlTarget(name: "audio_driver_disk", sources: ["src/audio/disk"]),
		.sdlTarget(name: "audio_driver_dummy", sources: ["src/audio/dummy"]),


		// Camera Subsystem Targets

		.sdlTarget(
			name: "camera",
			dependencies: [
				.target(name: "camera_driver_coremedia", condition: .when(platforms: [.iOS, .macCatalyst, .macOS], traits: [TraitDescription.enableCameraDriverCoreMedia.name])),
				.target(name: "camera_driver_dummy", condition: .when(traits: [TraitDescription.enableCameraDriverDummy.name])),
			],
			sources: ["swift/Sources/camera/src"],
		),
		.sdlTarget(
			name: "camera_driver_coremedia",
			sources: ["src/camera/coremedia"],
			additionalLinkerSettings: [
				.linkedFramework("AVFoundation", .when(platforms: [.iOS, .macCatalyst, .macOS])),
				.linkedFramework("CoreVideo", .when(platforms: [.iOS, .macCatalyst, .macOS])),
				.linkedFramework("CoreMedia", .when(platforms: [.iOS, .macCatalyst, .macOS]))
			]
		),
		.sdlTarget(name: "camera_driver_dummy", sources: ["src/camera/dummy"]),


		// Dialog Subsystem Targets

		.sdlTarget(
			name: "dialog",
			dependencies: [
				.target(name: "dialog_cocoa", condition: .when(platforms: [.macOS])),
			],
			sources: ["src/dialog/SDL_dialog_utils.c"],
		),
		.sdlTarget(
			name: "dialog_cocoa",
			sources: ["src/dialog/cocoa"],
			additionalLinkerSettings: [
				.linkedFramework("AppKit", .when(platforms: [.macOS])),
				.linkedFramework("UniformTypeIdentifiers", .when(platforms: [.macOS]))
			]
		),


		// GPU Subsystem Targets

		.sdlTarget(
			name: "gpu",
			dependencies: [
				.target(name: "gpu_metal", condition: .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS], traits: [TraitDescription.enableVideoMetal.name, TraitDescription.enableDefaultVideo.name])),
				.target(name: "gpu_vulkan", condition: .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS], traits: [TraitDescription.enableVideoVulkan.name])),
			],
			sources: ["swift/Sources/gpu/src"],
		),
		.sdlTarget(
			name: "gpu_metal",
			additionalExcludes: contentsOfDirectory(path: "src/gpu/metal", files: true, withExtensions: ["sh", "metal"]),
			sources: ["src/gpu/metal"],
			additionalLinkerSettings: [
				.linkedFramework("CoreGraphics", .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
				.linkedFramework("Metal", .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS]))
			]
		),
		.sdlTarget(name: "gpu_vulkan", sources: ["src/gpu/vulkan"]),


		// Haptic Subsystem Targets

		.sdlTarget(
			name: "haptic",
			dependencies: [
				.target(name: "haptic_iokit", condition: .when(platforms: [.macOS])),
			],
			sources: ["swift/Sources/haptic/src"],
		),
		.sdlTarget(name: "haptic_hidapi", sources: ["src/haptic/hidapi"]),
		.sdlTarget(
			name: "haptic_iokit",
			sources: ["src/haptic/darwin"],
			additionalLinkerSettings: [
				.linkedFramework("ForceFeedback", .when(platforms: [.macOS])),
				.linkedFramework("IOKit", .when(platforms: [.macOS])),
			]
		),


		// HIDAPI Subsystem Targets

		.sdlTarget(
			name: "hidapi",
			dependencies: [
				.target(name: "hidapi_ios", condition: .when(platforms: [.iOS, .macCatalyst, .tvOS]))
			],
			sources: ["swift/Sources/hidapi/src"],
			additionalLinkerSettings: [
				.linkedFramework("IOKit", .when(platforms: [.macOS])),
			]
		),
		.sdlTarget(
			name: "hidapi_ios",
			sources: ["src/hidapi/ios"],
			additionalLinkerSettings: [
				.linkedFramework("CoreBluetooth", .when(platforms: [.iOS, .macCatalyst, .tvOS])),
			]
		),


		// Joystick Subsystem Targets

		.sdlTarget(
			name: "joystick",
			dependencies: [
				.target(name: "joystick_driver_hidapi", condition: .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS], traits: [TraitDescription.enableJoystickDriverHIDAPI.name])),
				.target(name: "joystick_driver_iokit", condition: .when(platforms: [.macOS], traits: [TraitDescription.enableJoystickDriverIOKit.name])),
				.target(name: "joystick_driver_mfi", condition: .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS], traits: [TraitDescription.enableJoystickDriverMFI.name])),
				.target(name: "joystick_driver_virtual", condition: .when(traits: [TraitDescription.enableJoystickDriverVirtual.name])),
			],
			sources: ["swift/Sources/joystick/src"],
		),
		.sdlTarget(
			name: "joystick_driver_hidapi",
			dependencies: [.target(name: "haptic_hidapi")],
			sources: ["src/joystick/hidapi"],
			additionalLinkerSettings: [
				.linkedFramework("IOKit", .when(platforms: [.macOS])),
			]
		),
		.sdlTarget(
			name: "joystick_driver_iokit",
			sources: ["src/joystick/darwin"],
			additionalLinkerSettings: [
				.linkedFramework("ForceFeedback", .when(platforms: [.macOS])),
				.linkedFramework("IOKit", .when(platforms: [.macOS])),
			]
		),
		.sdlTarget(
			name: "joystick_driver_mfi",
			sources: ["src/joystick/apple"],
			additionalLinkerSettings: [
				.linkedFramework("CoreHaptics", .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS], traits: [TraitDescription.enableJoystickDriverMFI.name, TraitDescription.enableDefaultJoystick.name])),
				.linkedFramework("GameController", .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
				.linkedFramework("IOKit", .when(platforms: [.macOS], traits: [TraitDescription.enableJoystickDriverMFI.name, TraitDescription.enableDefaultJoystick.name])),
			]
		),
		.sdlTarget(name: "joystick_driver_virtual", sources: ["src/joystick/virtual"]),


		// Notification Subsystem Targets

		.sdlTarget(
			name: "notification",
			dependencies: [
				.target(name: "notification_cocoa", condition: .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			],
			sources: ["swift/Sources/notification/src"],
		),
		.sdlTarget(
			name: "notification_cocoa",
			sources: ["src/notification/cocoa"],
			additionalLinkerSettings: [
				.linkedFramework("Security", .when(platforms: [.iOS, .macCatalyst, .macOS, .visionOS])),
				.linkedFramework("UserNotifications", .when(platforms: [.iOS, .macCatalyst, .macOS, .visionOS])),
			]
		),


		// Power Subsystem Targets

		.sdlTarget(
			name: "power",
			dependencies: [
				.target(name: "power_macos", condition: .when(platforms: [.macOS])),
				.target(name: "power_uikit", condition: .when(platforms: [.iOS, .macCatalyst, .tvOS, .visionOS])),
			],
			sources: ["swift/Sources/power/src"],
		),
		.sdlTarget(
			name: "power_macos",
			sources: ["src/power/macos"],
			additionalLinkerSettings: [
				.linkedFramework("IOKit", .when(platforms: [.macOS])),
			]
		),
		.sdlTarget(
			name: "power_uikit",
			sources: ["src/power/uikit"],
		),


		// Render Subsystem Targets

		.sdlTarget(
			name: "render",
			dependencies: [
				.target(name: "render_gpu", condition: .when(traits: [TraitDescription.enableRenderGPU.name])),
				.target(name: "render_metal", condition: .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS], traits: [TraitDescription.enableRenderMetal.name])),
				.target(name: "render_opengl", condition: .when(platforms: [.macOS], traits: [TraitDescription.enableVideoOpenGL.name, TraitDescription.enableDefaultVideo.name])),
				.target(name: "render_opengles", condition: .when(platforms: [.iOS, .tvOS], traits: [TraitDescription.enableVideoOpenGLES.name, TraitDescription.enableDefaultVideo.name])),
				.target(name: "render_opengles2_uikit", condition: .when(platforms: [.iOS, .tvOS], traits: [TraitDescription.enableVideoOpenGLES.name, TraitDescription.enableDefaultVideo.name])),
				.target(name: "render_opengles2_mac", condition: .when(platforms: [.macOS], traits: [TraitDescription.enableVideoOpenGLES.name])),
				.target(name: "render_vulkan", condition: .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS], traits: [TraitDescription.enableRenderVulkan.name])),
			],
			sources: ["swift/Sources/render/src"],
		),
		.sdlTarget(name: "render_gpu", additionalExcludes: ["src/render/gpu/shaders"], sources: ["src/render/gpu"]),
		.sdlTarget(
			name: "render_metal",
			additionalExcludes: contentsOfDirectory(path: "src/render/metal", files: true, withExtensions: ["sh", "metal"]),
			sources: ["src/render/metal"],
			additionalLinkerSettings: [
				.linkedFramework("CoreGraphics", .when(platforms: [.iOS, .macCatalyst, .macOS, .visionOS])),
				.linkedFramework("CoreVideo", .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
				.linkedFramework("Metal", .when(platforms: [.iOS, .macCatalyst, .macOS, .tvOS, .visionOS])),
			]
		),
		.sdlTarget(name: "render_opengl", sources: ["src/render/opengl"]),
		.sdlTarget(
			name: "render_opengles",
			sources: ["src/render/opengles"],
			additionalCSettings: [
				.define("GLES_SILENCE_DEPRECATION", .when(platforms: [.iOS, .tvOS], traits: [TraitDescription.enableVideoOpenGLES.name, TraitDescription.enableDefaultVideo.name])),
			],
			additionalLinkerSettings: [
				.linkedFramework("OpenGLES", .when(platforms: [.iOS, .tvOS], traits: [TraitDescription.enableVideoOpenGLES.name, TraitDescription.enableDefaultVideo.name])),
			],
		),
		.sdlTarget(
			name: "render_opengles2",
			sources: ["src/render/opengles2"],
			additionalCSettings: [
				.define("GLES_SILENCE_DEPRECATION", .when(platforms: [.iOS, .tvOS], traits: [TraitDescription.enableVideoOpenGLES.name, TraitDescription.enableDefaultVideo.name])),
			],
			additionalLinkerSettings: [
				.linkedFramework("OpenGLES", .when(platforms: [.iOS, .tvOS], traits: [TraitDescription.enableVideoOpenGLES.name, TraitDescription.enableDefaultVideo.name])),
			],
		),
		.sdlTarget(name: "render_opengles2_uikit", dependencies: ["render_opengles2"], sources: ["swift/Sources/render_opengles2_uikit/src"]),
		.sdlTarget(name: "render_opengles2_mac", dependencies: ["render_opengles2"], sources: ["swift/Sources/render_opengles2_mac/src"]),
		.sdlTarget(name: "render_vulkan", additionalExcludes: contentsOfDirectory(path: "src/render/vulkan", files: true, withExtensions: ["bat", "hlsl", "hlsli"]), sources: ["src/render/vulkan"]),


		// Sensor Subsystem Targets

		.sdlTarget(
			name: "sensor",
			dependencies: [
				.target(name: "sensor_coremotion", condition: .when(platforms: [.iOS, .macCatalyst, .visionOS])),
			],
			sources: ["swift/Sources/sensor/src"],
		),
		.sdlTarget(
			name: "sensor_coremotion",
			sources: ["src/sensor/coremotion"],
			additionalLinkerSettings: [
				.linkedFramework("CoreMotion", .when(platforms: [.iOS, .macCatalyst, .visionOS])),
			]
		),

		
		// Tray Subsystem Targets

		.sdlTarget(
			name: "tray",
			dependencies: [
				.target(name: "tray_cocoa", condition: .when(platforms: [.macOS])),
			],
			sources: ["swift/Sources/tray/src"],
		),
		.sdlTarget(
			name: "tray_cocoa",
			sources: ["src/tray/cocoa"],
			additionalLinkerSettings: [
				.linkedFramework("AppKit", .when(platforms: [.macOS])),
			]
		),


		// Video Subsystem Targets

		.sdlTarget(
			name: "video",
			dependencies: [
				.target(name: "video_driver_cocoa", condition: .when(platforms: [.macOS], traits: [TraitDescription.enableVideoDriverCocoa.name])),
				.target(name: "video_driver_dummy", condition: .when(traits: [TraitDescription.enableVideoDriverDummy.name])),
				.target(name: "video_driver_offscreen", condition: .when(traits: [TraitDescription.enableVideoDriverOffscreen.name])),
				.target(name: "video_driver_uikit", condition: .when(platforms: [.iOS, .macCatalyst, .tvOS, .visionOS])),
			],
			sources: ["swift/Sources/video/src"],
		),
		.sdlTarget(
			name: "video_driver_cocoa",
			sources: ["src/video/cocoa"],
			additionalLinkerSettings: [
				.linkedFramework("AppKit", .when(platforms: [.macOS])),
				.linkedFramework("Carbon", .when(platforms: [.macOS])),
				.linkedFramework("CoreVideo", .when(platforms: [.macOS])),
				.linkedFramework("GameController", .when(platforms: [.macOS])),
				.linkedFramework("IOKit", .when(platforms: [.macOS])),
				.linkedFramework("QuartzCore", .when(platforms: [.macOS], traits: [TraitDescription.enableVideoVulkan.name])),
				.linkedFramework("UniformTypeIdentifiers", .when(platforms: [.macOS])),
			]
		),
		.sdlTarget(name: "video_driver_dummy", sources: ["src/video/dummy"]),
		.sdlTarget(name: "video_driver_offscreen", sources: ["src/video/offscreen"]),
		.sdlTarget(
			name: "video_driver_uikit",
			dependencies: [
				.target(name: "video_driver_uikit_swift", condition: .when(platforms: [.visionOS])),	// The visionOS curved-window UI is written in Swift
			],
			additionalExcludes: contentsOfDirectory(path: "src/video/uikit", files: true, withExtensions: ["swift"]),
			sources: ["src/video/uikit"],
			additionalCSettings: [
				.define("GLES_SILENCE_DEPRECATION", .when(platforms: [.iOS, .tvOS], traits: [TraitDescription.enableVideoOpenGLES.name, TraitDescription.enableDefaultVideo.name])),
			],
			additionalLinkerSettings: [
				.linkedFramework("CoreGraphics", .when(platforms: [.iOS, .macCatalyst, .tvOS, .visionOS])),
				.linkedFramework("GameController", .when(platforms: [.iOS, .macCatalyst, .tvOS, .visionOS])),
				.linkedFramework("OpenGLES", .when(platforms: [.iOS, .tvOS], traits: [TraitDescription.enableVideoOpenGLES.name, TraitDescription.enableDefaultVideo.name])),
			]
		),
		.target(
			name: "video_driver_uikit_swift",
			path: ".",
			exclude: createExcludePaths(for: ".", keeping: contentsOfDirectory(path: "src/video/uikit", files: true, withExtensions: ["swift"])),
			sources: contentsOfDirectory(path: "src/video/uikit", files: true, withExtensions: ["swift"]),
			swiftSettings: [
				.unsafeFlags([
					"-import-objc-header", "\(Context.packageDirectory)/src/video/uikit/SDL_UIKitBridge-swift.h",	// Uses Xcode's bridging header
					"-Xcc", "-fmodule-name=SimpleDirectMediaLayer",	// We need this to keep SDL_INTERNAL working
					"-Xcc", "-I\(Context.packageDirectory)/include",
					"-Xcc", "-I\(Context.packageDirectory)/swift/Sources/include/build_config",
					"-Xcc", "-I\(Context.packageDirectory)/include/build_config",
					"-Xcc", "-I\(Context.packageDirectory)/src",
					"-Xcc", "-idirafter", "-Xcc", "\(Context.packageDirectory)/src/video/khronos",
				]),
			]
		),


		// Additional Targets

		.sdlTarget(name: "storage_steam", sources: ["src/storage/steam"]),


		// MARK: - SimpleDirectMediaLayerTests

		.testTarget(
			name: "SimpleDirectMediaLayerTests",
			dependencies: [
				.target(name: "SimpleDirectMediaLayer"),
				.product(name: "Subprocess", package: "swift-subprocess", condition: .when(platforms: [.macOS])),
				.product(name: "SystemPackage", package: "swift-system"),
			],
			path: "swift/Tests/SimpleDirectMediaLayerTests",
		),


		// MARK: - testutils Library used by SDL Test Executables

		.target(
			name: "testutils",
			dependencies: [
				.target(name: "TestResources")
			],
			path: ".",
			exclude: createExcludePaths(for: ".", keeping: ["test/testutils.c"]),
			sources: [
				"test/testutils.c",
			],
			publicHeadersPath: "swift/Sources/testutils/include",
			cSettings: [
				.headerSearchPath("include"),
			],
		),


		// MARK: - Resource Files used by SDL Test Executables

		.target(
			name: "TestResources",
			dependencies: [
				.target(name: "BundleHelpers")
			],
			path: ".",
			exclude:
				createExcludePaths(for: ".", keeping: ["swift/Sources/TestResources"])
			+ contentsOfDirectory(path: "test", files: true, withExtensions: ["c", "cpp", "dat", "h", "hlsl", "in", "m", "markdown", "sh", "txt", "xbm", ""], directories: true, except: ["moose.dat", "utf8.txt"]),
			sources: [
				"swift/Sources/TestResources",
			],
			resources: (contentsOfDirectory(path: "test", files: true, withExtensions: ["png", "wav", "csv", "hex"]) + ["test/moose.dat", "test/utf8.txt"]).map { .copy($0) }
		),


		// MARK: - SDL Test Executables used by SimpleDirectMediaLayerTests.SDLTests

		.sdlTestExecutable(name: "childprocess"),
		.sdlTestExecutable(name: "pretest"),
		.sdlTestExecutable(name: "testatomic"),
		.sdlTestExecutable(name: "testaudioinfo"),
		.sdlTestExecutable(name: "testautomation", additionalSources: [
			"testautomation_audio.c",
			"testautomation_blit.c",
			"testautomation_clipboard.c",
			"testautomation_events.c",
			"testautomation_guid.c",
			"testautomation_hints.c",
			"testautomation_images.c",
			"testautomation_intrinsics.c",
			"testautomation_iostream.c",
			"testautomation_joystick.c",
			"testautomation_keyboard.c",
			"testautomation_log.c",
			"testautomation_main.c",
			"testautomation_math.c",
			"testautomation_mouse.c",
			"testautomation_pixels.c",
			"testautomation_platform.c",
			"testautomation_properties.c",
			"testautomation_rect.c",
			"testautomation_render.c",
			"testautomation_sdltest.c",
			"testautomation_stdlib.c",
			"testautomation_subsystems.c",
			"testautomation_surface.c",
			"testautomation_time.c",
			"testautomation_timer.c",
			"testautomation_video.c",
		], additionalCSettings: buildDependentCSettings),
		.sdlTestExecutable(name: "testbounds"),
		.sdlTestExecutable(name: "testdisplayinfo"),
		.sdlTestExecutable(name: "testerror"),
		.sdlTestExecutable(name: "testevdev", additionalCSettings: buildDependentCSettings),
		.sdlTestExecutable(name: "testfile"),
		.sdlTestExecutable(name: "testfilesystem"),
		.sdlTestExecutable(name: "testkeys"),
		.sdlTestExecutable(name: "testlocale"),
		.sdlTestExecutable(name: "testplatform"),
		.sdlTestExecutable(name: "testpower"),
		.sdlTestExecutable(name: "testprocess"),
		.sdlTestExecutable(name: "testqsort"),
		.sdlTestExecutable(name: "testrwlock"),
		.sdlTestExecutable(name: "testsem"),
		.sdlTestExecutable(name: "testsymbols"),
		.sdlTestExecutable(name: "testthread"),
		.sdlTestExecutable(name: "testtimer"),
		.sdlTestExecutable(name: "testver"),
		.sdlTestExecutable(name: "testyuv", additionalDependencies: ["testutils", "TestResources"], additionalSources: ["testyuv_cvt.c"]),
		.sdlTestExecutable(name: "torturethread"),


		// MARK: - Standalone SDL Test Executables

		.sdlTestExecutable(name: "checkkeys"),
		.sdlTestExecutable(name: "loopwave", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testasyncio", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testaudio", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testaudiohotplug", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testaudiorecording"),
		.sdlTestExecutable(name: "testaudiostreamdynamicresample", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testcamera"),
		.sdlTestExecutable(name: "testclipboard"),
		.sdlTestExecutable(name: "testcolorspace"),
		.sdlTestExecutable(name: "testcontroller", additionalDependencies: ["testutils"], additionalSources: ["gamepadutils.c"]),
		.sdlTestExecutable(name: "testcustomcursor"),
		.sdlTestExecutable(name: "testdescriptor", additionalCSettings: buildDependentCSettings + [.define("DEBUG_DESCRIPTOR")]),
		.sdlTestExecutable(name: "testdialog"),
		.sdlTestExecutable(name: "testdlopennote", additionalDependencies: ["testutils"]),
		.sdlTestExecutable(name: "testdraw"),
		.sdlTestExecutable(name: "testdrawchessboard"),
		.sdlTestExecutable(name: "testdropfile"),
		.sdlTestExecutable(name: "testdynaudioreopen", additionalDependencies: ["testutils", "TestResources"]),
		.executableTarget( // We have to special case testffmpeg to handle ffmpeg being installed or not.
			name: "testffmpeg",
			dependencies: [
				.target(name: "SimpleDirectMediaLayer"),
				.target(name: "SDL3_test"),
			],
			path: "swift/Sources/testffmpeg",
			cSettings: [
				.unsafeFlags(["-include", "\(Context.packageDirectory)/swift/Sources/SimpleDirectMediaLayer/include/SDL3/SDL_revision.h"]),
				.define("HAVE_BUILD_CONFIG"),	// CMake defines it for all tests even though only used by some
				.define("HAVE_SIGNAL_H"),
				.headerSearchPath("../../../src/video/khronos"),
			] + TraitDescription.allCSettings,
			linkerSettings: [
				.linkedLibrary("avcodec", .when(traits: [TraitDescription.enableTestFFmpeg.name])),
				.linkedLibrary("avformat", .when(traits: [TraitDescription.enableTestFFmpeg.name])),
				.linkedLibrary("avutil", .when(traits: [TraitDescription.enableTestFFmpeg.name])),
				.linkedLibrary("swscale", .when(traits: [TraitDescription.enableTestFFmpeg.name])),
				.linkedFramework("CoreVideo", .when(platforms: [.macOS], traits: [TraitDescription.enableTestFFmpeg.name])),
			],
		),
		.sdlTestExecutable(name: "testgeometry", additionalDependencies: ["testutils"]),
		.sdlTestExecutable(
			name: "testgl",
			additionalCSettings: [.define("HAVE_OPENGL", .when(platforms: [.macOS, .linux, .windows]))],
			additionalLinkerSettings: [.linkedFramework("OpenGL", .when(platforms: [.macOS]))]
		),
		.sdlTestExecutable(name: "testgles", additionalCSettings: [.define("GLES_SILENCE_DEPRECATION")]),
		.sdlTestExecutable(name: "testgles2"),
		.sdlTestExecutable(name: "testgpu_simple_clear"),
		.sdlTestExecutable(name: "testgpu_spinning_cube"),
		.sdlTestExecutable(name: "testgpu_spinning_cube_xr"),
		.sdlTestExecutable(name: "testgpurender_effects", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testgpurender_msdf", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testhaptic"),
		.sdlTestExecutable(name: "testhittesting"),
		.sdlTestExecutable(name: "testhotplug"),
		.sdlTestExecutable(name: "testiconv", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testime", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testintersections"),
		.sdlTestExecutable(name: "testloadso"),
		.sdlTestExecutable(name: "testlock"),
		.sdlTestExecutable(name: "testmanymouse"),
		.sdlTestExecutable(name: "testmessage"),
		.sdlTestExecutable(name: "testmodal"),
		.sdlTestExecutable(name: "testmouse"),
		.sdlTestExecutable(name: "testmultiaudio", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(
			name: "testnative",
			additionalDependencies: ["testutils", "TestResources"],
			additionalSources: ["testnativecocoa.m", "testnativex11.c"],
			additionalCSettings: buildDependentCSettings + [.unsafeFlags(["-fno-objc-arc"])],
		),
		.sdlTestExecutable(name: "testnotification", additionalDependencies: ["TestResources"]),
		.sdlTestExecutable(name: "testoffscreen"),
		.sdlTestExecutable(name: "testoverlay", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testpalette"),
		.sdlTestExecutable(name: "testpen"),
		.sdlTestExecutable(name: "testpopup"),
		.sdlTestExecutable(name: "testrelative"),
		.sdlTestExecutable(name: "testrendercopyex", additionalDependencies: ["testutils", "TestResources"], additionalCSettings: [.unsafeFlags(["-fno-modules"])]),
		.sdlTestExecutable(name: "testrendertarget", additionalDependencies: ["testutils", "TestResources"], additionalCSettings: [.unsafeFlags(["-fno-modules"])]),
		.sdlTestExecutable(name: "testresample", additionalDependencies: ["TestResources"]),
		.sdlTestExecutable(name: "testrotate"),
		.sdlTestExecutable(name: "testrumble"),
		.sdlTestExecutable(name: "testscale", additionalDependencies: ["testutils", "TestResources"], additionalCSettings: [.unsafeFlags(["-fno-modules"])]),
		.sdlTestExecutable(name: "testsensor"),
		.sdlTestExecutable(
			name: "testshader",
			additionalDependencies: ["testutils", "TestResources"],
			additionalCSettings: [.define("HAVE_OPENGL", .when(platforms: [.macOS, .linux, .windows]))],
			additionalLinkerSettings: [.linkedFramework("OpenGL", .when(platforms: [.macOS]))]
		),
		.sdlTestExecutable(name: "testshape", additionalDependencies: ["TestResources"]),
		.sdlTestExecutable(name: "testsoftwaretransparent"),
		.sdlTestExecutable(name: "testsprite", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testspritecxx", sources: ["testsprite.cpp"], additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testspriteminimal"),
		.sdlTestExecutable(name: "testspritesurface"),
		.sdlTestExecutable(name: "testsurround"),
		.sdlTestExecutable(name: "testtime"),
		.sdlTestExecutable(name: "testtray", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testurl"),
		.sdlTestExecutable(name: "testviewport", additionalDependencies: ["testutils", "TestResources"]),
		.sdlTestExecutable(name: "testvulkan"),
//		.sdlTestExecutable(name: "testwaylandcustom"),	// Wayland only, generated source, needs pkg-config wayland-client, I think needed for Linux, can we #define around it?
		.sdlTestExecutable(name: "testwm"),


		// MARK: - Resource Files used by SDL Example Executables

		.target(
			name: "ExampleResources",
			dependencies: [
				.target(name: "BundleHelpers")
			],
			path: ".",
			exclude: createExcludePaths(for: ".", keeping: [
				"swift/Sources/ExampleResources",
				"test/sample.png",
				"test/gamepad_front.png",
				"test/speaker.png",
				"test/icon2x.png",
				"test/sample.wav",
				"test/sword.wav",
			]),
			sources: [
				"swift/Sources/ExampleResources",
			],
			resources: [
				.copy("test/sample.png"),
				.copy("test/gamepad_front.png"),
				.copy("test/speaker.png"),
				.copy("test/icon2x.png"),
				.copy("test/sample.wav"),
				.copy("test/sword.wav"),
			]
		),


		// MARK: - SDL Example Executables

		.sdlExampleExecutable(name: "asyncio-load-bitmaps", sources: ["asyncio/01-load-bitmaps/load-bitmaps.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "audio-load-wav", sources: ["audio/03-load-wav/load-wav.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "audio-multiple-streams", sources: ["audio/04-multiple-streams/multiple-streams.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "audio-planar-data", sources: ["audio/05-planar-data/planar-data.c"]),
		.sdlExampleExecutable(name: "audio-simple-playback", sources: ["audio/01-simple-playback/simple-playback.c"]),
		.sdlExampleExecutable(name: "audio-simple-playback-callback", sources: ["audio/02-simple-playback-callback/simple-playback-callback.c"]),
		.sdlExampleExecutable(name: "camera-read-and-draw", sources: ["camera/01-read-and-draw/read-and-draw.c"]),
		.sdlExampleExecutable(name: "demo-snake", sources: ["demo/01-snake/snake.c"]),
		.sdlExampleExecutable(name: "demo-woodeneye-008", sources: ["demo/02-woodeneye-008/woodeneye-008.c"]),
		.sdlExampleExecutable(name: "demo-infinite-monkeys", sources: ["demo/03-infinite-monkeys/infinite-monkeys.c"]),
		.sdlExampleExecutable(name: "demo-bytepusher", sources: ["demo/04-bytepusher/bytepusher.c"]),
		.sdlExampleExecutable(name: "input-gamepad-events", sources: ["input/04-gamepad-events/gamepad-events.c"]),
		.sdlExampleExecutable(name: "input-gamepad-polling", sources: ["input/03-gamepad-polling/gamepad-polling.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "input-gamepad-rumble", sources: ["input/05-gamepad-rumble/gamepad-rumble.c"]),
		.sdlExampleExecutable(name: "input-joystick-events", sources: ["input/02-joystick-events/joystick-events.c"]),
		.sdlExampleExecutable(name: "input-joystick-polling", sources: ["input/01-joystick-polling/joystick-polling.c"]),
		.sdlExampleExecutable(name: "misc-clipboard", sources: ["misc/02-clipboard/clipboard.c"]),
		.sdlExampleExecutable(name: "misc-locale", sources: ["misc/03-locale/locale.c"]),
		.sdlExampleExecutable(name: "misc-power", sources: ["misc/01-power/power.c"]),
		.sdlExampleExecutable(name: "pen-drawing-lines", sources: ["pen/01-drawing-lines/drawing-lines.c"]),
		.sdlExampleExecutable(name: "renderer-affine-textures", sources: ["renderer/19-affine-textures/affine-textures.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "renderer-blending", sources: ["renderer/20-blending/blending.c"]),
		.sdlExampleExecutable(name: "renderer-clear", sources: ["renderer/01-clear/clear.c"]),
		.sdlExampleExecutable(name: "renderer-cliprect", sources: ["renderer/15-cliprect/cliprect.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "renderer-color-mods", sources: ["renderer/11-color-mods/color-mods.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "renderer-debug-text", sources: ["renderer/18-debug-text/debug-text.c"]),
		.sdlExampleExecutable(name: "renderer-geometry", sources: ["renderer/10-geometry/geometry.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "renderer-lines", sources: ["renderer/03-lines/lines.c"]),
		.sdlExampleExecutable(name: "renderer-points", sources: ["renderer/04-points/points.c"]),
		.sdlExampleExecutable(name: "renderer-primitives", sources: ["renderer/02-primitives/primitives.c"]),
		.sdlExampleExecutable(name: "renderer-read-pixels", sources: ["renderer/17-read-pixels/read-pixels.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "renderer-rectangles", sources: ["renderer/05-rectangles/rectangles.c"]),
		.sdlExampleExecutable(name: "renderer-rotating-textures", sources: ["renderer/08-rotating-textures/rotating-textures.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "renderer-scaling-textures", sources: ["renderer/09-scaling-textures/scaling-textures.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "renderer-streaming-textures", sources: ["renderer/07-streaming-textures/streaming-textures.c"]),
		.sdlExampleExecutable(name: "renderer-textures", sources: ["renderer/06-textures/textures.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "renderer-viewport", sources: ["renderer/14-viewport/viewport.c"], additionalDependencies: ["ExampleResources"]),
		.sdlExampleExecutable(name: "storage-user", sources: ["storage/01-user/user.c"]),


		// MARK: - Bundle Helpers used by TestResources and ExampleResources

		.target(
			name: "BundleHelpers",
			path: "swift/Sources/BundleHelpers",
		),


		// MARK: - Build Plugins

		.plugin(
			name: "BuildSDLRevisionHeaderPlugin",
			capability: .buildTool(),
			dependencies: [
				.target(name: "BuildSDLRevisionHeader"),
			],
			path: "swift/Sources/BuildSDLRevisionHeaderPlugin"
		),


		// MARK: - Build Tools used by Build Plugins

		.executableTarget(
			name: "BuildSDLRevisionHeader",
			path: "swift/Sources/BuildSDLRevisionHeader"
		),


		// MARK: - This is a hack to build the revision header before SimpleDirectMediaLayer sees it since it doesn't wait for the plugin

		.target(
			name: "revision_header",
			path: "swift/Sources/revision_header",
			plugins: [
				"BuildSDLRevisionHeaderPlugin",
			]
		),
	]
)
