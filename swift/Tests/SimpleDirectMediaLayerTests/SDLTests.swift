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

#if canImport(Subprocess)

import Testing
import Subprocess
import System
import Foundation

@Suite(.withPretest)
struct `SDL Tests` {
	static let defaultEnvironment = Environment.inherit.updating([
		"SDL_VIDEO_DRIVER": "dummy",
		"SDL_AUDIO_DRIVER": "dummy",
		"SDL_ASSERT": "abort",
	])
	static let defaultTimeout = Duration.seconds(10)

	@Test func testatomic() async throws {
		try await Self.runTest(name: "testatomic", timeout: .seconds(20))
	}

	@Test func testaudioinfo() async throws {
		try await Self.runTest(name: "testaudioinfo")
	}

	@Test func testautomation() async throws {
		try await Self.runTest(name: "testautomation", timeout: .seconds(120))
	}

	@Test func `testautomation-no-simd`() async throws {
		try await Self.runTest(name: "testautomation", environment: Self.defaultEnvironment.updating(["SDL_CPU_FEATURE_MASK": "-all"]), timeout: .seconds(120))
	}

	@Test func testbounds() async throws {
		try await Self.runTest(name: "testbounds")
	}

	@Test func testdisplayinfo() async throws {
		try await Self.runTest(name: "testdisplayinfo")
	}

	@Test func testerror() async throws {
		try await Self.runTest(name: "testerror")
	}

	@Test func testevdev() async throws {
		try await Self.runTest(name: "testevdev")
	}

	@Test func testfile() async throws {
		try await Self.runTest(name: "testfile")
	}

	@Test func testfilesystem() async throws {
		try await Self.runTest(name: "testfilesystem")
	}

	@Test func testkeys() async throws {
		try await Self.runTest(name: "testkeys")
	}

	@Test func testlocale() async throws {
		try await Self.runTest(name: "testlocale")
	}

	@Test func testplatform() async throws {
		try await Self.runTest(name: "testplatform")
	}

	@Test func `testplatform-no-simd`() async throws {
		try await Self.runTest(name: "testplatform", environment: Self.defaultEnvironment.updating(["SDL_CPU_FEATURE_MASK": "-all"]))
	}

	@Test func testpower() async throws {
		try await Self.runTest(name: "testpower")
	}

	@Test func testprocess() async throws {
		try await Self.runTest(name: "testprocess", arguments: [productsDirectoryURL.appending(path: "childprocess").path(percentEncoded: false)])
	}

	@Test func testqsort() async throws {
		try await Self.runTest(name: "testqsort")
	}

	@Test func testrwlock() async throws {
		try await Self.runTest(name: "testrwlock", timeout: .seconds(20))
	}

	@Test func testsem() async throws {
		try await Self.runTest(name: "testsem", arguments: ["10"], timeout: .seconds(30))
	}

	@Test func testsymbols() async throws {
		try await Self.runTest(name: "testsymbols", arguments: ["0", "10", "20", "40", "80", "160", "320", "640"])
	}

	@Test func testthread() async throws {
		try await Self.runTest(name: "testthread", timeout: .seconds(40))
	}

	@Test func testtimer() async throws {
		try await Self.runTest(name: "testtimer", arguments: ["--no-interactive"], timeout: .seconds(60))
	}

	@Test func testver() async throws {
		try await Self.runTest(name: "testver")
	}

	@Test func testyuv() async throws {
		try await Self.runTest(name: "testyuv", arguments: ["--automated"])
	}

	@Test func torturethread() async throws {
		try await Self.runTest(name: "torturethread", timeout: .seconds(30))
	}

	static func runTest(name: String, arguments: Arguments = [], environment: Environment = defaultEnvironment, timeout: Duration = defaultTimeout, sourceLocation: SourceLocation = #_sourceLocation) async throws {
		let productsDirectoryFilePath = FilePath(productsDirectoryURL)!

		await #expect(throws: Never.self, sourceLocation: sourceLocation) {
			let temporaryDirectoryURL = URL.temporaryDirectory.appending(path: UUID().uuidString)

			try FileManager.default.createDirectory(at: temporaryDirectoryURL, withIntermediateDirectories: true)
			defer {
				try? FileManager.default.removeItem(at: temporaryDirectoryURL)
			}

			let logURL = temporaryDirectoryURL.appending(path: "\(name).log")
			let fd = try FileDescriptor.open(FilePath(logURL.path(percentEncoded: false)), .writeOnly, options: [.create, .exclusiveCreate], permissions: .ownerReadWrite)

			do {
				let result = try await withThrowingTaskGroup { group in
					group.addTask {
						try await run(
							.path(productsDirectoryFilePath.appending(name)),
							arguments: arguments,
							environment: environment,
							workingDirectory: .init(temporaryDirectoryURL),
							output: .fileDescriptor(fd, closeAfterSpawningProcess: true),
							error: .combinedWithOutput
						)
					}
					group.addTask {
						try await Task.sleep(for: timeout)
						throw await SDLTestingError.timedOut(timeout: timeout, output: tailLines(of: logURL))
					}

					let result = try await group.next()
					group.cancelAll()

					return result!
				}

				guard result.terminationStatus == .exited(0) else {
					throw await SDLTestingError.error(terminationStatus: result.terminationStatus, output: tailLines(of: logURL))
				}
			} catch {
				if let workingDirectoryAttachment = try? await Attachment(contentsOf: temporaryDirectoryURL, named: "working_directory") {
					Attachment.record(workingDirectoryAttachment)
				}
				throw error
			}
		}
	}

	static func tailLines(of url: URL, maxLines: Int = 500) async -> String {
		var lines: [String] = []

		lines.reserveCapacity(maxLines)
		do {
			for try await line in url.lines {
				if lines.count == maxLines {
					lines.removeFirst()
				}
				lines.append(line)
			}
		} catch {
			lines.append("Unable to read output recorded at \(url.path(percentEncoded: false)): \(error)")
		}

		return lines.joined(separator: "\n")
	}
}

private final class BundleFinder { }
private let productsDirectoryURL = Bundle(for: BundleFinder.self).bundleURL.deletingLastPathComponent()

struct PretestTrait: SuiteTrait & TestScoping {
	func provideScope(for test: Test, testCase: Test.Case?, performing function: () async throws -> Void) async throws {
		try await pretest()
		try await function()
	}

	func pretest() async throws {
		let temporaryDirectoryURL = URL.temporaryDirectory.appending(path: UUID().uuidString)

		try FileManager.default.createDirectory(at: temporaryDirectoryURL, withIntermediateDirectories: true)
		defer {
			try? FileManager.default.removeItem(at: temporaryDirectoryURL)
		}

		let logURL = temporaryDirectoryURL.appending(path: "pretest.log")
		let fd = try FileDescriptor.open(FilePath(logURL.path(percentEncoded: false)), .writeOnly, options: [.create, .exclusiveCreate], permissions: .ownerReadWrite)

		let result = try await run(
			.path(.init(productsDirectoryURL)!.appending("pretest")),
			workingDirectory: .init(temporaryDirectoryURL),
			output: .fileDescriptor(fd, closeAfterSpawningProcess: true),
			error: .combinedWithOutput
		)

		guard result.terminationStatus == .exited(0) else {
			throw SDLTestingError.pretestFailed(output: (try? String(contentsOf: logURL, encoding: .utf8)) ?? "Unable to read \(logURL)")
		}
	}
}

extension SuiteTrait where Self == PretestTrait {
	static var withPretest: PretestTrait {
		PretestTrait()
	}
}

enum SDLTestingError: LocalizedError, CustomStringConvertible {
	case pretestFailed(output: String)
	case error(terminationStatus: TerminationStatus, output: String)
	case timedOut(timeout: Duration, output: String)

	var errorDescription: String? {
		description
	}

	var description: String {
		switch self {
			case .pretestFailed(let output):
				"Error: pretest failed: \(output)"

			case .error(let terminationStatus, let output):
				"Error: (\(terminationStatus)) output:\n\(output)"

			case .timedOut(let timeout, let output):
				"Error: timed out (\(timeout)) output:\n\(output)"
		}
	}
}

#endif
