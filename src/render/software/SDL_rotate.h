#ifndef MIN
#define MIN(a,b)    (((a) < (b)) ? (a) : (b))
#endif

extern SDL_Surface *SDLgfx_rotateSurface(SDL_Surface * src, double angle, int centerx, int centery, int smooth, int flipx, int flipy, int dstwidth, int dstheight, double cangle, double sangle);
extern void SDLgfx_rotozoomSurfaceSizeTrig(int width, int height, double angle, int *dstwidth, int *dstheight, double *cangle, double *sangle);

