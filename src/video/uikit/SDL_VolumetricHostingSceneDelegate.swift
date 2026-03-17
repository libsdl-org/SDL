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

// Defined in SDL_uikitvisionosscene.m — posts SDL_EVENT_WINDOW_RESIZED when a scene is resized
@_silgen_name("SDL_VisionOS_SendWindowResized")
func SDL_VisionOS_SendWindowResized(size: CGSize)

// Defined in SDL_uikitvisionosscene.m — sends touch events to the application
@_silgen_name("SDL_VisionOS_SendVolumetricTouch")
func SDL_VisionOS_SendVolumetricTouch(timestamp: Double, fingerID: UInt64, eventType: UInt32, location: CGPoint);

// Defined in SDL_quit.c
@_silgen_name("SDL_SendQuit")
func SDL_SendQuit()

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
 <path d="M133.333 400C311.111 311.111 488.889 311.111 666.667 400" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 </svg>
 */
struct CurvedButtonIcon : Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        let width = rect.size.width
        let height = rect.size.height
        var strokePath = Path()
        strokePath.move(to: CGPoint(x: 0.16667*width, y: 0.5*height))
        strokePath.addCurve(to: CGPoint(x: 0.83333*width, y: 0.5*height), control1: CGPoint(x: 0.38889*width, y: 0.38889*height), control2: CGPoint(x: 0.61111*width, y: 0.38889*height))
        path.addPath(strokePath.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        return path
    }
}

// Minimize button
/* SVG:
 <svg width="800" height="800" viewBox="0 0 800 800" fill="none" xmlns="http://www.w3.org/2000/svg">
 <path d="M175 625L343.75 456.25M625 175L456.25 343.75" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 <path d="M211 456H344.333V589.333" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 <path d="M589.333 344.333H456V211" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 </svg>
 */
struct VolumetricButtonIcon : Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        let width = rect.size.width
        let height = rect.size.height
        var strokePath2 = Path()
        strokePath2.move(to: CGPoint(x: 0.21875*width, y: 0.78125*height))
        strokePath2.addLine(to: CGPoint(x: 0.42969*width, y: 0.57031*height))
        strokePath2.move(to: CGPoint(x: 0.78125*width, y: 0.21875*height))
        strokePath2.addLine(to: CGPoint(x: 0.57031*width, y: 0.42969*height))
        path.addPath(strokePath2.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        var strokePath4 = Path()
        strokePath4.move(to: CGPoint(x: 0.26375*width, y: 0.57*height))
        strokePath4.addLine(to: CGPoint(x: 0.43042*width, y: 0.57*height))
        strokePath4.addLine(to: CGPoint(x: 0.43042*width, y: 0.73667*height))
        path.addPath(strokePath4.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        var strokePath6 = Path()
        strokePath6.move(to: CGPoint(x: 0.73667*width, y: 0.43042*height))
        strokePath6.addLine(to: CGPoint(x: 0.57*width, y: 0.43042*height))
        strokePath6.addLine(to: CGPoint(x: 0.57*width, y: 0.26375*height))
        path.addPath(strokePath6.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        return path
    }
}

// Maximize button
/* SVG:
 <svg width="800" height="800" viewBox="0 0 800 800" fill="none" xmlns="http://www.w3.org/2000/svg">
 <path d="M175 625L343.75 456.25M625 175L456.25 343.75" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 <path d="M492 175H625.333V308.333" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 <path d="M308.333 625.333H175V492" stroke="black" stroke-width="66.6667" stroke-linecap="round" stroke-linejoin="round"/>
 </svg>
 */
struct ImmersiveButtonIcon : Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        let width = rect.size.width
        let height = rect.size.height
        var strokePath2 = Path()
        strokePath2.move(to: CGPoint(x: 0.21875*width, y: 0.78125*height))
        strokePath2.addLine(to: CGPoint(x: 0.42969*width, y: 0.57031*height))
        strokePath2.move(to: CGPoint(x: 0.78125*width, y: 0.21875*height))
        strokePath2.addLine(to: CGPoint(x: 0.57031*width, y: 0.42969*height))
        path.addPath(strokePath2.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        var strokePath4 = Path()
        strokePath4.move(to: CGPoint(x: 0.615*width, y: 0.21875*height))
        strokePath4.addLine(to: CGPoint(x: 0.78167*width, y: 0.21875*height))
        strokePath4.addLine(to: CGPoint(x: 0.78167*width, y: 0.38542*height))
        path.addPath(strokePath4.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        var strokePath6 = Path()
        strokePath6.move(to: CGPoint(x: 0.38542*width, y: 0.78167*height))
        strokePath6.addLine(to: CGPoint(x: 0.21875*width, y: 0.78167*height))
        strokePath6.addLine(to: CGPoint(x: 0.21875*width, y: 0.615*height))
        path.addPath(strokePath6.strokedPath(StrokeStyle(lineWidth: 0.08333*width, lineCap: .round, lineJoin: .round, miterLimit: 4)))
        return path
    }
}

/// SwiftUI scene delegate for volumetric windows using UIHostingSceneDelegate bridging.
///
/// Conforms to UIHostingSceneDelegate to declare a SwiftUI Scene that SwiftUI's
/// internal AppGraph can discover. This prevents the visionOS fatalError that occurs
/// when SwiftUI intercepts a volumetric scene connection but finds no matching scene
/// in the app body.
///
/// Note: SwiftUI creates a NEW instance of this class for each scene connection.
/// All state that needs to persist across the ObjC bridge must be static.
///
/// Architecture:
/// - Main app: UIKit lifecycle (e.g., Qt's QIOSApplicationDelegate)
/// - Volumetric Scene: SwiftUI via UIHostingSceneDelegate bridging (this class)
/// - Activation: UISceneSessionActivationRequest(hostingDelegateClass:) from ObjC
@available(visionOS 26.0, *)
@MainActor
@objc(SDL_VolumetricHostingSceneDelegate)
public class SDL_VolumetricHostingSceneDelegate: NSObject, UIWindowSceneDelegate, @MainActor UIHostingSceneDelegate {

    // MARK: - Static State (shared across all instances)

    /// Shared instance for Objective-C configuration calls (curvature, texture, etc.)
    @objc public static let shared = SDL_VolumetricHostingSceneDelegate()

    /// Helper for RealityKit mesh generation (static so rootScene can access)
    static var helper: SDL_RealityKitHelper = SDL_RealityKitHelper()

    /// Separate helper for immersive mode, cloned from volumetric helper
    /// on transition so each scene has its own entity and can be configured
    /// independently (e.g., different curvature).
    static var immersiveHelper: SDL_RealityKitHelper = SDL_RealityKitHelper()

    /// Tracks whether immersive mode is active so texture updates are
    /// routed to the correct helper.
    static var immersiveActive: Bool = false

    /// Set to true when transitioning between scenes (volumetric to immersive).
    /// Consumed by scene(_:willConnectTo:) when the new scene connects.
    /// Prevents the background observer from posting SDL_EVENT_QUIT during transitions.
    static var isTransitioningScene: Bool = false

    /// The active volumetric scene session, tracked statically because SwiftUI
    /// creates a new delegate instance for each scene connection.
    @objc public static var activeSession: UISceneSession?

    /// Callback when scene connects. Set from ObjC before activation, fired
    /// from whichever instance SwiftUI creates for the scene.
    @objc public static var sceneConnectedCallback: ((UISceneSession) -> Void)?

    /// Notification observer for app backgrounding. This is the primary mechanism
    /// for detecting when the user closes a reactivated volumetric scene, since
    /// UIHostingSceneDelegate-managed scenes don't reliably fire sceneDidDisconnect
    /// or UIScene.didDisconnectNotification after reactivation.
    static var backgroundObserver: NSObjectProtocol?

    // MARK: - UIHostingSceneDelegate Protocol Requirement

    /// Root SwiftUI scene for the volumetric window.
    ///
    /// When activated via UISceneSessionActivationRequest(hostingDelegateClass:),
    /// SwiftUI registers this scene in the AppGraph so the volumetric role has a
    /// matching scene item. The WindowGroup id must match what's passed to the
    /// activation request.
    public static var rootScene: some SwiftUI.Scene {
        WindowGroup(id: "sdl-volumetric") {
            SDL_VolumetricRootView(helper: helper)
        }
        .windowStyle(.volumetric)
        .windowResizability(.contentSize)
        .defaultSize(width: 16.0 / 9.0, height: 1.0, depth: 0.25, in: .meters)
    }

    // MARK: - UIWindowSceneDelegate Lifecycle

    public func scene(
        _ scene: UIScene,
        willConnectTo session: UISceneSession,
        options connectionOptions: UIScene.ConnectionOptions
    ) {
        NSLog("SDL_VolumetricHostingSceneDelegate: scene willConnectTo session: \(session) (instance: \(Unmanaged.passUnretained(self).toOpaque()))")
        Self.activeSession = session
        Self.sceneConnectedCallback?(session)

        // Transition is complete when a new scene connects
        if Self.isTransitioningScene {
            Self.isTransitioningScene = false
        }

        // Register background observer to detect when the user closes the
        // volumetric window. When all foreground scenes are gone, the app
        // enters background — we post SDL_EVENT_QUIT unless we're mid-transition.
        if let oldBg = Self.backgroundObserver {
            NotificationCenter.default.removeObserver(oldBg)
        }
        Self.backgroundObserver = NotificationCenter.default.addObserver(
            forName: UIApplication.didEnterBackgroundNotification,
            object: nil,
            queue: .main
        ) { _ in
            NSLog("SDL_VolumetricHostingSceneDelegate: didEnterBackground (isTransitioning=%d)",
                  Self.isTransitioningScene ? 1 : 0)
            if !Self.isTransitioningScene {
                NSLog("SDL_VolumetricHostingSceneDelegate: not transitioning")
                Self.activeSession = nil
                if let obs = Self.backgroundObserver {
                    NotificationCenter.default.removeObserver(obs)
                    Self.backgroundObserver = nil
                }
            }
        }
    }

    public func sceneDidDisconnect(_ scene: UIScene) {
        NSLog("SDL_VolumetricHostingSceneDelegate: sceneDidDisconnect scene=\(scene.session.persistentIdentifier)")

        // Remove background observer to prevent double-quit
        if let obs = Self.backgroundObserver {
            NotificationCenter.default.removeObserver(obs)
            Self.backgroundObserver = nil
        }

        Self.activeSession = nil

        if Self.isTransitioningScene {
            NSLog("SDL_VolumetricHostingSceneDelegate: transitioning scene, skipping SDL_EVENT_QUIT")
            Self.isTransitioningScene = false
        } else {
            NSLog("SDL_VolumetricHostingSceneDelegate: posting SDL_EVENT_QUIT")
            SDL_SendQuit();
        }
    }

    // MARK: - ObjC-Callable Activation

    /// Activate the volumetric scene from Objective-C code.
    ///
    /// This wraps the Swift-generic UISceneSessionActivationRequest(hostingDelegateClass:)
    /// API so it can be called via NSInvocation from SDL's ObjC layer.
    @objc public static func activateVolumetricScene(errorHandler: @escaping (Error) -> Void) {
        guard let request = UISceneSessionActivationRequest(
            hostingDelegateClass: SDL_VolumetricHostingSceneDelegate.self,
            id: "sdl-volumetric"
        ) else {
            let error = NSError(
                domain: "SDL",
                code: -1,
                userInfo: [NSLocalizedDescriptionKey: "No scene with id 'sdl-volumetric' defined"])
            NSLog("SDL_VolumetricHostingSceneDelegate: Failed to create activation request")
            errorHandler(error)
            return
        }

        NSLog("SDL_VolumetricHostingSceneDelegate: Activating volumetric scene via hostingDelegateClass API")
        UIApplication.shared.activateSceneSession(for: request) { error in
            NSLog("SDL_VolumetricHostingSceneDelegate: Scene activation error: %@", error.localizedDescription)
            errorHandler(error)
        }
    }

    /// Dismiss the active volumetric scene from Objective-C code.
    @objc public static func dismissVolumetricScene() {
        guard let session = activeSession else {
            NSLog("SDL_VolumetricHostingSceneDelegate: No active session to dismiss")
            return
        }

        NSLog("SDL_VolumetricHostingSceneDelegate: Dismissing volumetric scene session")

        // Reset helper state so the next activation starts fresh
        helper.reset()

        let options = UISceneDestructionRequestOptions()
        UIApplication.shared.requestSceneSessionDestruction(session, options: options) { error in
            NSLog("SDL_VolumetricHostingSceneDelegate: Dismiss error: %@", error.localizedDescription)
        }
        activeSession = nil
    }

    /// Dismiss the volumetric scene for transition to immersive.
    /// Does not reset the helper since we may return to volumetric.
    static func dismissVolumetricSceneForTransition() {
        guard let session = activeSession else {
            NSLog("SDL_VolumetricHostingSceneDelegate: No active session to dismiss for transition")
            return
        }

        NSLog("SDL_VolumetricHostingSceneDelegate: Dismissing volumetric scene for immersive transition")

        let options = UISceneDestructionRequestOptions()
        UIApplication.shared.requestSceneSessionDestruction(session, options: options) { error in
            NSLog("SDL_VolumetricHostingSceneDelegate: Transition dismiss error: %@", error.localizedDescription)
        }
        activeSession = nil
    }

    // MARK: - Configuration (Called from Objective-C SDL)

    /// Configure size and curvature before scene activation
    @objc public func configure(size: CGSize, curvature: Float) {
        NSLog("SDL_VolumetricHostingSceneDelegate: Configuring size %gx%g and curvature %.2f", size.width, size.height, curvature)
        Self.helper.configure(width: Int(size.width), height: Int(size.height), curvature: curvature)
    }

    /// Get the display texture for this frame
    @objc public func getDisplayTexture(_ commandBuffer: MTLCommandBuffer, width: Int, height: Int, pixelFormat: MTLPixelFormat) -> MTLTexture? {
        if Self.immersiveActive {
            return SDL_ImmersiveHostingSceneDelegate.shared.getDisplayTexture(commandBuffer, width: width, height: height, pixelFormat: pixelFormat)
        } else {
            return Self.helper.getDisplayTexture(commandBuffer, width: width, height: height, pixelFormat: pixelFormat)
        }
    }

    /// Update window size dynamically
    @objc public func updateSize(_ size: CGSize) {
        NSLog("SDL_VolumetricHostingSceneDelegate: Updating size to %gx%g", size.width, size.height)
        Self.helper.updateSize(width: Int(size.width), height: Int(size.height))
    }

    /// Update curvature dynamically
    @objc public func updateCurvature(_ curvature: Float) {
        NSLog("SDL_VolumetricHostingSceneDelegate: Updating curvature to %.2f", curvature)
        Self.helper.updateCurvature(curvature: curvature)
    }
}

// MARK: - SwiftUI Content View

/// RealityKit content view for volumetric windows and immersive spaces
@available(visionOS 26.0, *)
struct SDL_VolumetricRootView: View {
    let helper: SDL_RealityKitHelper
    var isImmersive: Bool = false
    @Environment(\.dismissImmersiveSpace) private var dismissImmersiveSpace
    @Environment(\.physicalMetrics) private var metrics: PhysicalMetricsConverter

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
        SDL_VisionOS_SendVolumetricTouch(timestamp: event.timestamp, fingerID: fingerID, eventType: eventType, location: event.location)
    }

    var body: some View {
        GeometryReader3D { proxy in
            realityContent
                .onChange(of: proxy.size) { _, newSize in
                    guard !isImmersive else { return }
                    NSLog("SDL_VolumetricRootView: size changed to %g,%g", newSize.width, newSize.height);
                    SDL_VolumetricHostingSceneDelegate.immersiveActive = false;
                    let size = CGSize(width: newSize.width, height: newSize.height);
                    SDL_ImmersiveHostingSceneDelegate.shared.updateSize(size);
                    SDL_VisionOS_SendWindowResized(size: size);
                }
        }
        .frame(
            //idealWidth: helper.contentSizeInPoints.width > 0 ? helper.contentSizeInPoints.width : nil,
            //idealHeight: helper.contentSizeInPoints.height > 0 ? helper.contentSizeInPoints.height : nil
            width: helper.contentSizeInPoints.width > 0 ? helper.contentSizeInPoints.width : nil,
            height: helper.contentSizeInPoints.height > 0 ? helper.contentSizeInPoints.height : nil
        )
    }

    private var realityContent: some View {
        RealityView { content, attachments in
            NSLog("SDL_VolumetricRootView: RealityView setup (immersive=%d)", isImmersive ? 1 : 0)

            if let attachmentEntity = attachments.entity(for: "sceneButton") {
                if isImmersive {
                    attachmentEntity.position = SIMD3<Float>(0.0, 1.0, -1.4)
                } else {
                    let buttonHeight = Float(metrics.convert(80.0, to: .meters))
                    attachmentEntity.position = SIMD3<Float>(0, -(helper.meshHeight * 0.5) + buttonHeight * 0.5, 0.24 * 0.5)
                }
                content.add(attachmentEntity)
                NSLog("SDL_VolumetricRootView: Added button attachment (immersive=%d)", isImmersive ? 1 : 0)
            }

            if !isImmersive {
                NotificationCenter.default.post(name: NSNotification.Name("SDLVolumetricSceneReady"), object: nil)
            }
        } update: { content, attachments in
            let entity = helper.getCurvedEntity()
            if let entity, entity.parent == nil {
                if isImmersive {
                    entity.position = SIMD3<Float>(0, 1.5, -1.5)
                } else {
                    entity.position = .zero
                }
                content.add(entity)
            }
        } attachments: {
            Attachment(id: "sceneButton") {
                HStack(spacing: 32) {
                    if isImmersive {
                        Button(action: {
                            SDL_SendQuit()
                        }) {
                            Label {
                                Text("Close")
                            } icon: {
                                CloseButtonIcon()
                            }
                        }
                        .frame(width: 80, height: 80)
                        .glassBackgroundEffect()
                        .labelStyle(.iconOnly)
                    }
                    if helper.isCurved() {
                        Button(action: {
                            helper.updateCurvature(curvature: 0.0)
                        }) {
                            Label {
                                Text("Flat")
                            } icon: {
                                FlatButtonIcon()
                            }
                        }
                        .frame(width: 80, height: 80)
                        .glassBackgroundEffect()
                        .labelStyle(.iconOnly)
                    } else {
                        Button(action: {
                            helper.updateCurvature(curvature: 0.2)
                        }) {
                            Label {
                                Text("Curved")
                            } icon: {
                                CurvedButtonIcon()
                            }
                        }
                        .frame(width: 80, height: 80)
                        .glassBackgroundEffect()
                        .labelStyle(.iconOnly)
                    }
                    if isImmersive {
                        Button(action: {
                            Task {
                                SDL_VolumetricHostingSceneDelegate.immersiveActive = false
                                SDL_VolumetricHostingSceneDelegate.immersiveHelper.reset()
                                // Reopen the volumetric scene before dismissing immersive
                                // so the app always has a foreground scene.
                                SDL_VolumetricHostingSceneDelegate.activateVolumetricScene { error in
                                    NSLog("SDL_VolumetricHostingSceneDelegate: Failed to reactivate volumetric: %@",
                                          error.localizedDescription)
                                }
                                await dismissImmersiveSpace()
                            }
                        }) {
                            Label {
                                Text("Volumetric")
                            } icon: {
                                VolumetricButtonIcon()
                            }
                        }
                        .frame(width: 80, height: 80)
                        .glassBackgroundEffect()
                        .labelStyle(.iconOnly)
                    } else {
                        Button(action: {
                            // Configure the immersive delegate's helper with current settings
                            SDL_ImmersiveHostingSceneDelegate.shared.configure(
                                size: SDL_VolumetricHostingSceneDelegate.helper.contentSizeInPoints,
                                curvature: SDL_VolumetricHostingSceneDelegate.helper.meshCurvature
                            )
                            SDL_VolumetricHostingSceneDelegate.immersiveActive = true
                            SDL_VolumetricHostingSceneDelegate.isTransitioningScene = true
                            SDL_VolumetricHostingSceneDelegate.dismissVolumetricSceneForTransition()
                            SDL_ImmersiveHostingSceneDelegate.activateImmersiveScene { error in
                                NSLog("SDL_VolumetricHostingSceneDelegate: Failed to activate immersive: %@",
                                      error.localizedDescription)
                            }
                        }) {
                            Label {
                                Text("Immersive")
                            } icon: {
                                ImmersiveButtonIcon()
                            }
                        }
                        .frame(width: 80, height: 80)
                        .glassBackgroundEffect()
                        .labelStyle(.iconOnly)
                    }
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
