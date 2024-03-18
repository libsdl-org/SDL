/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

/* Function Pointer Signatures */
typedef HRESULT(WINAPI* PFN_CREATE_DXGI_FACTORY1)(const GUID* riid, void** ppFactory);
typedef HRESULT(WINAPI* PFN_DXGI_GET_DEBUG_INTERFACE)(const GUID* riid, void** ppDebug);

 /* IIDs (from https://magnumdb.com) */
static const IID D3D_IID_IDXGIFactory1 = { 0x770aae78,0xf26f,0x4dba,{0xa8,0x29,0x25,0x3c,0x83,0xd1,0xb3,0x87} };
static const IID D3D_IID_IDXGIFactory4 = { 0x1bc6ea02,0xef36,0x464f,{0xbf,0x0c,0x21,0xca,0x39,0xe5,0x16,0x8a} };
static const IID D3D_IID_IDXGIFactory5 = { 0x7632e1f5,0xee65,0x4dca,{0x87,0xfd,0x84,0xcd,0x75,0xf8,0x83,0x8d} };
static const IID D3D_IID_IDXGIFactory6 = { 0xc1b6694f,0xff09,0x44a9,{0xb0,0x3c,0x77,0x90,0x0a,0x0a,0x1d,0x17} };
static const IID D3D_IID_IDXGIAdapter1 = { 0x29038f61,0x3839,0x4626,{0x91,0xfd,0x08,0x68,0x79,0x01,0x1a,0x05} };
static const IID D3D_IID_ID3D11Texture2D = { 0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c} };
static const IID D3D_IID_ID3D11Device1 = { 0xa04bfb29,0x08ef,0x43d6,{0xa4,0x9c,0xa9,0xbd,0xbd,0xcb,0xe6,0x86} };
static const IID D3D_IID_IDXGIDebug = { 0x119e7452,0xde9e,0x40fe,{0x88,0x06,0x88,0xf9,0x0c,0x12,0xb4,0x41} };
static const IID D3D_IID_IDXGIInfoQueue = { 0xd67441c7,0x672a,0x476f,{0x9e,0x82,0xcd,0x55,0xb4,0x49,0x49,0xce} };

static const GUID D3D_IID_DXGI_DEBUG_ALL = { 0xe48ae283,0xda80,0x490b,{0x87,0xe6,0x43,0xe9,0xa9,0xcf,0xda,0x08} };
