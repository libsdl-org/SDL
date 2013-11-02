/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2012 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

#include <D3D11_1.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>

struct SDL_VertexShaderConstants
{
    DirectX::XMFLOAT4X4 model;
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;
};

typedef struct
{
    Microsoft::WRL::ComPtr<ID3D11Device1> d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> d3dContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mainRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> currentOffscreenRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> texturePixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> colorPixelShader;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendModeBlend;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendModeAdd;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendModeMod;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> nearestPixelSampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearSampler;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> mainRasterizer;
    D3D_FEATURE_LEVEL featureLevel;

    // Vertex buffer constants:
    SDL_VertexShaderConstants vertexShaderConstantsData;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexShaderConstants;

    // Cached renderer properties.
    DirectX::XMFLOAT2 windowSizeInDIPs;
    DirectX::XMFLOAT2 renderTargetSize;
    Windows::Graphics::Display::DisplayOrientations orientation;

    // Transform used for display orientation.
    DirectX::XMFLOAT4X4 orientationTransform3D;
} D3D11_RenderData;

typedef struct
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> mainTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mainTextureResourceView;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mainTextureRenderTargetView;
    SDL_PixelFormat * pixelFormat;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
    DirectX::XMINT2 lockedTexturePosition;
    D3D11_FILTER scaleMode;
} D3D11_TextureData;

struct VertexPositionColor
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 tex;
    DirectX::XMFLOAT4 color;
};

/* vi: set ts=4 sw=4 expandtab: */
