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
import RealityKit
import SwiftUI
import Metal
import MetalKit
import ModelIO
import simd

/// Swift wrapper for RealityKit functionality to be called from Objective-C
///
/// This helper provides the core RealityKit mesh generation and texture update functionality.
/// It's used by SDL_VolumetricHostingSceneDelegate which hosts the SwiftUI/RealityKit content.
///
/// Architecture:
/// SDL (ObjC) → SDL_VolumetricHostingSceneDelegate (Swift) → SDL_RealityKitHelper (Swift) → RealityKit
///
/// Key responsibilities:
/// - Generate curved mesh geometry procedurally (CPU-based)
/// - Update textures using LowLevelTexture for efficient Metal → RealityKit transfer
/// - Update materials with SDL-rendered content
/// - Provide entity for RealityView to display
@available(visionOS 2.0, *)
@MainActor
@Observable
@objc(SDL_RealityKitHelper)
public class SDL_RealityKitHelper: NSObject {

    private var curvedEntity: ModelEntity?
    private(set) var meshWidth: Float = 16.0 / 9.0
    private(set) var meshHeight: Float = 1.0
    private(set) var meshCurvature: Float = 0.0

    /// Content size in points, used by SwiftUI to drive .windowResizability(.contentSize)
    var contentSizeInPoints: CGSize = .zero

    // LowLevelTexture pipeline for efficient Metal → RealityKit texture transfer
    private var lowLevelTexture: LowLevelTexture?
    private var textureResource: TextureResource?
    private var textureWidth: Int = 0
    private var textureHeight: Int = 0

    @objc public override init() {
        super.init()
    }

    /// Resets all state so the helper can be reused for a new volumetric scene session.
    /// Called when the volumetric scene is dismissed.
    @objc public func reset() {
        NSLog("SDL_RealityKitHelper: Resetting state for next session")
        curvedEntity?.removeFromParent()
        curvedEntity = nil
        lowLevelTexture = nil
        textureResource = nil
        textureWidth = 0
        textureHeight = 0
        meshWidth = 16.0 / 9.0
        meshHeight = 1.0
        meshCurvature = 0.0
    }

    // MARK: - Mesh Generation

    /// Creates a plane entity for displaying SDL content with optional curvature
    private func createCurvedMesh(width: Float, height: Float, curvature: Float) async {
        self.meshWidth = width
        self.meshHeight = height
        self.meshCurvature = max(0.0, min(1.0, curvature))

        NSLog("SDL_RealityKitHelper: Creating display plane %.2fx%.2f (curvature %.2f)",
              self.meshWidth, self.meshHeight, self.meshCurvature)

        // Generate curved mesh geometry using LowLevelMesh
        let planeMesh: MeshResource
        let needsRotation: Bool

        if self.meshCurvature > 0.01 {
            // Create curved mesh procedurally (already in correct orientation)
            planeMesh = generateCurvedPlaneMesh(width: self.meshWidth, height: self.meshHeight, curvature: self.meshCurvature)
            needsRotation = false  // Curved mesh is generated facing viewer already
            NSLog("SDL_RealityKitHelper: Created curved mesh geometry")
        } else {
            // Use flat plane for zero curvature (needs rotation)
            planeMesh = MeshResource.generatePlane(width: self.meshWidth, depth: self.meshHeight)
            needsRotation = true  // Flat plane needs 90° rotation
            NSLog("SDL_RealityKitHelper: Using flat plane")
        }

        // Use SimpleMaterial (CustomMaterial not supported in volumetric windows)
        let material = SimpleMaterial(color: .white, isMetallic: false)

        guard let shape = try? await ShapeResource.generateStaticMesh(from: planeMesh) else {
            NSLog("SDL_RealityKitHelper: couldn't create static mesh")
            return
        }

        let entity = ModelEntity(mesh: planeMesh, materials: [material])
        entity.position = SIMD3<Float>(0, 0, 0)
        entity.collision = CollisionComponent(shapes: [shape])
        entity.components.set(InputTargetComponent(allowedInputTypes: .all))

        // Only rotate flat plane (curved mesh is already in correct orientation)
        if needsRotation {
            entity.orientation = simd_quatf(angle: .pi / 2, axis: SIMD3<Float>(1, 0, 0))
        }

        // Increase bounds margin for curved mesh
        if var modelComponent = entity.components[ModelComponent.self] {
            modelComponent.boundsMargin = 0.5
            entity.components[ModelComponent.self] = modelComponent
        }

        curvedEntity?.removeFromParent()
        self.curvedEntity = entity

        NSLog("SDL_RealityKitHelper: Created plane entity %.2fx%.2f meters", self.meshWidth, self.meshHeight)
    }

    /// Generates a curved plane mesh with cylindrical curvature
    /// Mesh is generated in XY plane with Z varying based on X for horizontal curvature
    private func generateCurvedPlaneMesh(width: Float, height: Float, curvature: Float) -> MeshResource {
        let segmentsX = 32
        let segmentsY = 32

        var positions: [SIMD3<Float>] = []
        var normals: [SIMD3<Float>] = []
        var uvs: [SIMD2<Float>] = []
        var indices: [UInt32] = []

        // Generate vertices with cylindrical curvature
        // X = width (left/right), Y = height (up/down), Z = depth (toward/away from viewer)
        for y in 0...segmentsY {
            let v = Float(y) / Float(segmentsY)
            let posY = (v - 0.5) * height  // Height stays vertical

            for x in 0...segmentsX {
                let u = Float(x) / Float(segmentsX)
                let posX = (u - 0.5) * width  // Width horizontal

                // Apply cylindrical curve: Z varies with X to create wrap-around
                let curveAmount = curvature * 2.0
                let angle = posX * curveAmount
                let radius = 1.0 / max(curveAmount, 0.001)

                // Z offset: center at 0, edges forward (positive Z) to wrap around viewer
                let posZ = radius * (1.0 - cos(angle))
                let adjustedX = radius * sin(angle)

                positions.append(SIMD3<Float>(adjustedX, posY, posZ))

                // Normal points away from curve center (toward viewer for convex curve)
                let normalX = sin(angle)
                let normalZ = -cos(angle)
                normals.append(normalize(SIMD3<Float>(normalX, 0, normalZ)))

                uvs.append(SIMD2<Float>(u, v))
            }
        }

        // Generate triangle indices (reversed winding for correct front face)
        for y in 0..<segmentsY {
            for x in 0..<segmentsX {
                let i0 = UInt32(y * (segmentsX + 1) + x)
                let i1 = i0 + 1
                let i2 = i0 + UInt32(segmentsX + 1)
                let i3 = i2 + 1

                // Two triangles per quad (counter-clockwise from viewer)
                indices.append(contentsOf: [i0, i1, i2, i1, i3, i2])
            }
        }

        // Create mesh descriptor
        var descriptor = MeshDescriptor(name: "CurvedPlane")
        descriptor.positions = MeshBuffer(positions)
        descriptor.normals = MeshBuffer(normals)
        descriptor.textureCoordinates = MeshBuffer(uvs)
        descriptor.primitives = .triangles(indices)

        do {
            return try MeshResource.generate(from: [descriptor])
        } catch {
            NSLog("SDL_RealityKitHelper: Failed to generate curved mesh: %@, using flat plane", error.localizedDescription)
            return MeshResource.generatePlane(width: width, depth: height)
        }
    }

    /// Returns the curved entity for adding to RealityView content
    func getCurvedEntity() -> ModelEntity? {
        return curvedEntity
    }

    // MARK: - Texture Updates (LowLevelTexture Pipeline)

    /// Creates or recreates the LowLevelTexture for the given dimensions
    private func ensureLowLevelTexture(width: Int, height: Int, pixelFormat: MTLPixelFormat) {
        // Check if we need to recreate (size or format changed)
        if lowLevelTexture != nil && textureWidth == width && textureHeight == height {
            return
        }

        NSLog("SDL_RealityKitHelper: Creating LowLevelTexture %dx%d", width, height)

        textureWidth = width
        textureHeight = height

        do {
            // Create LowLevelTexture descriptor using Metal pixel format directly
            var descriptor = LowLevelTexture.Descriptor()
            descriptor.textureType = .type2D
            descriptor.pixelFormat = pixelFormat
            descriptor.width = width
            descriptor.height = height
            descriptor.depth = 1
            descriptor.mipmapLevelCount = 1
            descriptor.textureUsage = [.shaderRead, .renderTarget]

            // Create the LowLevelTexture
            lowLevelTexture = try LowLevelTexture(descriptor: descriptor)

            // Create TextureResource from LowLevelTexture (this is reusable)
            textureResource = try TextureResource(from: lowLevelTexture!)

            NSLog("SDL_RealityKitHelper: LowLevelTexture created successfully")

            // Update the entity's material to use this texture resource
            updateEntityMaterial()

        } catch {
            NSLog("SDL_RealityKitHelper: ERROR - Failed to create LowLevelTexture: %@", error.localizedDescription)
            lowLevelTexture = nil
            textureResource = nil
        }
    }

    /// Updates the entity's material to use the current textureResource
    private func updateEntityMaterial() {
        guard let entity = curvedEntity, let texResource = textureResource else {
            return
        }

        if var modelComponent = entity.components[ModelComponent.self],
           modelComponent.materials.count > 0 {
            var material = UnlitMaterial()
            material.color = .init(texture: .init(texResource))
            modelComponent.materials[0] = material
            entity.components[ModelComponent.self] = modelComponent
            NSLog("SDL_RealityKitHelper: Updated entity material with LowLevelTexture")
        }
    }

    @objc public func getDisplayTexture(_ commandBuffer: MTLCommandBuffer, width: Int, height: Int, pixelFormat: MTLPixelFormat) -> MTLTexture? {
        // This can happen where we are in the middle of a transition between QT -> SDL or Volumetric -> Immersive
        guard curvedEntity != nil else {
            return nil
        }

        // Ensure LowLevelTexture exists with correct dimensions
        ensureLowLevelTexture(
            width: width,
            height: height,
            pixelFormat: pixelFormat
        )

        guard let llt = lowLevelTexture else {
            NSLog("SDL_RealityKitHelper: ERROR - No LowLevelTexture available")
            return nil
        }

        // Get the writable texture from LowLevelTexture
        return llt.replace(using: commandBuffer)
    }

    /// Updates the curvature and recreates the mesh
    public func configure(width: Int, height: Int, curvature: Float) {
        contentSizeInPoints = CGSize(width: width, height: height);

        @Environment(\.physicalMetrics) var metrics: PhysicalMetricsConverter
        let newWidth = Float(metrics.convert(Float(width), to: .meters))
        let newHeight = Float(metrics.convert(Float(height), to: .meters))

        NSLog("SDL_RealityKitHelper: configuring size %.2fx%.2f and curvature %.2f", width, height, curvature)

        Task {
            await createCurvedMesh(width: newWidth, height: newHeight, curvature: curvature)
            updateEntityMaterial()
        }
    }

    /// Updates the mesh dimensions and recreates the mesh
    public func updateSize(width: Int, height: Int) {
        if (width == Int(contentSizeInPoints.width) && height == Int(contentSizeInPoints.height)) {
            return;
        }

        contentSizeInPoints = CGSize(width: width, height: height);

        @Environment(\.physicalMetrics) var metrics: PhysicalMetricsConverter
        let newWidth = Float(metrics.convert(Float(width), to: .meters))
        let newHeight = Float(metrics.convert(Float(height), to: .meters))

        NSLog("SDL_RealityKitHelper: Updating size from %.2fx%.2f to %.2fx%.2f",
              self.meshWidth, self.meshHeight, newWidth, newHeight)

        Task {
            await createCurvedMesh(width: newWidth, height: newHeight, curvature: meshCurvature)
            updateEntityMaterial()
        }
    }

    /// Updates the curvature and recreates the mesh
    public func updateCurvature(curvature: Float) {
        let clampedCurvature = max(0.0, min(1.0, curvature))

        if abs(self.meshCurvature - clampedCurvature) < 0.01 {
            return
        }

        NSLog("SDL_RealityKitHelper: Updating curvature from %.2f to %.2f",
              self.meshCurvature, clampedCurvature)

        Task {
            await createCurvedMesh(width: meshWidth, height: meshHeight, curvature: clampedCurvature)
            updateEntityMaterial()
        }
    }

    public func isCurved() -> Bool {
        return meshCurvature > 0.0
    }
}
