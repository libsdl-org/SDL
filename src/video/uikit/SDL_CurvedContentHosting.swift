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

// Icons used by buttons below

// Flat button
/* SVG:
 <svg width="800" height="800" viewBox="0 0 800 800" fill="none" xmlns="http://www.w3.org/2000/svg">
 <path d="M133.333 400H666.667" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 </svg>
 */
struct FlatButtonIcon : Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        let width = rect.size.width
        let height = rect.size.height
        var strokePath = Path()
        strokePath.move(to: CGPoint(x: 0.16667*width, y: 0.5*height))
        strokePath.addLine(to: CGPoint(x: 0.83333*width, y: 0.5*height))
        path.addPath(strokePath.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        return path
    }
}

// Curved button
/* SVG:
 <svg width="800" height="800" viewBox="0 0 800 800" fill="none" xmlns="http://www.w3.org/2000/svg">
 <path d="M133 380C311 317.333 489 317.333 667 380" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 </svg>
 */
struct CurvedButtonIcon : Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        let width = rect.size.width
        let height = rect.size.height
        var strokePath = Path()
        strokePath.move(to: CGPoint(x: 0.16625*width, y: 0.475*height))
        strokePath.addCurve(to: CGPoint(x: 0.83375*width, y: 0.475*height), control1: CGPoint(x: 0.38875*width, y: 0.39667*height), control2: CGPoint(x: 0.61125*width, y: 0.39667*height))
        path.addPath(strokePath.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        return path
    }
}

// Curviest button
/* SVG:
 <svg width="800" height="800" viewBox="0 0 800 800" fill="none" xmlns="http://www.w3.org/2000/svg">
 <path d="M133 370C310.667 230 488.333 230 666 370" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 </svg>
 */
struct CurviestButtonIcon : Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        let width = rect.size.width
        let height = rect.size.height
        var strokePath = Path()
        strokePath.move(to: CGPoint(x: 0.16625*width, y: 0.4625*height))
        strokePath.addCurve(to: CGPoint(x: 0.8325*width, y: 0.4625*height), control1: CGPoint(x: 0.38833*width, y: 0.2875*height), control2: CGPoint(x: 0.61042*width, y: 0.2875*height))
        path.addPath(strokePath.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        return path
    }
}

/// UIHostingController subclass that hides the visionOS glass background.
@available(visionOS 26.0, *)
private class SDL_ClearHostingController<Content: View>: UIHostingController<Content> {
    override var preferredContainerBackgroundStyle: UIContainerBackgroundStyle {
        return .hidden
    }
}

/// ObjC-accessible wrapper that manages presenting SDL curved content
/// via a UIHostingController
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
                SDL_CurvedContentCurvatureOrnamentView(helper: curvedHelper)
            },
            UIHostingOrnament(sceneAnchor: .topTrailing, contentAlignment: .leading) {
                SDL_CurvedContentCloseOrnamentView()
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

/// Ornament view with curvature control curved content mode.
@available(visionOS 26.0, *)
struct SDL_CurvedContentCurvatureOrnamentView: View {
    let helper: SDL_RealityKitHelper
    @State var changingCurvature: Bool = false

    var body: some View {
        if ( changingCurvature ) {
            Slider(
                value: Binding(
                    get: { Double( helper.meshCurvature * 100 ) },
                    set: {
                        var curvature = $0 / 100
                        if (curvature < 0.01) {
                            curvature = 0.0
                        }
                        helper.updateCurvature(curvature: Float(curvature))
                        SDL_VisionOS_SendCurvatureChanged(curvature)
                    }
                ),
                in: 0...60,
                onEditingChanged: { editing in
                    changingCurvature = editing
                }
            )
            .frame(width: 120, height: 48)

        } else {
            if helper.meshCurvature == 0.0 {
                Button(action: {
                    changingCurvature = true
                }) {
                    FlatButtonIcon()
                        .frame(width: 48, height: 48)
                }
                .frame(width: 48, height: 48)
            } else if helper.meshCurvature <= 0.3 {
                Button(action: {
                    changingCurvature = true
                }) {
                    CurvedButtonIcon()
                        .frame(width: 48, height: 48)
                }
                .frame(width: 48, height: 48)
            } else {
                Button(action: {
                    changingCurvature = true
                }) {
                    CurviestButtonIcon()
                        .frame(width: 48, height: 48)
                }
                .frame(width: 48, height: 48)
            }
        }
    }
}

/// Ornament view with close button for curved content mode.
@available(visionOS 26.0, *)
struct SDL_CurvedContentCloseOrnamentView: View {
    var body: some View {
        Button(action: {
            SDL_VisionOS_LeaveCurvedMode()
        }) {
            Image(systemName: "rectangle")
        }
        .frame(width: 48, height: 48)
    }
}
