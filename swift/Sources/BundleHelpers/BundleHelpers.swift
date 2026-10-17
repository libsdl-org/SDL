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

public func getBundleResourceDirectory(bundle: Bundle, buffer: UnsafeMutablePointer<CChar>?, size: CInt) -> CBool {
	guard let buffer, let url = bundle.resourceURL else {
		return false
	}

	let path = url.path.utf8CString

	guard path.count <= size else {
		return false
	}

	path.withUnsafeBufferPointer {
		buffer.update(from: $0.baseAddress!, count: $0.count)
	}

	return true
}

public func getBundleResource(bundle: Bundle, name: UnsafePointer<CChar>?, buffer: UnsafeMutablePointer<CChar>?, size: CInt) -> CBool {
	guard let name, let buffer, let name = String(validatingCString: name), let url = bundle.url(forResource: name, withExtension: nil) else {
		return false
	}

	let path = url.path.utf8CString

	guard path.count <= size else {
		return false
	}

	path.withUnsafeBufferPointer {
		buffer.update(from: $0.baseAddress!, count: $0.count)
	}

	return true
}
