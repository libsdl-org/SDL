#include <SDL3/SDL.h>

#include <stdio.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("     CPU count : %d\n", SDL_GetCPUCount());
    printf("CacheLine size : %d\n", SDL_GetCPUCacheLineSize());
    printf("         RDTSC : %d\n", SDL_HasRDTSC());
    printf("       Altivec : %d\n", SDL_HasAltiVec());
    printf("           MMX : %d\n", SDL_HasMMX());
    printf("           SSE : %d\n", SDL_HasSSE());
    printf("          SSE2 : %d\n", SDL_HasSSE2());
    printf("          SSE3 : %d\n", SDL_HasSSE3());
    printf("        SSE4.1 : %d\n", SDL_HasSSE41());
    printf("        SSE4.2 : %d\n", SDL_HasSSE42());
    printf("           AVX : %d\n", SDL_HasAVX());
    printf("          AVX2 : %d\n", SDL_HasAVX2());
    printf("      AVX-512F : %d\n", SDL_HasAVX512F());
    printf("      ARM SIMD : %d\n", SDL_HasARMSIMD());
    printf("          NEON : %d\n", SDL_HasNEON());
    printf("           LSX : %d\n", SDL_HasLSX());
    printf("          LASX : %d\n", SDL_HasLASX());
    printf("           RAM : %d MB\n", SDL_GetSystemRAM());
    return 0;
}
