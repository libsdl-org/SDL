dxc  -E main -T ps_6_0 -Fh D3D12_PixelShader_Colors.h D3D12_PixelShader_Colors.hlsl
dxc  -E main -T ps_6_0 -Fh D3D12_PixelShader_Textures.h D3D12_PixelShader_Textures.hlsl
dxc  -E main -T ps_6_0 -Fh D3D12_PixelShader_Advanced.h D3D12_PixelShader_Advanced.hlsl

dxc -E mainColor -T vs_6_0 -Fh D3D12_VertexShader_Color.h D3D12_VertexShader.hlsl
dxc -E mainTexture -T vs_6_0 -Fh D3D12_VertexShader_Texture.h D3D12_VertexShader.hlsl
dxc -E mainAdvanced -T vs_6_0 -Fh D3D12_VertexShader_Advanced.h D3D12_VertexShader.hlsl

dxc -E ColorRS -T rootsig_1_1 -rootsig-define ColorRS -Fh D3D12_RootSig_Color.h D3D12_VertexShader.hlsl
dxc -E TextureRS -T rootsig_1_1 -rootsig-define TextureRS -Fh D3D12_RootSig_Texture.h D3D12_VertexShader.hlsl
dxc -E AdvancedRS -T rootsig_1_1 -rootsig-define AdvancedRS -Fh D3D12_RootSig_Advanced.h D3D12_VertexShader.hlsl
