/*
 *  common.h
 *  written by Holmes Futrell
 *  use however you want
 */

#if __TVOS__
#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#else
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480
#endif

extern int randomInt(int min, int max);
extern float randomFloat(float min, float max);
extern void fatalError(const char *string);
