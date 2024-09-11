fxc /T vs_5_0 /E FullscreenVert /Fh D3D11_FullscreenVert.h ..\d3dcommon\D3D_Blit.hlsl
fxc /T ps_5_0 /E BlitFrom2D /Fh D3D11_BlitFrom2D.h ..\d3dcommon\D3D_Blit.hlsl
fxc /T ps_5_0 /E BlitFrom2DArray /Fh D3D11_BlitFrom2DArray.h ..\d3dcommon\D3D_Blit.hlsl
fxc /T ps_5_0 /E BlitFrom3D /Fh D3D11_BlitFrom3D.h ..\d3dcommon\D3D_Blit.hlsl
fxc /T ps_5_0 /E BlitFromCube /Fh D3D11_BlitFromCube.h ..\d3dcommon\D3D_Blit.hlsl
fxc /T ps_5_0 /E BlitFromCubeArray /Fh D3D11_BlitFromCubeArray.h ..\d3dcommon\D3D_Blit.hlsl
copy /b D3D11_FullscreenVert.h+D3D11_BlitFrom2D.h+D3D11_BlitFrom2DArray.h+D3D11_BlitFrom3D.h+D3D11_BlitFromCube.h+D3D11_BlitFromCubeArray.h D3D11_Blit.h
del D3D11_FullscreenVert.h D3D11_BlitFrom2D.h D3D11_BlitFrom2DArray.h D3D11_BlitFrom3D.h D3D11_BlitFromCube.h D3D11_BlitFromCubeArray.h
