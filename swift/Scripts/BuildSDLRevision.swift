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

// Determines the SDL revision the way CMakeLists.txt does: from REVISION.txt in a source
// release, otherwise from the version in SDL_version.h plus git describe. The result is
// written to a file that the BuildSDLRevisionHeader tool turns into SDL_revision_swiftpm.h.
//
// Run as a script (no package dependencies are available in script mode):
//     swift swift/Scripts/BuildSDLRevision.swift <package-dir> <output-file>

import Foundation

enum ScriptError: LocalizedError {
	case missingArguments
	case badArguments(String)
	case notDirectory(String)
	case versionParseError(String)

	var errorDescription: String? {
		switch self {
			case .missingArguments:
				"Missing arguments"

			case .badArguments(let arguments):
				"Bad arguments: \(arguments)"

			case .notDirectory(let path):
				"Bad arguments: \(path) is not a directory!"

			case .versionParseError(let path):
				"Unable to parse version header at \"\(path)\""
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

func git(_ arguments: [String], packageDirectory: URL) throws -> (status: Int32, stdout: String) {
	let process = Process()
	let stdoutPipe = Pipe()

	process.executableURL = URL(filePath: "/usr/bin/env")
	process.arguments = [
		"git",
		"-C",
		packageDirectory.path(percentEncoded: false),
	] + arguments
	process.standardOutput = stdoutPipe
	process.standardError = FileHandle.nullDevice

	try process.run()

	let stdoutData = try stdoutPipe.fileHandleForReading.readToEnd() ?? Data()

	process.waitUntilExit()

	return (status: process.terminationStatus, stdout: String(decoding: stdoutData, as: UTF8.self))
}

// Mirrors git_describe() in cmake/GetGitRevisionDescription.cmake, including its NOTFOUND results.
func gitDescribe(packageDirectory: URL) throws -> String {
	let head = try git(["rev-parse", "HEAD"], packageDirectory: packageDirectory)

	// /usr/bin/env exits with 127 when it can't find git.
	guard head.status != 127 else {
		return "GIT-NOTFOUND"
	}

	let headOutput = head.stdout.trimmingCharacters(in: .whitespacesAndNewlines)

	guard head.status == 0, !headOutput.isEmpty else {
		return "HEAD-HASH-NOTFOUND"
	}

	let describe = try git(["describe", "--tags", "--always", headOutput], packageDirectory: packageDirectory)
	let describeOutput = describe.stdout.trimmingCharacters(in: .whitespacesAndNewlines)

	guard describe.status == 0 else {
		return "\(describeOutput)-\(describe.status)-NOTFOUND"
	}

	return describeOutput
}

do {
	guard CommandLine.arguments.count > 2 else {
		throw ScriptError.missingArguments
	}
	guard CommandLine.arguments.count == 3 else {
		throw ScriptError.badArguments(CommandLine.arguments.dropFirst().joined(separator: " "))
	}

	let packageDirectoryURL = URL(filePath: CommandLine.arguments[1])
	let values = try packageDirectoryURL.resourceValues(forKeys: [.isDirectoryKey])

	guard values.isDirectory == true else {
		throw ScriptError.notDirectory(CommandLine.arguments[1])
	}

	let sdlRevision: String
	let revisionTextURL = packageDirectoryURL.appending(path: "REVISION.txt")

	// Source releases include REVISION.txt, so they don't need git.
	if let revisionText = try? String(contentsOf: revisionTextURL, encoding: .utf8), let firstLine = revisionText.split(whereSeparator: \.isNewline).first {
		sdlRevision = "SDL-\(firstLine.trimmingCharacters(in: .whitespaces))"
	} else {
		let versionHeaderURL = packageDirectoryURL.appending(path: "include/SDL3/SDL_version.h")
		let versionHeader = try String(contentsOf: versionHeaderURL, encoding: .utf8)
		let versionRegex = #/#define\s+SDL_MAJOR_VERSION\s+(?<major>\d+)[\s\S]*?#define\s+SDL_MINOR_VERSION\s+(?<minor>\d+)[\s\S]*?#define\s+SDL_MICRO_VERSION\s+(?<micro>\d+)/#

		guard let match = versionHeader.firstMatch(of: versionRegex) else {
			throw ScriptError.versionParseError(versionHeaderURL.path(percentEncoded: false))
		}

		sdlRevision = "SDL-\(match.major).\(match.minor).\(match.micro)-\(try gitDescribe(packageDirectory: packageDirectoryURL))"
	}

	let newRevision = sdlRevision + "\n"
	let revisionURL = URL(filePath: CommandLine.arguments[2])
	let oldRevision = try? String(contentsOf: revisionURL, encoding: .utf8)

	// Only write when the revision changes: a prebuild command runs on every build, and rewriting the file would make the build command regenerate the header every time.
	if oldRevision != newRevision {
		try FileManager.default.createDirectory(at: revisionURL.deletingLastPathComponent(), withIntermediateDirectories: true)
		try newRevision.write(to: revisionURL, atomically: true, encoding: .utf8)
	}
} catch {
	print("error: BuildSDLRevision: \(error.localizedDescription)", to: &standardError)
	exit(1)
}
