if %2.==one. goto setxboxone
rem Xbox Series compile
set DXC="%GameDKLatest%\GXDK\bin\Scarlett\DXC.exe"
set SUFFIX=_Series.h
goto startbuild

:setxboxone
set DXC="%GameDKLatest%\GXDK\bin\XboxOne\DXC.exe"
set SUFFIX=_One.h

:startbuild

echo Building with %DXC%
echo Suffix %SUFFIX%

cd "%~dp0"

%DXC% -E main -T ps_6_0 -Fh color.frag.dxil%SUFFIX% color.frag.hlsl
%DXC% -E main -T ps_6_0 -Fh texture_advanced.frag.dxil%SUFFIX% texture_advanced.frag.hlsl
%DXC% -E main -T ps_6_0 -Fh texture_rgba.frag.dxil%SUFFIX% texture_rgba.frag.hlsl
%DXC% -E main -T ps_6_0 -Fh texture_rgb.frag.dxil%SUFFIX% texture_rgb.frag.hlsl

%DXC% -E main -T vs_6_0 -Fh linepoint.vert.dxil%SUFFIX% linepoint.vert.hlsl
%DXC% -E main -T vs_6_0 -Fh tri_color.vert.dxil%SUFFIX% tri_color.vert.hlsl
%DXC% -E main -T vs_6_0 -Fh tri_texture.vert.dxil%SUFFIX% tri_texture.vert.hlsl
