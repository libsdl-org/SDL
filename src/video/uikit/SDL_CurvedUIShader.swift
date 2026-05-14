//
//  SDL_CurvedUIShader.swift
//  SDL3
//
//  Created by Adrian Biagioli on 4/21/26.
//

import Foundation
import RealityKit

/// A MaterialX curved UI shader USDA.  This is loaded on launch into a ShaderGraphMaterial.
///
/// You can inspect this shader yourself in Reality Composer Pro.
/// To do this, copy this string and save it as a .usda file.
/// Then, add it to a Reality Composer Pro object.
private let curvedUIShaderUSDA = """
#usda 1.0
(
    customLayerData = {
        string creator = "Reality Composer Pro Version 2.0 (494.100.6)"
    }
    defaultPrim = "Root"
    metersPerUnit = 1
    upAxis = "Y"
)

def Xform "Root"
{
    def Material "CurvedUIMaterial"
    {
        reorder nameChildren = ["DefaultSurfaceShader", "UnlitSurface", "TextureCoordinates", "Position", "Image2D", "Group2", "Group4", "CursorPositionOnScreen", "SelectCursorColor", "SelectCursorOpacity", "GameTextureRGB", "NormalizedDistance", "Dot", "Group", "Dot_1", "DiscardCursorOutsideRange", "MixCursorOverGame", "HideCursorIfDisabled"]
        color3f inputs:CursorColor = (0, 0.87658346, 1) (
            colorSpace = "lin_srgb"
            customData = {
                dictionary realitykit = {
                    float2 positionInSubgraph = (-374.2671, 402.7502)
                    int stackingOrderInSubgraph = 1955
                }
            }
        )
        color3f inputs:CursorColorOnInteract = (0.016926037, 0, 0.7703071) (
            colorSpace = "lin_srgb"
            customData = {
                dictionary realitykit = {
                    float2 positionInSubgraph = (-408.82837, 336.09396)
                    int stackingOrderInSubgraph = 2017
                }
            }
        )
        float inputs:CursorEdgeThreshold = 0.9 (
            customData = {
                dictionary realitykit = {
                    float2 positionInSubgraph = (-706.12756, 582.3273)
                    int stackingOrderInSubgraph = 1951
                }
            }
        )
        float inputs:CursorOpacityEdge = 0.7 (
            customData = {
                dictionary realitykit = {
                    float2 positionInSubgraph = (-704.3221, 648.0528)
                    int stackingOrderInSubgraph = 1953
                }
            }
        )
        float inputs:CursorOpacityInside = 0.4 (
            customData = {
                dictionary realitykit = {
                    float2 positionInSubgraph = (-701.167, 710.96765)
                    int stackingOrderInSubgraph = 1955
                }
            }
        )
        float inputs:CursorSize = 0.003 (
            customData = {
                dictionary realitykit = {
                    float2 positionInSubgraph = (-1204.8192, 509.2949)
                    int stackingOrderInSubgraph = 2015
                }
            }
        )
        asset inputs:GameTexture (
            customData = {
                dictionary realitykit = {
                    float2 positionInSubgraph = (-1270.7656, -315.35458)
                    int stackingOrderInSubgraph = 1834
                }
            }
        )
        bool inputs:IsInteracting = 0 (
            customData = {
                dictionary realitykit = {
                    float2 positionInSubgraph = (-373.38513, 263.61777)
                    int stackingOrderInSubgraph = 1955
                }
            }
        )
        bool inputs:ShowCursor = 1 (
            customData = {
                dictionary realitykit = {
                    float2 positionInSubgraph = (-1721.0664, 367.89142)
                    int stackingOrderInSubgraph = 2360
                }
            }
        )
        token outputs:mtlx:surface.connect = </Root/CurvedUIMaterial/UnlitSurface.outputs:out>
        token outputs:realitykit:vertex
        token outputs:surface.connect = </Root/CurvedUIMaterial/DefaultSurfaceShader.outputs:surface>
        float2 ui:nodegraph:realitykit:subgraphOutputs:pos = (612.1894, 109.99387)
        int ui:nodegraph:realitykit:subgraphOutputs:stackingOrder = 1993

        def Shader "DefaultSurfaceShader" (
            active = false
        )
        {
            uniform token info:id = "UsdPreviewSurface"
            color3f inputs:diffuseColor = (1, 1, 1)
            float inputs:roughness = 0.75
            token outputs:surface
        }

        def Shader "UnlitSurface"
        {
            uniform token info:id = "ND_realitykit_unlit_surfaceshader"
            bool inputs:applyPostProcessToneMap = 0
            color3f inputs:color.connect = </Root/CurvedUIMaterial/MixCursorOverGame.outputs:out>
            bool inputs:hasPremultipliedAlpha
            float inputs:opacity
            float inputs:opacityThreshold
            token outputs:out
            float2 ui:nodegraph:node:pos = (368.7634, 58.4275)
            int ui:nodegraph:node:stackingOrder = 1993
        }

        def Shader "TextureCoordinates"
        {
            uniform token info:id = "ND_texcoord_vector2"
            float2 outputs:out
            float2 ui:nodegraph:node:pos = (-1292.3005, -120.02362)
            int ui:nodegraph:node:stackingOrder = 1834
        }

        def Shader "Position"
        {
            uniform token info:id = "ND_position_vector3"
            string inputs:space = "world"
            float3 outputs:out
            float2 ui:nodegraph:node:pos = (-1205.6492, 445.2142)
            int ui:nodegraph:node:stackingOrder = 2314
        }

        def Shader "Image2D"
        {
            uniform token info:id = "ND_RealityKitTexture2D_color4"
            float inputs:bias
            string inputs:border_color
            float inputs:dynamic_min_lod_clamp
            asset inputs:file.connect = </Root/CurvedUIMaterial.inputs:GameTexture>
            bool inputs:no_flip_v = 1
            int2 inputs:offset
            float2 inputs:texcoord.connect = </Root/CurvedUIMaterial/TextureCoordinates.outputs:out>
            string inputs:u_wrap_mode
            string inputs:v_wrap_mode
            color4f outputs:out
            float2 ui:nodegraph:node:pos = (-1023.8389, -194.1174)
            int ui:nodegraph:node:stackingOrder = 1834
            string[] ui:nodegraph:realitykit:node:attributesShowingChildren = ["inputs:no_flip_v"]
        }

        def Scope "Group2" (
            kind = "group"
        )
        {
            string ui:group:annotation = "Apply final color to UnlitMaterial"
            string ui:group:annotationDescription = ""
            string[] ui:group:members = ["p:UnlitSurface", "o:_subgraphOutput"]
        }

        def Scope "Group4" (
            kind = "group"
        )
        {
            string ui:group:annotation = "Sample game texture"
            string ui:group:annotationDescription = ""
            string[] ui:group:members = ["i:inputs:GameTexture", "p:Image2D", "p:TextureCoordinates"]
        }

        def Shader "SelectCursorColor"
        {
            uniform token info:id = "ND_ifequal_color3B"
            color3f inputs:in1.connect = </Root/CurvedUIMaterial.inputs:CursorColorOnInteract>
            color3f inputs:in2.connect = </Root/CurvedUIMaterial.inputs:CursorColor>
            bool inputs:value1.connect = </Root/CurvedUIMaterial.inputs:IsInteracting>
            bool inputs:value2 = 1
            color3f outputs:out
            float2 ui:nodegraph:node:pos = (-175.6293, 330.2353)
            int ui:nodegraph:node:stackingOrder = 1955
        }

        def Shader "SelectCursorOpacity"
        {
            uniform token info:id = "ND_ifgreater_float"
            float inputs:in1.connect = </Root/CurvedUIMaterial.inputs:CursorOpacityEdge>
            float inputs:in2.connect = </Root/CurvedUIMaterial.inputs:CursorOpacityInside>
            float inputs:value1.connect = </Root/CurvedUIMaterial/Dot.outputs:out>
            float inputs:value2.connect = </Root/CurvedUIMaterial.inputs:CursorEdgeThreshold>
            float outputs:out
            float2 ui:nodegraph:node:pos = (-463.96164, 578.08826)
            int ui:nodegraph:node:stackingOrder = 1853
        }

        def Shader "GameTextureRGB"
        {
            uniform token info:id = "ND_swizzle_color4_color3"
            string inputs:channels = "rgb"
            color4f inputs:in.connect = </Root/CurvedUIMaterial/Image2D.outputs:out>
            color3f outputs:out
            float2 ui:nodegraph:node:pos = (-732.1035, -11.733684)
            int ui:nodegraph:node:stackingOrder = 1834
        }

        def NodeGraph "NormalizedDistance"
        {
            float3 inputs:A (
                customData = {
                    dictionary realitykit = {
                        float2 positionInSubgraph = (79.30469, 187.10547)
                        int stackingOrderInSubgraph = 1406
                    }
                }
            )
            float3 inputs:A.connect = </Root/CurvedUIMaterial/HideCursorIfDisabled.outputs:out>
            float3 inputs:B (
                customData = {
                    dictionary realitykit = {
                        float2 positionInSubgraph = (79.234375, 270.22266)
                        int stackingOrderInSubgraph = 1408
                    }
                }
            )
            float3 inputs:B.connect = </Root/CurvedUIMaterial/Position.outputs:out>
            float inputs:Radius (
                customData = {
                    dictionary realitykit = {
                        float2 positionInSubgraph = (306.85156, 333.83984)
                        int stackingOrderInSubgraph = 1406
                    }
                }
            )
            float inputs:Radius.connect = </Root/CurvedUIMaterial.inputs:CursorSize>
            float outputs:ZeroToOneDistance (
                customData = {
                    dictionary realitykit = {
                        float2 positionInSubgraph = (444.625, 223)
                        int stackingOrderInSubgraph = 1409
                    }
                }
            )
            float outputs:ZeroToOneDistance.connect = </Root/CurvedUIMaterial/NormalizedDistance/Remap.outputs:out>
            float2 ui:nodegraph:node:pos = (-998.9227, 417.7417)
            int ui:nodegraph:node:stackingOrder = 2010
            string[] ui:nodegraph:realitykit:node:attributesShowingChildren = ["outputs:Clamp_out", "inputs:A"]
            float2 ui:nodegraph:realitykit:subgraphOutputs:pos = (711.2656, 366.07812)
            int ui:nodegraph:realitykit:subgraphOutputs:stackingOrder = 1409

            def Shader "Remap"
            {
                uniform token info:id = "ND_remap_float"
                float inputs:in.connect = </Root/CurvedUIMaterial/NormalizedDistance/MTLDistance.outputs:out>
                float inputs:inhigh.connect = </Root/CurvedUIMaterial/NormalizedDistance.inputs:Radius>
                float inputs:inlow = 0
                float inputs:outhigh = 1
                float inputs:outlow = 0
                float outputs:out
                float2 ui:nodegraph:node:pos = (503, 318.58984)
                int ui:nodegraph:node:stackingOrder = 1407
            }

            def Shader "MTLDistance"
            {
                uniform token info:id = "ND_MTL_distance_vector3_float"
                float3 inputs:x.connect = </Root/CurvedUIMaterial/NormalizedDistance.inputs:A>
                float3 inputs:y.connect = </Root/CurvedUIMaterial/NormalizedDistance.inputs:B>
                float outputs:out
                float2 ui:nodegraph:node:pos = (304, 186.67969)
                int ui:nodegraph:node:stackingOrder = 1402
            }
        }

        def Shader "Dot"
        {
            uniform token info:id = "ND_dot_float"
            float inputs:in.connect = </Root/CurvedUIMaterial/NormalizedDistance.outputs:ZeroToOneDistance>
            float outputs:out
            float2 ui:nodegraph:node:pos = (-626.7584, 475.93542)
            int ui:nodegraph:node:stackingOrder = 1735
        }

        def Scope "Group" (
            kind = "group"
        )
        {
            string ui:group:annotation = "Select cursor color and opacity"
            string ui:group:annotationDescription = "The color is selected depending if the user is interacting (click/tap/pinch/drag).  The opacity is selected via the distance between this fragment's position and the cursor position"
            string[] ui:group:members = ["i:inputs:IsInteracting", "p:Dot_1", "p:DiscardCursorOutsideRange", "i:inputs:CursorColorOnInteract", "p:SelectCursorColor", "i:inputs:CursorColor", "p:Dot", "i:inputs:CursorOpacityEdge", "i:inputs:CursorOpacityInside", "p:SelectCursorOpacity", "i:inputs:CursorEdgeThreshold"]
        }

        def Shader "Dot_1"
        {
            uniform token info:id = "ND_dot_float"
            float inputs:in.connect = </Root/CurvedUIMaterial/Dot.outputs:out>
            float outputs:out
            float2 ui:nodegraph:node:pos = (-370.1385, 475.2281)
            int ui:nodegraph:node:stackingOrder = 1851
        }

        def Shader "DiscardCursorOutsideRange"
        {
            uniform token info:id = "ND_ifgreater_float"
            float inputs:in1 = 0
            float inputs:in2.connect = </Root/CurvedUIMaterial/SelectCursorOpacity.outputs:out>
            float inputs:value1.connect = </Root/CurvedUIMaterial/Dot_1.outputs:out>
            float inputs:value2 = 1
            float outputs:out
            float2 ui:nodegraph:node:pos = (-192.05971, 600.1504)
            int ui:nodegraph:node:stackingOrder = 1966
        }

        def Shader "MixCursorOverGame"
        {
            uniform token info:id = "ND_mix_color3"
            color3f inputs:bg.connect = </Root/CurvedUIMaterial/GameTextureRGB.outputs:out>
            color3f inputs:fg.connect = </Root/CurvedUIMaterial/SelectCursorColor.outputs:out>
            float inputs:mix.connect = </Root/CurvedUIMaterial/DiscardCursorOutsideRange.outputs:out>
            color3f outputs:out
            float2 ui:nodegraph:node:pos = (90.70218, -17.587646)
            int ui:nodegraph:node:stackingOrder = 1973
        }

        def Shader "HideCursorIfDisabled"
        {
            uniform token info:id = "ND_ifequal_vector3B"
            float3 inputs:in1.connect = </Root/CurvedUIMaterial/HoverState.outputs:position>
            float3 inputs:in2 = (999999, 999999, 999999)
            bool inputs:value1.connect = </Root/CurvedUIMaterial/And.outputs:out>
            bool inputs:value2 = 1
            bool inputs:value2.connect = None
            float3 outputs:out
            float2 ui:nodegraph:node:pos = (-1281.8472, 322.0585)
            int ui:nodegraph:node:stackingOrder = 2361
        }

        def Shader "HoverState"
        {
            uniform token info:id = "ND_realitykit_hover_state"
            float outputs:intensity
            bool outputs:isActive
            float3 outputs:position
            float outputs:timeSinceHoverStart
            float2 ui:nodegraph:node:pos = (-1730.769, 258.70575)
            int ui:nodegraph:node:stackingOrder = 2360
            string[] ui:nodegraph:realitykit:node:attributesShowingChildren = ["outputs:position"]
        }

        def Shader "And"
        {
            uniform token info:id = "ND_realitykit_logical_and"
            bool inputs:in1.connect = </Root/CurvedUIMaterial/HoverState.outputs:isActive>
            bool inputs:in2.connect = </Root/CurvedUIMaterial.inputs:ShowCursor>
            bool outputs:out
            float2 ui:nodegraph:node:pos = (-1571.7467, 334.56076)
            int ui:nodegraph:node:stackingOrder = 2360
        }
    }
}

"""

/// A wrapper object around a RealityKit `ShaderGraphMaterial`, but specific to the SDL curved UI shader.
///
/// This struct provides material parameters that pass through to the `ShaderGraphMaterial`.
@MainActor
struct CurvedUIMaterial: @MainActor Equatable {
    /// A cached ShaderGraphMaterial, populated with a prototype ShaderGraphMaterial.
    ///
    /// On subsequent loads, the alread-loaded material is used directly.
    @MainActor private static var cachedShaderGraph: ShaderGraphMaterial?
    
    /// The ShaderGraphMaterial which should be used to populate the curved UI Entity's `ModelComponent`.
    ///
    /// - Note: ShaderGraphMaterial is a value type (`struct`), so you must re-query this value after changing any parameters.
    private(set) var shaderGraphMaterial: ShaderGraphMaterial
    
    /// Initializes the curved UI material.
    ///
    /// If the shader needs to compile (first launch), then it compiles before returning.
    /// If the shader is already compiled, returns immediately.
    @MainActor
    init() async throws {
        if let cachedShaderGraph = Self.cachedShaderGraph {
            self.shaderGraphMaterial = cachedShaderGraph
        } else {
            let result = try await ShaderGraphMaterial(
                named: "/Root/CurvedUIMaterial",
                from: Data(curvedUIShaderUSDA.utf8)
            )
            Self.cachedShaderGraph = result
            self.shaderGraphMaterial = result
        }
    }
    
    /// The texture containing SDL content.
    var gameTexture: TextureResource! {
        get { shaderGraphMaterial.getParameter(.gameTexture) }
        set { try! shaderGraphMaterial.setParameter(.gameTexture, value: newValue) }
    }
    
    /// Color of the cursor overlay when not actively interacting.
    var cursorColor: UIColor! {
        get { shaderGraphMaterial.getParameter(.cursorColor) }
        set { try! shaderGraphMaterial.setParameter(.cursorColor, value: newValue) }
    }
    
    /// Color of the cursor when interacting (click/tap/pinch/drag)
    var cursorColorOnInteract: UIColor! {
        get { shaderGraphMaterial.getParameter(.cursorColorOnInteract) }
        set { try! shaderGraphMaterial.setParameter(.cursorColorOnInteract, value: newValue) }
    }
    
    /// The size of the cursor in meters.
    var cursorSize: Float! {
        get { shaderGraphMaterial.getParameter(.cursorSize) }
        set { try! shaderGraphMaterial.setParameter(.cursorSize, value: newValue) }
    }
    
    /// Whether to show the cursor overlay on the mesh surface.
    var showCursor: Bool! {
        get { shaderGraphMaterial.getParameter(.showCursor) }
        set { try! shaderGraphMaterial.setParameter(.showCursor, value: newValue) }
    }

    /// True if the user is actively interacting with the scene (e.g. click, tap, pinch, or drag).
    var isInteracting: Bool! {
        get { shaderGraphMaterial.getParameter(.isInteracting) }
        set { try! shaderGraphMaterial.setParameter(.isInteracting, value: newValue) }
    }
    
    static func == (lhs: CurvedUIMaterial, rhs: CurvedUIMaterial) -> Bool {
        return lhs.gameTexture == rhs.gameTexture
            && lhs.cursorColor == rhs.cursorColor
            && lhs.cursorColorOnInteract == rhs.cursorColorOnInteract
            && lhs.cursorSize == rhs.cursorSize
            && lhs.showCursor == rhs.showCursor
            && lhs.isInteracting == rhs.isInteracting
    }
}

@MainActor
private extension MaterialParameters.Handle {
    static let gameTexture = ShaderGraphMaterial.parameterHandle(name: "GameTexture")
    static let cursorColor = ShaderGraphMaterial.parameterHandle(name: "CursorColor")
    static let cursorColorOnInteract = ShaderGraphMaterial.parameterHandle(name: "CursorColorOnInteract")
    static let cursorSize = ShaderGraphMaterial.parameterHandle(name: "CursorSize")
    static let showCursor = ShaderGraphMaterial.parameterHandle(name: "ShowCursor")
    static let isInteracting = ShaderGraphMaterial.parameterHandle(name: "IsInteracting")
}

private extension ShaderGraphMaterial {
    /// Convenience function to recover a typed shader parameter (without going through `MaterialParametres.Value` enum)
    func getParameter<T>(_ handle: MaterialParameters.Handle, type: T.Type = T.self) -> T? {
        guard let value = self.getParameter(handle: handle) else { return nil }
        
        switch (type.self, value) {
        case (is MaterialParameters.Texture.Type, .texture(let v)): return (v as! T)
        case (is TextureResource.Type, .texture(let v)): return (v.resource as! T)
        case (is TextureResource.Type, .textureResource(let v)): return (v as! T)
        case (is Float.Type, .float(let v)): return (v as! T)
        case (is SIMD2<Float>.Type, .simd2Float(let v)): return (v as! T)
        case (is SIMD3<Float>.Type, .simd3Float(let v)): return (v as! T)
        case (is SIMD4<Float>.Type, .simd4Float(let v)): return (v as! T)
        case (is UIColor.Type, .color(let v)): fallthrough
        case (is CGColor.Type, .color(let v)):
            // `is CGColor` works for both UIColor and CGColor
            if type == CGColor.self {
                return (v as! T)
            } else if type == UIColor.self {
                return (UIColor(cgColor: v) as! T)
            } else {
                preconditionFailure("Unknown Color type \(type)")
            }
        case (is float2x2.Type, .float2x2(let v)): return (v as! T)
        case (is float3x3.Type, .float3x3(let v)): return (v as! T)
        case (is float4x4.Type, .float4x4(let v)): return (v as! T)
        case (is Bool.Type, .bool(let v)): return (v as! T)
        case (is Int.Type, .int(let v)): return (Int(v) as! T)
        case (is Int32.Type, .int(let v)): return (v as! T)
        default:
            preconditionFailure("Invalid type \(type) for handle with value \(value)")
        }
    }
    
    /// Convenience function to set a typed shader parameter (without going through `MaterialParametres.Value` enum)
    mutating func setParameter<T>(_ handle: MaterialParameters.Handle, value: T!) throws {
        guard let value else { preconditionFailure("can not clear a material parameter") }
        switch type(of: value).self {
        case is MaterialParameters.Texture.Type:
            try self.setParameter(handle: handle, value: .texture(value as! MaterialParameters.Texture))
        case is TextureResource.Type:
            try self.setParameter(handle: handle, value: .textureResource(value as! TextureResource))
        case is Float.Type:
            try self.setParameter(handle: handle, value: .float(value as! Float))
        case is SIMD2<Float>.Type:
            try self.setParameter(handle: handle, value: .simd2Float(value as! SIMD2<Float>))
        case is SIMD3<Float>.Type:
            try self.setParameter(handle: handle, value: .simd3Float(value as! SIMD3<Float>))
        case is SIMD4<Float>.Type:
            try self.setParameter(handle: handle, value: .simd4Float(value as! SIMD4<Float>))
        case is CGColor.Type: fallthrough
        case is UIColor.Type:
            // `is CGColor` works for both UIColor and CGColor
            if T.self == UIColor.self {
                try self.setParameter(handle: handle, value: .color(value as! UIColor))
            } else if T.self == CGColor.self {
                try self.setParameter(handle: handle, value: .color(value as! CGColor))
            } else {
                preconditionFailure("Unknown Color type \(type(of: value))")
            }
        case is float2x2.Type:
            try self.setParameter(handle: handle, value: .float2x2(value as! float2x2))
        case is float3x3.Type:
            try self.setParameter(handle: handle, value: .float3x3(value as! float3x3))
        case is float4x4.Type:
            try self.setParameter(handle: handle, value: .float4x4(value as! float4x4))
        case is Bool.Type:
            try self.setParameter(handle: handle, value: .bool(value as! Bool))
        case is Int.Type:
            try self.setParameter(handle: handle, value: .int(Int32(value as! Int)))
        case is Int32.Type:
            try self.setParameter(handle: handle, value: .int(value as! Int32))
        default:
            preconditionFailure("Invalid type \(type(of: value))")
        }
    }
}
