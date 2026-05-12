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
import GameController

/// SwiftUI view that presents SDL content on a curved RealityKit mesh
/// inside a UIHostingController
internal struct SDL_CurvedContentView: View {
    /// Helper object used to manage the mesh and texture of the curved UI.
    let helper: SDL_RealityKitHelper

    /// Settings object provided by the caller which determines the UI state.
    let settings: SDL_CurvedContentSettings

    /// Information about the window snap status
    @Environment(\.surfaceSnappingInfo) private var snappedStatus

    /// Closure which is called when the content is ready to present.
    let onContentReady: @MainActor () -> Void

    /// RealityKit entity which is created on appear, to be populated by the curved UI content.
    @State private var curvedUIEntity: ModelEntity! = nil

    /// Curved UI material which is created on appear.  Holds the compiled shader and material parameters.
    @State private var curvedUIMaterial: CurvedUIMaterial! = nil

    /// Converts SwiftUI points to meters (RealityKit coordinates)
    ///
    /// - Note: This conversion varies depending on the physical distance between the window and the user.
    @PhysicalMetric(from: .meters) private var pointsPerMeter: Float = 1

    /// Inverse of ``pointsPerMeter``.
    var metersPerPoint: Float { 1.0 / pointsPerMeter }

    /// The cursor color which should be passed to `curvedUIMaterial`
    @State private var cursorColor: UIColor = .lightGray

    /// The cursor color on interact (pinch/drag/click) which should be passed to `curvedUIMaterial`
    @State private var cursorColorOnInteract: UIColor = .systemCyan

    /// Whether to show the cursor overlay on the mesh surface.
    private var showCursor: Bool {
        return !mouseInputEnabled && settings.showHover
    }

    /// Whether mouse input is enabled.  When this is the case, the collision shape for indirect input should be disabled.
    private var mouseInputEnabled: Bool {
        return settings.inputType == .pointer
    }

    private var shouldPopulateCollisionShape: Bool {
        return curvedUIEntity != nil && helper.collisionShape != nil && !mouseInputEnabled
    }

    /// Value use to animate the screen radius
    @State private var animatedScreenRadius: Float = 1010

    let SDL_EVENT_FINGER_DOWN: UInt32 = 0x700
    let SDL_EVENT_FINGER_UP: UInt32 = 0x701
    let SDL_EVENT_FINGER_MOTION: UInt32 = 0x702
    let SDL_EVENT_FINGER_CANCELED: UInt32 = 0x703
    private(set) static var last_fingerID: UInt64 = 0
    private(set) static var fingers: [SpatialEventCollection.Event.ID: UInt64] = [:]

    private func sendTouchEvent(event: SpatialEventCollection.Event, proxy: GeometryProxy3D) {
        var fingerID: UInt64
        var eventType: UInt32
        if let value = Self.fingers[event.id] {
            fingerID = value
            if event.phase == SpatialEventCollection.Event.Phase.active {
                eventType = SDL_EVENT_FINGER_MOTION
            } else if event.phase == SpatialEventCollection.Event.Phase.ended {
                eventType = SDL_EVENT_FINGER_UP
                Self.fingers.removeValue(forKey: event.id)
            } else {
                eventType = SDL_EVENT_FINGER_CANCELED
                Self.fingers.removeValue(forKey: event.id)
            }
        } else if event.phase == SpatialEventCollection.Event.Phase.active {
            Self.last_fingerID += 1
            fingerID = Self.last_fingerID
            Self.fingers[event.id] = fingerID
            eventType = SDL_EVENT_FINGER_DOWN
        } else {
            return
        }

        let loc = Point3D(x: event.location3D.x - proxy.size.width / 2,
                          y: event.location3D.y - proxy.size.height / 2,
                          z: event.location3D.z - proxy.size.depth / 2)
        let meshPos = SIMD3<Float>(Float(loc.x) * metersPerPoint,
                                   Float(loc.y) * metersPerPoint,
                                   Float(loc.z) * metersPerPoint)
        let uv = helper.meshGeometry.normalizedUV(fromMeshPosition: meshPos)

        SDL_VisionOS_SendTouch(event.timestamp, fingerID, eventType, uv.x, uv.y)
    }

    var body: some View {
        GeometryReader3D { proxy in
            realityContent(proxy)
                .glassBackgroundEffect(displayMode: .never)
        }
    }

    private func realityContent(_ proxy: GeometryProxy3D) -> some View {
        RealityView { content in
            //NSLog("SDL_CurvedContentView: RealityView setup")

            let frameInMeters: BoundingBox = content.convert(proxy.frame(in: .local), from: .local, to: .scene)
            helper.updateMeshSize(width: frameInMeters.extents.x, height: frameInMeters.extents.y)

            // Compile curved UI shader (may take a while)
            let material = try! await CurvedUIMaterial()
            self.curvedUIMaterial = material

            // Create RealityKit Entity to host the curved UI content
            let mesh = try! await MeshResource(from: helper.lowLevelMesh)
            let entity = ModelEntity(mesh: mesh, materials: [material.shaderGraphMaterial])

            // Add InputTargetComponent to the mesh to accept input.
            entity.components.set(InputTargetComponent(allowedInputTypes: .all))

            // Add HoverEffectComponent to visualize the gaze target
            let shaderInputs = HoverEffectComponent.ShaderHoverEffectInputs.default
            let hoverEffect = HoverEffectComponent.HoverEffect.shader(shaderInputs)
            let hoverEffectComponent = HoverEffectComponent(hoverEffect)
            entity.components.set(hoverEffectComponent)

            // Increase the responsiveness of the hover effect
            RenderRefreshSystem.registerSystem()
            entity.components.set(RenderRefreshComponent(
                componentToRefresh: hoverEffectComponent
            ))

            self.curvedUIEntity = entity
            content.add(entity)

            // Call the user-provided contentReady closure.
            onContentReady()
        } update: { content in
            let frameInMeters: BoundingBox = content.convert(proxy.frame(in: .local), from: .local, to: .scene)
            helper.updateMeshSize(width: frameInMeters.extents.x, height: frameInMeters.extents.y)

            let frame = proxy.frame(in: .local)
            SDL_VisionOS_SendSizeChanged(Int(frame.size.width), Int(frame.size.height))
        }
        .overlay {
            if mouseInputEnabled {
                // This enables mouse motion events, but blocks hover location
                Color.white
                    .opacity(0.001)
                    .pointerStyle(.shape(Circle(), size: .zero))
            }
        }
        .gesture(
            SpatialEventGesture()
                .onChanged { events in
                    guard curvedUIMaterial != nil else { return }

                    if !mouseInputEnabled {
                        curvedUIMaterial.isInteracting = true

                        for event in events {
                            if event.kind != .pointer {
                                sendTouchEvent(event: event, proxy: proxy)
                            } else {
                                settings.inputType = .pointer
                                settings.sceneState = .cinematic
                            }
                        }
                    }
                }
                .onEnded { events in
                    guard curvedUIMaterial != nil else { return }

                    if !mouseInputEnabled {
                        for event in events {
                            if event.kind != .pointer {
                                sendTouchEvent(event: event, proxy: proxy)
                            }
                        }
                    } else {
                        for event in events {
                            if event.kind != .pointer {
                                settings.inputType = .eyes
                                settings.sceneState = .interactive
                            }
                        }
                    }

                    curvedUIMaterial.isInteracting = false
                }
        )
        .onChange(of: sceneActivationOrObject(showCursor), initial: true) {
            curvedUIMaterial?.showCursor = showCursor
        }
        .onChange(of: sceneActivationOrObject(cursorColor), initial: true) {
            curvedUIMaterial?.cursorColor = cursorColor
        }
        .onChange(of: sceneActivationOrObject(cursorColorOnInteract), initial: true) {
            curvedUIMaterial?.cursorColorOnInteract = cursorColorOnInteract
        }
        .onChange(of: sceneActivationOrObject(helper.meshGeometry), initial: true) {
            guard curvedUIMaterial != nil else { return }
            let geometry = helper.meshGeometry
            curvedUIMaterial.cursorSize = geometry.height * 0.01
        }
        .onChange(of: sceneActivationOrObject(helper.textureResource), initial: true) {
            if let textureResource = helper.textureResource {
                curvedUIMaterial?.gameTexture = textureResource
            }
        }
        .onChange(of: sceneActivationOrObject(curvedUIMaterial), initial: true) {
            // Update the materials array of the entity with the updated material parameters.
            if let curvedUIMaterial, let curvedUIEntity {
                curvedUIEntity.model!.materials = [curvedUIMaterial.shaderGraphMaterial]
            }
        }
        .onChange(of: settings.inputType, initial: true) { oldInputType, inputType in
            if inputType == .pointer {
                SDL_VisionOS_SendPointerMode(true)
            } else {
                SDL_VisionOS_SendPointerMode(false)
            }
        }
        .onChange(of: settings.curvatureRadius, initial: true) { oldRadius, curvatureRadius in
            if oldRadius != curvatureRadius {
                withAnimation(.smooth) {
                    if curvatureRadius > 0 {
                        animatedScreenRadius = curvatureRadius / 1000
                    } else {
                        animatedScreenRadius = AnimatedCurveRadiusModifier.assumedFlatThreshold + 0.01
                    }
                }
            } else {
                if curvatureRadius > 0 {
                    animatedScreenRadius = curvatureRadius / 1000
                } else {
                    animatedScreenRadius = AnimatedCurveRadiusModifier.assumedFlatThreshold + 0.01
                }
            }
        }
        .modifier(AnimatedCurveRadiusModifier(helper: helper, curveRadius: animatedScreenRadius))
        .onChange(of: sceneActivationOrObject(shouldPopulateCollisionShape ? helper.collisionShape : nil)) {
            guard let curvedUIEntity else { return }
            if let shape = helper.collisionShape, shouldPopulateCollisionShape {
                curvedUIEntity.components.set(CollisionComponent(shapes: [shape]))
            } else {
                curvedUIEntity.components.set(CollisionComponent(shapes: []))
            }
        }
        .onChange(of: snappedStatus) {
            settings.isSnapped = snappedStatus.isSnapped
            helper.updateSnappedStatus(snapped: snappedStatus.isSnapped)
        }
        .preferredSurroundingsEffect(settings.isDimmed ? .dark : nil)
        .frame(depth: 0)
        .ignoresSafeArea()
        .persistentSystemOverlays(settings.sceneState == .cinematic ? .hidden : .automatic)
        .handlesGameControllerEvents(matching: .gamepad)
    }
}

// MARK: Animating the curve radius

@Animatable
private struct AnimatedCurveRadiusModifier: @MainActor ViewModifier {
    /// Curvature radius beyond which we assume it is flat.
    static let assumedFlatThreshold: Float = 30.0

    /// Helper object to modify
    let helper: SDL_RealityKitHelper

    /// Curve radius > `assumedFlatThreshold` meters is assumed to be flat.
    var curveRadius: Float

    func body(content: Content) -> some View {
        content.onChange(of: curveRadius, initial: true) {
            if curveRadius > 10 {
                helper.updateMeshCurvature(curvatureRadius: 0)
            } else {
                helper.updateMeshCurvature(curvatureRadius: curveRadius)
            }
        }
    }
}

// MARK: Bridging SwiftUI and RealityKit

private extension SDL_CurvedContentView {
    private struct Box<T: Equatable>: Equatable {
        var sceneActivation: Bool
        var value: T
    }

    /// Convenience function which triggers an `onChange` event either when `object` changes, or when
    /// ``curvedUIMaterial`` finishes compiling.
    func sceneActivationOrObject<T: Equatable>(_ object: T) -> some Equatable {
        return Box(sceneActivation: self.curvedUIMaterial != nil && self.curvedUIEntity != nil, value: object)
    }
}

// MARK: Per-frame component refresh

/// Attach this component to an entity to reset a RealityKit component every rendering frame.
/// This can be used to disable system-default interpolation on any component that applies it.
///
/// Example — to reset a platform-specific component every frame:
///     entity.components.set(RenderRefreshComponent(
///         componentToRefresh: CustomComponent()
///     ))
private struct RenderRefreshComponent: TransientComponent {
    var componentToRefresh: (any Component)?
}

private struct RenderRefreshSystem: System {
    static let query = EntityQuery(where: .has(RenderRefreshComponent.self))
    init(scene: RealityKit.Scene) {
        RenderRefreshComponent.registerComponent()
    }

    func update(context: SceneUpdateContext) {
        for entity in context.entities(matching: Self.query, updatingSystemWhen: .rendering) {
            guard let refresh = entity.components[RenderRefreshComponent.self],
                  let component = refresh.componentToRefresh else { continue }
            entity.components.remove(type(of: component))
            entity.components.set(component)
        }
    }
}
