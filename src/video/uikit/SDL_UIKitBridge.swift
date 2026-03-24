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

import SwiftUI

extension SDL_uikitviewcontroller {
    @available(visionOS 26.0, *)
    @objc func addOrnaments() {
        // This is called before the SwiftUI runtime is initialized, so run this in a task later
        Task {
            ornaments = [
                UIHostingOrnament(sceneAnchor: .topTrailing, contentAlignment: .leading) {
                    Button(action: {
                        self.ornaments = []
                        SDL_VisionOS_EnterImmersiveMode()
                    }) {
                        Label {
                            Text("Enter Immersive")
                        } icon: {
                            EnterImmersiveButtonIcon()
                        }
                    }
                    .frame(width: 80, height: 80)
                    .glassBackgroundEffect()
                    .labelStyle(.iconOnly)
                }
            ]
        }
    }
}
