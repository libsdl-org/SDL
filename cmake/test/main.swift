/* Contributed by Piotr Usewicz (https://github.com/pusewicz) */

import SDL3

guard SDL_Init(SDL_INIT_VIDEO) else {
  fatalError("SDL_Init error: \(String(cString: SDL_GetError()))")
}

var sdlVersion = SDL_GetVersion()

print("SDL version: \(sdlVersion)")

SDL_Quit()
