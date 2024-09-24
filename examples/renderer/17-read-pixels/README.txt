This example creates an SDL window and renderer, and draws a
rotating texture to it, reads back the rendered pixels, converts them to
black and white, and then draws the converted image to a corner of the
screen.

This isn't necessarily an efficient thing to do--in real life one might
want to do this sort of thing with a render target--but it's just a visual
example of how to use SDL_RenderReadPixels().

A better, but less visual, use of SDL_RenderReadPixels() is to make
screenshots: you grab the current contents of the screen, and save the pixels
as a bitmap file or whatever.
