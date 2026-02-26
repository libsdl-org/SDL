#if defined(SDL_PLATFORM_XBOXONE)
#define g_main color_frag_dxil
#include "color.frag.dxil_One.h"
#undef g_main
#define g_main linepoint_vert_dxil
#include "linepoint.vert.dxil_One.h"
#undef g_main
#define g_main texture_advanced_frag_dxil
#include "texture_advanced.frag.dxil_One.h"
#undef g_main
#define g_main texture_rgb_frag_dxil
#include "texture_rgb.frag.dxil_One.h"
#undef g_main
#define g_main texture_rgba_frag_dxil
#include "texture_rgba.frag.dxil_One.h"
#undef g_main
#define g_main tri_color_vert_dxil
#include "tri_color.vert.dxil_One.h"
#undef g_main
#define g_main tri_texture_vert_dxil
#include "tri_texture.vert.dxil_One.h"
#undef g_main
#elif defined(SDL_PLATFORM_XBOXSERIES)
#define g_main color_frag_dxil
#include "color.frag.dxil_Series.h"
#undef g_main
#define g_main linepoint_vert_dxil
#include "linepoint.vert.dxil_Series.h"
#undef g_main
#define g_main texture_advanced_frag_dxil
#include "texture_advanced.frag.dxil_Series.h"
#undef g_main
#define g_main texture_rgb_frag_dxil
#include "texture_rgb.frag.dxil_Series.h"
#undef g_main
#define g_main texture_rgba_frag_dxil
#include "texture_rgba.frag.dxil_Series.h"
#undef g_main
#define g_main tri_color_vert_dxil
#include "tri_color.vert.dxil_Series.h"
#undef g_main
#define g_main tri_texture_vert_dxil
#include "tri_texture.vert.dxil_Series.h"
#undef g_main
#else
#include "color.frag.dxil.h"
#include "linepoint.vert.dxil.h"
#include "texture_advanced.frag.dxil.h"
#include "texture_rgb.frag.dxil.h"
#include "texture_rgba.frag.dxil.h"
#include "tri_color.vert.dxil.h"
#include "tri_texture.vert.dxil.h"
#endif
