/*
  SDL Cocoa regression test for https://github.com/libsdl-org/SDL/issues/12141

  Run on macOS with the Cocoa video backend. This test intentionally doesn't
  require a renderer or a GPU device: the Metal view is checked for correct
  attachment and layout in the application's existing NSView hierarchy.
  It does not measure present latency or guarantee correct rendered pixels.
*/
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#include <stdio.h>
#include <stdlib.h>

static void check_condition(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s (SDL error: %s)\n", message, SDL_GetError());
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    @autoreleasepool {
        [NSApplication sharedApplication];

        NSWindow *host = [[NSWindow alloc]
            initWithContentRect:NSMakeRect(100, 100, 700, 500)
                      styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                NSWindowStyleMaskResizable
                        backing:NSBackingStoreBuffered
                          defer:NO];

        NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 700, 500)];
        [host setContentView:root];

        // Deliberately offset and smaller than the host content view.
        NSView *embedded = [[NSView alloc] initWithFrame:NSMakeRect(23, 41, 320, 240)];
        [root addSubview:embedded];
        [host makeKeyAndOrderFront:nil];

        check_condition(SDL_Init(SDL_INIT_VIDEO), "SDL_Init");
        SDL_PropertiesID props = SDL_CreateProperties();
        check_condition(props != 0, "SDL_CreateProperties");
        check_condition(SDL_SetPointerProperty(props,
                        SDL_PROP_WINDOW_CREATE_COCOA_VIEW_POINTER,
                        (__bridge void *)embedded), "set external Cocoa NSView");
        check_condition(SDL_SetNumberProperty(props,
                        SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER,
                        SDL_WINDOW_METAL), "request Metal-capable SDL window");

        SDL_Window *window = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
        check_condition(window != NULL, "SDL_CreateWindowWithProperties");
        check_condition(host.contentView == root, "SDL replaced the host content view");
        check_condition(embedded.superview == root, "SDL detached the supplied NSView");

        SDL_MetalView metal = SDL_Metal_CreateView(window);
        check_condition(metal != NULL, "SDL_Metal_CreateView");

        NSView *metalView = (__bridge NSView *)metal;
        check_condition(metalView.superview == embedded,
                        "Metal view should be a child of the supplied NSView");
        check_condition(NSEqualRects(metalView.frame, embedded.bounds),
                        "Metal view frame must be local to its parent bounds");
        CALayer *layer = (__bridge CALayer *)SDL_Metal_GetLayer(metal);
        check_condition([layer isKindOfClass:[CAMetalLayer class]],
                        "Metal view must have a CAMetalLayer");

        // Cover frame changes independently of the host window's dimensions.
        [embedded setFrameSize:NSMakeSize(400, 260)];
        [embedded layoutSubtreeIfNeeded];
        check_condition(NSEqualSizes(metalView.frame.size, embedded.bounds.size),
                        "Metal subview must follow parent resizing");

        // Existing Cocoa code can report host-window dimensions for embedded
        // views. Record the discrepancy but don't assert it in this scoped test.
        int logicalW = 0, logicalH = 0, pixelW = 0, pixelH = 0;
        SDL_GetWindowSize(window, &logicalW, &logicalH);
        SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);
        printf("SDL dimensions: logical %dx%d, pixel %dx%d; NSView %.0fx%.0f\n",
               logicalW, logicalH, pixelW, pixelH,
               embedded.bounds.size.width, embedded.bounds.size.height);

        SDL_Metal_DestroyView(metal);
        SDL_DestroyWindow(window);
        check_condition(host.contentView == root, "SDL destroyed the host root view");
        check_condition(embedded.superview == root, "SDL detached host subview on teardown");

        SDL_Quit();
        [host close];
        puts("PASS: external NSView remains embedded and Metal view uses its bounds");
    }
    return EXIT_SUCCESS;
}
