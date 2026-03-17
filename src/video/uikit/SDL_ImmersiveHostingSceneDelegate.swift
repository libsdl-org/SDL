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

/// SwiftUI scene delegate for immersive spaces using UIHostingSceneDelegate bridging.
///
/// Parallel to SDL_VolumetricHostingSceneDelegate but declares an ImmersiveSpace
/// instead of a volumetric WindowGroup. The RealityKit content (SDL_VolumetricRootView)
/// is shared between both scene types.
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
            SDL_VolumetricRootView(helper: helper, isImmersive: true)
                .preferredSurroundingsEffect(.ultraDark)
        }
        .immersionStyle(selection: .constant(.full), in: .full)
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
    @objc public static func activateImmersiveScene(errorHandler: @escaping (Error) -> Void) {
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
    @objc public static func dismissImmersiveScene() {
        guard let session = activeSession else {
            NSLog("SDL_ImmersiveHostingSceneDelegate: No active session to dismiss")
            return
        }

        NSLog("SDL_ImmersiveHostingSceneDelegate: Dismissing immersive scene session")

        helper.reset()

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
