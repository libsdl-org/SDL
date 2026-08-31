rem This script runs for the Windows build, but also via the _xbox variant with these vars set.
rem Make sure to default to building for Windows if they're not set.
if %COMPILER%.==. set COMPILER=fxc
if %SUFFIX%.==. set SUFFIX=.h
if %MODEL%.==. set MODEL=5_1

echo Building with %COMPILER%
echo Suffix %SUFFIX%
echo Shader Model %MODEL%

cd "%~dp0"

%COMPILER% -E FullscreenVert -T vs_%MODEL% -Fh D3D12_FullscreenVert.h D3D_Blit.hlsl
%COMPILER% -E BlitFrom2D -T ps_%MODEL% -Fh D3D12_BlitFrom2D.h D3D_Blit.hlsl
%COMPILER% -E BlitFrom2DArray -T ps_%MODEL% -Fh D3D12_BlitFrom2DArray.h D3D_Blit.hlsl
%COMPILER% -E BlitFrom3D -T ps_%MODEL% -Fh D3D12_BlitFrom3D.h D3D_Blit.hlsl
%COMPILER% -E BlitFromCube -T ps_%MODEL%  -Fh D3D12_BlitFromCube.h D3D_Blit.hlsl
%COMPILER% -E BlitFromCubeArray -T ps_%MODEL% -Fh D3D12_BlitFromCubeArray.h D3D_Blit.hlsl
copy /b D3D12_FullscreenVert.h+D3D12_BlitFrom2D.h+D3D12_BlitFrom2DArray.h+D3D12_BlitFrom3D.h+D3D12_BlitFromCube.h+D3D12_BlitFromCubeArray.h D3D12_Blit%SUFFIX%
del D3D12_FullscreenVert.h D3D12_BlitFrom2D.h D3D12_BlitFrom2DArray.h D3D12_BlitFrom3D.h D3D12_BlitFromCube.h D3D12_BlitFromCubeArray.h
