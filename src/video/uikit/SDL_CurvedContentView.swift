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

/// SwiftUI view that presents SDL content on a curved RealityKit mesh
/// inside a UIHostingController (no ImmersiveSpace required).
@available(visionOS 26.0, *)
struct SDL_CurvedContentView: View {
    let helper: SDL_RealityKitHelper

    @State private var touchScale: CGPoint = CGPoint()
    @State private var addedEntity: ModelEntity?

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
        var location = event.location
        location.x *= touchScale.x
        location.y *= touchScale.y
        SDL_VisionOS_SendImmersiveTouch(event.timestamp, fingerID, eventType, location.x, location.y)
    }

    var body: some View {
        GeometryReader3D { proxy in
            realityContent(proxy)
                .glassBackgroundEffect(displayMode: .never)
        }
    }

    private func realityContent(_ proxy: GeometryProxy3D) -> some View {
        RealityView { content in
            NSLog("SDL_CurvedContentView: RealityView setup")

            let frameInMeters: BoundingBox = content.convert(proxy.frame(in: .local), from: .local, to: .scene)
            helper.updateMeshSize(width: frameInMeters.extents.x, height: frameInMeters.extents.y)

        } update: { content in
            let frameInMeters: BoundingBox = content.convert(proxy.frame(in: .local), from: .local, to: .scene)
            helper.updateMeshSize(width: frameInMeters.extents.x, height: frameInMeters.extents.y)

            // The entity is created asynchronously by updateMeshSize.
            // Each time the helper changes (@Observable), this update closure runs.
            // Add the entity to content when it becomes available, or when it changes.
            if let entity = helper.getCurvedEntity(), entity !== addedEntity {
                addedEntity?.removeFromParent()
                entity.position = SIMD3<Float>(0, 0, -0.2)
                content.add(entity)
                Task {
                    addedEntity = entity
                }
                NSLog("SDL_CurvedContentView: Added entity at Z=-0.2, meshWidth=%.3f meshHeight=%.3f curvature=%.3f",
                      helper.meshWidth, helper.meshHeight, helper.meshCurvature)
            }

            // Compensate for the curve in the touch event location
            let posX = 0.5 * frameInMeters.extents.x
            let curveAmount = helper.meshCurvature * 2.0
            let angle = posX * curveAmount
            let radius = 1.0 / max(curveAmount, 0.001)
            let adjustedX = radius * sin(angle)
            let ratio = CGFloat(adjustedX > 0 ? (adjustedX / posX): 1.0)
            let a = (1.0 - ratio) * 0.5
            let curveScale = 1.0 - a

            let frame = proxy.frame(in: .local)
            let scale = CGPoint(x: 1 / (frame.size.width * curveScale), y: 1 / frame.size.height)

            Task {
                touchScale = scale
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
