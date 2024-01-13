set XBOXDXC="%GameDKLatest%\GXDK\bin\Scarlett\DXC.exe"
echo Building with %XBOXDXC%
cd "%1\shaders"
rem Root Signatures
%XBOXDXC% -E ColorRS -T rootsig_1_1 -rootsig-define ColorRS -Fh D3D12_RootSig_Color D3D12_VertexShader.hlsl
%XBOXDXC% -E TextureRS -T rootsig_1_1 -rootsig-define TextureRS -Fh D3D12_RootSig_Texture D3D12_VertexShader.hlsl
%XBOXDXC% -E YUVRS -T rootsig_1_1 -rootsig-define YUVRS -Fh D3D12_RootSig_YUV D3D12_VertexShader.hlsl
%XBOXDXC% -E NVRS -T rootsig_1_1 -rootsig-define NVRS -Fh D3D12_RootSig_NV D3D12_VertexShader.hlsl
rem Vertex Shaders
%XBOXDXC% -E mainColor -T vs_6_0 -Fh D3D12_VertexShader_Color D3D12_VertexShader.hlsl
%XBOXDXC% -E mainTexture -T vs_6_0 -Fh D3D12_VertexShader_Texture D3D12_VertexShader.hlsl
%XBOXDXC% -E mainNV -T vs_6_0 -Fh D3D12_VertexShader_NV D3D12_VertexShader.hlsl
%XBOXDXC% -E mainYUV -T vs_6_0 -Fh D3D12_VertexShader_YUV D3D12_VertexShader.hlsl
rem Pixel Shaders
%XBOXDXC% -E main -T ps_6_0 -Fh D3D12_PixelShader_Colors D3D12_PixelShader_Colors.hlsl
%XBOXDXC% -E main -T ps_6_0 -Fh D3D12_PixelShader_NV12_BT601 D3D12_PixelShader_NV12_BT601.hlsl
%XBOXDXC% -E main -T ps_6_0 -Fh D3D12_PixelShader_NV12_BT709 D3D12_PixelShader_NV12_BT709.hlsl
%XBOXDXC% -E main -T ps_6_0 -Fh D3D12_PixelShader_NV12_JPEG D3D12_PixelShader_NV12_JPEG.hlsl
%XBOXDXC% -E main -T ps_6_0 -Fh D3D12_PixelShader_NV21_BT601 D3D12_PixelShader_NV21_BT601.hlsl
%XBOXDXC% -E main -T ps_6_0 -Fh D3D12_PixelShader_NV21_BT709 D3D12_PixelShader_NV21_BT709.hlsl
%XBOXDXC% -E main -T ps_6_0 -Fh D3D12_PixelShader_NV21_JPEG D3D12_PixelShader_NV21_JPEG.hlsl
%XBOXDXC% -E main -T ps_6_0 -Fh D3D12_PixelShader_Textures D3D12_PixelShader_Textures.hlsl
%XBOXDXC% -E main -T ps_6_0 -Fh D3D12_PixelShader_YUV_BT601 D3D12_PixelShader_YUV_BT601.hlsl
%XBOXDXC% -E main -T ps_6_0 -Fh D3D12_PixelShader_YUV_BT709 D3D12_PixelShader_YUV_BT709.hlsl
%XBOXDXC% -E main -T ps_6_0 -Fh D3D12_PixelShader_YUV_JPEG D3D12_PixelShader_YUV_JPEG.hlsl