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
import simd

/// Custom vertex format for the curved plane mesh.
/// Matches the layout described to LowLevelMesh via vertexAttributes/vertexLayouts.
private struct CurvedPlaneVertex {
    var position: SIMD3<Float> = .zero
    var normal: SIMD3<Float> = .zero
    var uv: SIMD2<Float> = .zero

    static var vertexAttributes: [LowLevelMesh.Attribute] {
        [
            .init(semantic: .position, format: .float3, offset: MemoryLayout<Self>.offset(of: \.position)!),
            .init(semantic: .normal, format: .float3, offset: MemoryLayout<Self>.offset(of: \.normal)!),
            .init(semantic: .uv0, format: .float2, offset: MemoryLayout<Self>.offset(of: \.uv)!)
        ]
    }

    static var vertexLayouts: [LowLevelMesh.Layout] {
        [.init(bufferIndex: 0, bufferStride: MemoryLayout<Self>.stride)]
    }

    static func descriptor(vertexCount: Int, indexCount: Int) -> LowLevelMesh.Descriptor {
        var desc = LowLevelMesh.Descriptor()
        desc.vertexAttributes = vertexAttributes
        desc.vertexLayouts = vertexLayouts
        desc.vertexCapacity = vertexCount
        desc.indexCapacity = indexCount
        desc.indexType = .uint32
        return desc
    }
}

/// Provides RealityKit functionality
///
/// Key responsibilities:
/// - Generate curved mesh geometry procedurally using LowLevelMesh for fast updates
/// - Update textures using LowLevelTexture for efficient Metal → RealityKit transfer
/// - Asynchronously cooks a physics collision mesh of the curved UI to be used as an input target
@MainActor
@Observable
internal class SDL_RealityKitHelper {
    /// A collision shape which should be assigned to the same entity as ``lowLevelMesh``, for input targeting.
    private(set) var collisionShape: ShapeResource? = nil
    
    /// The TextureResource object which should be assigned to an entity in the scene.
    private(set) var textureResource: TextureResource? = nil
    
    /// The LowLevelMesh object which should be assigned to an entity in the scene, positioned at the origin.
    ///
    /// This mesh is auomatically updated when you change ``meshGeometry`` via ``updateMeshGeometry()``.
    /// LowLevelMesh is a class (reference type) so you can add it to your Entity's `MeshResource` once at init time.
    let lowLevelMesh: LowLevelMesh

    /// Topology characteristics of the generated mesh.  This is fixed at initialization time.
    let meshTopology: CurvedMeshTopology

    /// The current generated mesh geometry.  Update this with ``updateMeshGeometry()``
    private(set) var meshGeometry: CurvedMeshGeometry = CurvedMeshGeometry(width: 1, height: 1)
    
    /// An async task responsible for managing physics mesh cooking.
    ///
    /// This guarantees that at most one cooking operation is active at a time.
    /// Cooking generally takes > 1 frame, so it's important that there is not an explosion of redundant work
    /// if there is a burst of resize activity.
    private var physicsCookingTask: Task<Void, Never>?
    
    /// ``collisionShape`` is up to date with this `CurvedMeshGeometry`.
    private var lastCookedGeometry: CurvedMeshGeometry?

    /// LowLevelTexture that backs ``textureResource``.
    private var lowLevelTexture: LowLevelTexture?
    
    struct CurvedMeshTopology: Sendable, Equatable {
        /// Number of horizontal segments to use to generate the mesh grid
        var segmentsX: Int = 32
        
        /// Number of vertical segments to use to generate the mesh grid
        var segmentsY: Int = 32
        
        /// Total number of vertices required to generate a mesh with this topology
        var vertexCount: Int { (segmentsX + 1) * (segmentsY + 1) }
        
        /// Total size of the index buffer when generating a mesh with this topology
        var indexCount: Int { segmentsX * segmentsY * 6 }
    }
    
    struct CurvedMeshGeometry: Sendable, Equatable {
        /// Width of the mesh in meters.
        var width: Float

        /// Height of the mesh in meters.
        var height: Float

        /// Radius of the mesh curvature in meters, or `nil` for a flat mesh.
        var curvatureRadius: Float = 0
        
        /// The bounding box of the mesh
        var bounds: BoundingBox = BoundingBox()
    
        /// Current snapped status
        var snapped: Bool = false

        /// Converts a 3D position on the mesh surface (in meters, relative to mesh center)
        /// to normalized texture coordinates (0..1, 0..1).
        func normalizedUV(fromMeshPosition position: SIMD3<Float>) -> SIMD2<Float> {
            if curvatureRadius > 0 {
                let halfWidth = bounds.extents.x / 2
                
                let theta = asinf(halfWidth / curvatureRadius)
                let angle = asinf(position.x / curvatureRadius)
                
                let u = (angle / theta + 1) / 2
                let v = (position.y / height) + 0.5
                return SIMD2(u, v)
            } else {
                let u = (position.x / width) + 0.5
                let v = (position.y / height) + 0.5
                return SIMD2(u, v)
            }
        }
    }

    init(meshTopology: CurvedMeshTopology = CurvedMeshTopology(),
         meshGeometry: CurvedMeshGeometry = CurvedMeshGeometry(width: 1, height: 1)) {
        self.meshTopology = meshTopology
        self.meshGeometry = CurvedMeshGeometry(width: -1, height: -1)
        
        let lowLevelMesh = try! meshTopology.generateMesh()

        self.lowLevelMesh = lowLevelMesh
        
        updateMeshGeometry(meshGeometry)
    }
    
    // MARK: - Mesh Generation (LowLevelMesh)
    
    func updateSnappedStatus(snapped: Bool) {
        var geometry = self.meshGeometry
        geometry.snapped = snapped
        updateMeshGeometry(geometry)
    }
    
    func updateMeshSize(width: Float, height: Float) {
        var geometry = self.meshGeometry
        geometry.width = width
        geometry.height = height
        updateMeshGeometry(geometry)
    }
    
    func updateMeshCurvature(curvatureRadius: Float) {
        var geometry = self.meshGeometry
        geometry.curvatureRadius = curvatureRadius
        updateMeshGeometry(geometry)
    }

    /// Writes vertex position/normal/uv data into the LowLevelMesh buffer.
    /// This is the fast path — called on every size or curvature change without
    /// recreating MeshResource or Entity.
    func updateMeshGeometry(_ meshGeometry: CurvedMeshGeometry) {
        if meshGeometry == self.meshGeometry {
            return // nothing to do
        }
        
        let width = meshGeometry.width
        let height = meshGeometry.height
        let curvatureRadius = meshGeometry.curvatureRadius
        
        let segmentsX = meshTopology.segmentsX
        let segmentsY = meshTopology.segmentsY
        let indexCount = meshTopology.indexCount
        
        var boundsMin = SIMD3(repeating: Float.infinity)
        var boundsMax = SIMD3(repeating: -Float.infinity)
        
        lowLevelMesh.withUnsafeMutableBytes(bufferIndex: 0) { rawBytes in
            let vertices = rawBytes.bindMemory(to: CurvedPlaneVertex.self)

            if curvatureRadius > 0 {

                // Apply cylindrical curve: Z varies with X to create wrap-around
                var curve_positions: [SIMD3<Float>] = []
                var curve_normals: [SIMD3<Float>] = []
                let r = curvatureRadius
                let arc_length = width / r
                for x in 0...segmentsX {
                    let u = Float(x) / Float(segmentsX)
                    let angle = (u - 0.5) * arc_length
                    let vec: SIMD3<Float> = simd_normalize([sin(angle), 0.0, cos(angle)])
                    let pos: SIMD3<Float> = [vec.x, vec.y, 1.0 - vec.z] * r
                    curve_positions.append(pos)

                    // Normal points toward viewer for convex curve
                    curve_normals.append(-vec)
                }
                let offsetZ = meshGeometry.snapped ? 0 : -curve_positions[0].z
                
                for y in 0...segmentsY {
                    let v = Float(y) / Float(segmentsY) * 2 - 1
                    let posY = v * height / 2

                    for x in 0...segmentsX {
                        let u = Float(x) / Float(segmentsX) * 2 - 1
                        
                        let position = curve_positions[x] + SIMD3<Float>(0, posY, offsetZ)
                        let normal = curve_normals[x]

                        let idx = y * (segmentsX + 1) + x
                        vertices[idx].position = position
                        vertices[idx].normal = normal
                        vertices[idx].uv = SIMD2<Float>((u + 1) / 2, (v + 1) / 2)
                        
                        boundsMin = min(boundsMin, position)
                        boundsMax = max(boundsMax, position)
                    }
                }
            } else {
                // Flat plane — same grid, z=0
                for y in 0...segmentsY {
                    let v = Float(y) / Float(segmentsY)
                    let posY = (v - 0.5) * height

                    for x in 0...segmentsX {
                        let u = Float(x) / Float(segmentsX)
                        let posX = (u - 0.5) * width

                        let idx = y * (segmentsX + 1) + x
                        let position = SIMD3<Float>(posX, posY, 0)
                        vertices[idx].position = position
                        vertices[idx].normal = SIMD3<Float>(0, 0, -1)
                        vertices[idx].uv = SIMD2<Float>(u, v)
                        
                        boundsMin = min(boundsMin, position)
                        boundsMax = max(boundsMax, position)
                    }
                }
            }
        }

        let bounds = BoundingBox(min: boundsMin, max: boundsMax)
        lowLevelMesh.parts.replaceAll([
            LowLevelMesh.Part(indexCount: indexCount, topology: .triangle, bounds: bounds)
        ])
        
        self.meshGeometry = meshGeometry
        self.meshGeometry.bounds = bounds
        invalidatePhysicsMesh()
    }

    // MARK: - Physics Mesh Cooking

    /// Schedules an async physics mesh cook. If a cook is already in progress,
    /// it will automatically re-cook when done if the geometry has changed.
    private func invalidatePhysicsMesh() {
        guard physicsCookingTask == nil else { return }
        physicsCookingTask = Task {
            defer { physicsCookingTask = nil }
            // Loop until the cooked physics mesh matches the current geometry.
            // Each iteration cooks against whatever the MeshResource currently reflects.
            while lastCookedGeometry != meshGeometry {
                let geometryAtStart = meshGeometry
                do {
                    let meshResource = try await MeshResource(from: lowLevelMesh)
                    let shape = try await ShapeResource.generateStaticMesh(from: meshResource)
                    collisionShape = shape
                    lastCookedGeometry = geometryAtStart
                } catch {
                    NSLog("SDL_RealityKitHelper: Failed to generate physics mesh: %@", error.localizedDescription)
                    break
                }
            }
        }
    }

    // MARK: - Texture Updates (LowLevelTexture Pipeline)

    /// Creates or recreates the LowLevelTexture for the given dimensions
    private func ensureLowLevelTexture(width: Int, height: Int, pixelFormat: MTLPixelFormat) {
        // Check if we need to recreate (size or format changed)
        if let lowLevelTexture,
           lowLevelTexture.descriptor.width == width,
           lowLevelTexture.descriptor.height == height,
           lowLevelTexture.descriptor.pixelFormat == pixelFormat
        {
            return
        }

        //NSLog("SDL_RealityKitHelper: Creating LowLevelTexture %dx%d", width, height)

        do {
            // Create LowLevelTexture descriptor using Metal pixel format directly
            var descriptor = LowLevelTexture.Descriptor()
            descriptor.textureType = .type2D
            descriptor.pixelFormat = pixelFormat
            descriptor.width = width
            descriptor.height = height
            descriptor.depth = 1
            let size = max(width, height)
            if (size > 32) {
                descriptor.mipmapLevelCount = Int(floor(log2(Float(size)))) - 5
            } else {
                descriptor.mipmapLevelCount = 0
            }
            descriptor.textureUsage = [.shaderRead, .renderTarget]

            // Create the LowLevelTexture
            lowLevelTexture = try LowLevelTexture(descriptor: descriptor)

            // Create TextureResource from LowLevelTexture (this is reusable)
            textureResource = try TextureResource(from: lowLevelTexture!)

            //NSLog("SDL_RealityKitHelper: LowLevelTexture created successfully")
        } catch {
            NSLog("SDL_RealityKitHelper: ERROR - Failed to create LowLevelTexture: %@", error.localizedDescription)
            lowLevelTexture = nil
            textureResource = nil
        }
    }

    @objc public func getDisplayTexture(_ commandBuffer: MTLCommandBuffer, width: Int, height: Int, pixelFormat: MTLPixelFormat) -> MTLTexture? {
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
}

extension SDL_RealityKitHelper.CurvedMeshTopology {
    @MainActor
    func generateMesh() throws -> LowLevelMesh {
        //NSLog("SDL_RealityKitHelper: Creating LowLevelMesh (%dx%d grid, %d vertices, %d indices)",
        //      segmentsX, segmentsY, vertexCount, indexCount)

        // Create LowLevelMesh with our custom vertex format
        let desc = CurvedPlaneVertex.descriptor(vertexCount: vertexCount, indexCount: indexCount)
        let mesh = try LowLevelMesh(descriptor: desc)

        // Write index buffer once — topology never changes for a fixed grid
        mesh.withUnsafeMutableIndices { rawIndices in
            let indices = rawIndices.bindMemory(to: UInt32.self)
            var idx = 0
            for y in 0..<segmentsY {
                for x in 0..<segmentsX {
                    let i0 = UInt32(y * (segmentsX + 1) + x)
                    let i1 = i0 + 1
                    let i2 = i0 + UInt32(segmentsX + 1)
                    let i3 = i2 + 1

                    // Two triangles per quad (counter-clockwise winding)
                    indices[idx]     = i0
                    indices[idx + 1] = i1
                    indices[idx + 2] = i2
                    indices[idx + 3] = i1
                    indices[idx + 4] = i3
                    indices[idx + 5] = i2
                    idx += 6
                }
            }
        }
        
        return mesh
    }
}
