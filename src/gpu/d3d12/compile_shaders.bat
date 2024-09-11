rem This script runs for the Windows build, but also via the _xbox variant with these vars set.
rem Make sure to default to building for Windows if they're not set.
if %DXC%.==. set DXC=dxc
if %SUFFIX%.==. set SUFFIX=.h

echo Building with %DXC%
echo Suffix %SUFFIX%

cd "%~dp0"

%DXC% -E FullscreenVert -T vs_6_0 -Fh D3D12_FullscreenVert.h ..\d3dcommon\D3D_Blit.hlsl /D D3D12=1
%DXC% -E BlitFrom2D -T ps_6_0 -Fh D3D12_BlitFrom2D.h ..\d3dcommon\D3D_Blit.hlsl /D D3D12=1
%DXC% -E BlitFrom2DArray -T ps_6_0 -Fh D3D12_BlitFrom2DArray.h ..\d3dcommon\D3D_Blit.hlsl /D D3D12=1
%DXC% -E BlitFrom3D -T ps_6_0 -Fh D3D12_BlitFrom3D.h ..\d3dcommon\D3D_Blit.hlsl /D D3D12=1
%DXC% -E BlitFromCube -T ps_6_0  -Fh D3D12_BlitFromCube.h ..\d3dcommon\D3D_Blit.hlsl /D D3D12=1
%DXC% -E BlitFromCubeArray -T ps_6_0 -Fh D3D12_BlitFromCubeArray.h ..\d3dcommon\D3D_Blit.hlsl /D D3D12=1
copy /b D3D12_FullscreenVert.h+D3D12_BlitFrom2D.h+D3D12_BlitFrom2DArray.h+D3D12_BlitFrom3D.h+D3D12_BlitFromCube.h+D3D12_BlitFromCubeArray.h D3D12_Blit%SUFFIX%
del D3D12_FullscreenVert.h D3D12_BlitFrom2D.h D3D12_BlitFrom2DArray.h D3D12_BlitFrom3D.h D3D12_BlitFromCube.h D3D12_BlitFromCubeArray.h
