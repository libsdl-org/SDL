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

// Generates SDL_revision_swiftpm.h from the revision written by BuildSDLRevision.swift,
// mirroring what CMake's configure_file() produces from include/build_config/SDL_revision.h.cmake.
//
// Used by BuildSDLRevisionHeaderPlugin's build command:
//     BuildSDLRevisionHeader <revision-file> <output-header>

import Foundation

enum ToolError: LocalizedError {
	case missingArguments
	case badArguments(String)

	var errorDescription: String? {
		switch self {
			case .missingArguments:
				"Missing arguments"

			case .badArguments(let arguments):
				"Bad arguments: \(arguments)"
		}
	}
}

struct StandardError: TextOutputStream {
	func write(_ string: String) {
		guard string.isEmpty == false else {
			return
		}

		FileHandle.standardError.write(Data(string.utf8))
	}
}

var standardError = StandardError()

do {
	guard CommandLine.arguments.count > 2 else {
		throw ToolError.missingArguments
	}
	guard CommandLine.arguments.count == 3 else {
		throw ToolError.badArguments(CommandLine.arguments.dropFirst().joined(separator: " "))
	}

	let revisionURL = URL(fileURLWithPath: CommandLine.arguments[1])
	let sdlRevision = try String(contentsOf: revisionURL, encoding: .utf8).trimmingCharacters(in: .whitespacesAndNewlines)
	let sdlRevisionHeader = """
	   #ifndef SDL_revision_swiftpm_h_
	   #define SDL_revision_swiftpm_h_

	   #ifdef SDL_VENDOR_INFO
	   #define SDL_REVISION "\(sdlRevision) (" SDL_VENDOR_INFO ")"
	   #else
	   #define SDL_REVISION "\(sdlRevision)"
	   #endif

	   #endif /* SDL_revision_swiftpm_h_ */

	   """
	let sdlRevisionHeaderURL = URL(fileURLWithPath: CommandLine.arguments[2])
	let oldSDLRevisionHeader = try? String(contentsOf: sdlRevisionHeaderURL, encoding: .utf8)

	if oldSDLRevisionHeader != sdlRevisionHeader {
		try FileManager.default.createDirectory(at: sdlRevisionHeaderURL.deletingLastPathComponent(), withIntermediateDirectories: true)
		try sdlRevisionHeader.write(to: sdlRevisionHeaderURL, atomically: true, encoding: .utf8)
	}
} catch {
	print("error: BuildSDLRevisionHeader: \(error.localizedDescription)", to: &standardError)
	exit(1)
}
