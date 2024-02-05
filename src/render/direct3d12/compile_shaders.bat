dxc  -E main -T ps_6_0 -Fh D3D12_PixelShader_Colors.h D3D12_PixelShader_Colors.hlsl
dxc  -E main -T ps_6_0 -Fh D3D12_PixelShader_Textures.h D3D12_PixelShader_Textures.hlsl
dxc  -E main -T ps_6_0 -Fh D3D12_PixelShader_YUV.h D3D12_PixelShader_YUV.hlsl
dxc  -E main -T ps_6_0 -Fh D3D12_PixelShader_NV12.h D3D12_PixelShader_NV12.hlsl
dxc  -E main -T ps_6_0 -Fh D3D12_PixelShader_NV21.h D3D12_PixelShader_NV21.hlsl
dxc  -E main -T ps_6_0 -Fh D3D12_PixelShader_HDR10.h D3D12_PixelShader_HDR10.hlsl

dxc -E mainColor -T vs_6_0 -Fh D3D12_VertexShader_Color.h D3D12_VertexShader.hlsl
dxc -E mainTexture -T vs_6_0 -Fh D3D12_VertexShader_Texture.h D3D12_VertexShader.hlsl
dxc -E mainYUV -T vs_6_0 -Fh D3D12_VertexShader_YUV.h D3D12_VertexShader.hlsl
dxc -E mainNV -T vs_6_0 -Fh D3D12_VertexShader_NV.h D3D12_VertexShader.hlsl

dxc -E ColorRS -T rootsig_1_1 -rootsig-define ColorRS -Fh D3D12_RootSig_Color.h D3D12_VertexShader.hlsl
dxc -E TextureRS -T rootsig_1_1 -rootsig-define TextureRS -Fh D3D12_RootSig_Texture.h D3D12_VertexShader.hlsl
dxc -E YUVRS -T rootsig_1_1 -rootsig-define YUVRS -Fh D3D12_RootSig_YUV.h D3D12_VertexShader.hlsl
dxc -E NVRS -T rootsig_1_1 -rootsig-define NVRS -Fh D3D12_RootSig_NV.h D3D12_VertexShader.hlsl
