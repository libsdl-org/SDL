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

/// SwiftUI scene delegate for immersive spaces using UIHostingSceneDelegate bridging.
///
/// Note: SwiftUI creates a NEW instance of this class for each scene connection.
/// All state that needs to persist across the ObjC bridge must be static.
@available(visionOS 26.0, *)
@MainActor
@objc(SDL_ImmersiveHostingSceneDelegate)
public class SDL_ImmersiveHostingSceneDelegate: NSObject, UISceneDelegate, @MainActor UIHostingSceneDelegate {

    // MARK: - Static State (shared across all instances)

    /// Shared instance for Objective-C configuration calls (curvature, texture, etc.)
    @objc public static let shared = SDL_ImmersiveHostingSceneDelegate()

    /// Helper for RealityKit mesh generation (static so rootScene can access)
    private static var helper: SDL_RealityKitHelper = SDL_RealityKitHelper()

    /// The active immersive scene session, tracked statically because SwiftUI
    /// creates a new delegate instance for each scene connection.
    @objc public static weak var activeSession: UISceneSession?

    /// Callback when scene connects. Set from ObjC before activation, fired
    /// from whichever instance SwiftUI creates for the scene.
    @objc public static var sceneConnectedCallback: ((UISceneSession) -> Void)?

    // MARK: - UIHostingSceneDelegate Protocol Requirement

    /// Root SwiftUI scene for the immersive space.
    ///
    /// Uses .mixed immersion style by default so SDL content blends with
    /// the user's real-world surroundings.
    public static var rootScene: some SwiftUI.Scene {
        ImmersiveSpace(id: "sdl-immersive") {
            SDL_ImmersiveRootView(helper: helper)
        }
    }

    // MARK: - UISceneDelegate Lifecycle

    public func scene(
        _ scene: UIScene,
        willConnectTo session: UISceneSession,
        options connectionOptions: UIScene.ConnectionOptions
    ) {
        NSLog("SDL_ImmersiveHostingSceneDelegate: scene willConnectTo session: %@", session)
        Self.activeSession = session
        Self.sceneConnectedCallback?(session)
    }

    public func sceneDidDisconnect(_ scene: UIScene) {
        NSLog("SDL_ImmersiveHostingSceneDelegate: sceneDidDisconnect")
        if Self.activeSession?.persistentIdentifier == scene.session.persistentIdentifier {
            Self.activeSession = nil
        }
    }

    // MARK: - ObjC-Callable Activation

    /// Activate the immersive scene from Objective-C code.
    @objc public static func activateScene(errorHandler: @escaping (Error) -> Void) {
        guard let request = UISceneSessionActivationRequest(
            hostingDelegateClass: SDL_ImmersiveHostingSceneDelegate.self,
            id: "sdl-immersive"
        ) else {
            let error = NSError(
                domain: "SDL",
                code: -1,
                userInfo: [NSLocalizedDescriptionKey: "No scene with id 'sdl-immersive' defined"])
            NSLog("SDL_ImmersiveHostingSceneDelegate: Failed to create activation request")
            errorHandler(error)
            return
        }

        NSLog("SDL_ImmersiveHostingSceneDelegate: Activating immersive scene via hostingDelegateClass API")
        UIApplication.shared.activateSceneSession(for: request) { error in
            NSLog("SDL_ImmersiveHostingSceneDelegate: Scene activation error: %@", error.localizedDescription)
            errorHandler(error)
        }
    }

    /// Dismiss the active immersive scene from Objective-C code.
    @objc public static func dismissScene() {
        guard let session = activeSession else {
            NSLog("SDL_ImmersiveHostingSceneDelegate: No active session to dismiss")
            return
        }

        NSLog("SDL_ImmersiveHostingSceneDelegate: Dismissing immersive scene session")

        let options = UISceneDestructionRequestOptions()
        UIApplication.shared.requestSceneSessionDestruction(session, options: options) { error in
            NSLog("SDL_ImmersiveHostingSceneDelegate: Dismiss error: %@", error.localizedDescription)
        }
        activeSession = nil
    }

    // MARK: - Configuration (Called from Objective-C SDL)

    /// Configure curvature and device before scene activation
    @objc public func configure(size: CGSize, curvature: Float) {
        NSLog("SDL_ImmersiveHostingSceneDelegate: Configuring size %gx%g and curvature %.2f", size.width, size.height, curvature)
        Self.helper.configure(width: Int(size.width), height: Int(size.height), curvature: curvature)
    }

    /// Get the display texture for this frame
    @objc public func getDisplayTexture(_ commandBuffer: MTLCommandBuffer, width: Int, height: Int, pixelFormat: MTLPixelFormat) -> MTLTexture? {
        return Self.helper.getDisplayTexture(commandBuffer, width: width, height: height, pixelFormat: pixelFormat)
    }

    /// Update window size dynamically
    @objc public func updateSize(_ size: CGSize) {
        NSLog("SDL_ImmersiveHostingSceneDelegate: Updating size to %gx%g", size.width, size.height)
        Self.helper.updateSize(width: Int(size.width), height: Int(size.height))
    }

    /// Update curvature dynamically
    @objc public func updateCurvature(_ curvature: Float) {
        NSLog("SDL_ImmersiveHostingSceneDelegate: Updating curvature to %.2f", curvature)
        Self.helper.updateCurvature(curvature: curvature)
    }
}


// Icons used by buttons below

// Close button
/* SVG:
 <svg width="800" height="800" viewBox="0 0 800 800" fill="none" xmlns="http://www.w3.org/2000/svg">
 <path d="M200 200L600 600" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 <path d="M600 200L200 600" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 </svg>
 */
struct CloseButtonIcon: Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        let width = rect.size.width
        let height = rect.size.height
        var strokePath1 = Path()
        strokePath1.move(to: CGPoint(x: 0.25*width, y: 0.25*height))
        strokePath1.addLine(to: CGPoint(x: 0.75*width, y: 0.75*height))
        path.addPath(strokePath1.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        var strokePath2 = Path()
        strokePath2.move(to: CGPoint(x: 0.75*width, y: 0.25*height))
        strokePath2.addLine(to: CGPoint(x: 0.25*width, y: 0.75*height))
        path.addPath(strokePath2.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        return path
    }
}

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

// MARK: - SwiftUI Content View

/// RealityKit content view for volumetric windows and immersive spaces
@available(visionOS 26.0, *)
struct SDL_ImmersiveRootView: View {
    let helper: SDL_RealityKitHelper
    let immersivePosition: SIMD3<Float> = SIMD3<Float>(0, 0, -1.5)
    @State private var touchScale: CGPoint = CGPoint()
    @State private var touchOffset: CGPoint = CGPoint()

    let SDL_EVENT_FINGER_DOWN: UInt32 = 0x700
    let SDL_EVENT_FINGER_UP: UInt32 = 0x701
    let SDL_EVENT_FINGER_MOTION: UInt32 = 0x702
    let SDL_EVENT_FINGER_CANCELED: UInt32 = 0x703
    private(set) static var last_fingerID: UInt64 = 0
    private(set) static var fingers: [SpatialEventCollection.Event.ID: UInt64] = [:]

    private func sendTouchEvent(event: SpatialEventCollection.Event) {
        var fingerID: UInt64
        var eventType: UInt32
        if let value = Self.fingers[event.id] {
            fingerID = value
            if (event.phase == SpatialEventCollection.Event.Phase.active) {
                eventType = SDL_EVENT_FINGER_MOTION
            } else if (event.phase == SpatialEventCollection.Event.Phase.ended) {
                eventType = SDL_EVENT_FINGER_UP
                Self.fingers.removeValue(forKey: event.id)
            } else {
                eventType = SDL_EVENT_FINGER_CANCELED
                Self.fingers.removeValue(forKey: event.id)
            }
        } else if (event.phase == SpatialEventCollection.Event.Phase.active) {
            Self.last_fingerID += 1
            fingerID = Self.last_fingerID
            Self.fingers[event.id] = fingerID
            eventType = SDL_EVENT_FINGER_DOWN
        } else {
            // An inactive event for a finger we don't know about
            return
        }
        var location = event.location
        location.x -= touchOffset.x
        location.y -= touchOffset.y
        location.x *= touchScale.x
        location.y *= touchScale.y
        SDL_VisionOS_SendImmersiveTouch(event.timestamp, fingerID, eventType, location.x, location.y)
    }

    var body: some View {
        GeometryReader3D { proxy in
            realityContent(proxy)
        }
        .frame(
            width: helper.contentSizeInPoints.width > 0 ? helper.contentSizeInPoints.width * 1.5 : nil,
            height: helper.contentSizeInPoints.height > 0 ? helper.contentSizeInPoints.height * 1.5 : nil
        )
    }

    private func realityContent(_ proxy: GeometryProxy3D) -> some View {
        RealityView { content, attachments in
            NSLog("SDL_ImmersiveRootView: RealityView setup")

        } update: { content, attachments in
            //NSLog("SDL_ImmersiveRootView: RealityView update")

            let frameInMeters: BoundingBox = content.convert(proxy.frame(in: .local), from: .local, to: .scene)
            helper.updateMeshSize(width: frameInMeters.extents.x, height: frameInMeters.extents.y)

            let entity = helper.getCurvedEntity()
            if let entity, entity.parent == nil {
                let headAnchor = AnchorEntity(.head)
                headAnchor.anchoring.trackingMode = .once
                content.add(headAnchor)

                entity.setPosition(immersivePosition, relativeTo: headAnchor)
                headAnchor.addChild(entity)

                if let attachmentEntity = attachments.entity(for: "sceneButton") {
                    attachmentEntity.setPosition(immersivePosition + SIMD3<Float>(0.0, -(frameInMeters.extents.y * 0.5), 0.1), relativeTo: headAnchor)
                    headAnchor.addChild(attachmentEntity)
                }
            }

            let frame = proxy.frame(in: .local)
            let scale = CGPoint(x: 1 / frame.size.width, y: 1 / frame.size.height)
            var translate = content.convert(point:immersivePosition, from: .scene, to: .local)
            translate.x -= (frame.max.x - frame.min.x) / 2
            translate.y -= (frame.max.y - frame.min.y) / 2
            Task {
                touchScale = scale
                touchOffset = CGPoint(x: translate.x, y: translate.y)
            }
        } attachments: {
            Attachment(id: "sceneButton") {
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
                        SDL_VisionOS_LeaveImmersiveMode()
                    }) {
                        Image(systemName: "rectangle.arrowtriangle.2.inward")
                    }
                    .frame(width: 48, height: 48)
                }
            }
        }
        .ignoresSafeArea()
        .gesture(
            SpatialEventGesture()
                .onChanged { events in
                    for event in events {
                        sendTouchEvent(event: event)
                    }
                }
                .onEnded { events in
                    for event in events {
                        sendTouchEvent(event: event)
                    }
                }
        )
    }
}
