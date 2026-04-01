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
import RealityKit
import Metal

/// UIHostingController subclass that hides the visionOS glass background.
@available(visionOS 26.0, *)
private class SDL_ClearHostingController<Content: View>: UIHostingController<Content> {
    override var preferredContainerBackgroundStyle: UIContainerBackgroundStyle {
        return .hidden
    }
}

/// ObjC-accessible wrapper that manages presenting SDL curved content
/// via a UIHostingController (no ImmersiveSpace required).
@available(visionOS 26.0, *)
@MainActor
@objc(SDL_CurvedContentHosting)
public class SDL_CurvedContentHosting: NSObject {

    private let helper = SDL_RealityKitHelper()
    private var hostingController: UIHostingController<SDL_CurvedContentView>?

    @objc public override init() {
        super.init()
    }

    /// Configure size and curvature before presenting.
    @objc public func configure(size: CGSize, curvature: Float) {
        helper.configure(width: Int(size.width), height: Int(size.height), curvature: curvature)
    }

    /// Present the curved content view full-screen from the given view controller.
    @objc public func present(from viewController: UIViewController) {
        let contentView = SDL_CurvedContentView(helper: helper)
        let hc = SDL_ClearHostingController(rootView: contentView)
        hc.modalPresentationStyle = .fullScreen
        hc.view.backgroundColor = .clear
        hostingController = hc

        viewController.present(hc, animated: true) { [weak self] in
            guard let self, let hc = self.hostingController else { return }
            self.addOrnaments(to: hc)
        }
        NSLog("SDL_CurvedContentHosting: Presented full-screen UIHostingController")
    }

    private func addOrnaments(to viewController: UIViewController) {
        let curvedHelper = helper
        viewController.ornaments = [
            UIHostingOrnament(sceneAnchor: .bottom, contentAlignment: .center) {
                SDL_CurvedContentOrnamentView(helper: curvedHelper)
            }
        ]
    }

    /// Dismiss the curved content view.
    @objc public func dismiss() {
        guard let hc = hostingController else {
            NSLog("SDL_CurvedContentHosting: No hosting controller to dismiss")
            return
        }
        hc.dismiss(animated: true) {
            NSLog("SDL_CurvedContentHosting: Dismissed UIHostingController")
        }
        hostingController = nil
    }

    /// Get the display texture for this frame.
    @objc public func getDisplayTexture(_ commandBuffer: MTLCommandBuffer, width: Int, height: Int, pixelFormat: MTLPixelFormat) -> MTLTexture? {
        return helper.getDisplayTexture(commandBuffer, width: width, height: height, pixelFormat: pixelFormat)
    }

    /// Update the content size dynamically.
    @objc public func updateSize(_ size: CGSize) {
        helper.updateSize(width: Int(size.width), height: Int(size.height))
    }

    /// Update the curvature dynamically.
    @objc public func updateCurvature(_ curvature: Float) {
        helper.updateCurvature(curvature: curvature)
    }

    /// Whether the hosting controller is currently presented.
    @objc public var isPresented: Bool {
        return hostingController != nil
    }
}

/// Ornament view with curvature and close buttons for curved content mode.
@available(visionOS 26.0, *)
struct SDL_CurvedContentOrnamentView: View {
    let helper: SDL_RealityKitHelper

    var body: some View {
        HStack(spacing: 32) {
            if helper.meshCurvature == 0.0 {
                Button(action: {
                    helper.updateCurvature(curvature: 0.2)
                    SDL_VisionOS_SendCurvatureChanged(0.2)
                }) {
                    FlatButtonIcon()
                        .frame(width: 48, height: 48)
                }
                .frame(width: 48, height: 48)
            } else if helper.meshCurvature == 0.2 {
                Button(action: {
                    helper.updateCurvature(curvature: 0.4)
                    SDL_VisionOS_SendCurvatureChanged(0.4)
                }) {
                    CurvedButtonIcon()
                        .frame(width: 48, height: 48)
                }
                .frame(width: 48, height: 48)
            } else {
                Button(action: {
                    helper.updateCurvature(curvature: 0.0)
                    SDL_VisionOS_SendCurvatureChanged(0.0)
                }) {
                    CurviestButtonIcon()
                        .frame(width: 48, height: 48)
                }
                .frame(width: 48, height: 48)
            }

            Button(action: {
                SDL_VisionOS_LeaveCurvedMode()
            }) {
                Image(systemName: "rectangle.arrowtriangle.2.inward")
            }
            .frame(width: 48, height: 48)
        }
    }
}
