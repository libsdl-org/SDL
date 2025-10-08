/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

SDL_PROC(void, glBindTexture, (GLenum, GLuint))
SDL_PROC(void, glBlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum))
SDL_PROC(void, glClear, (GLbitfield))
SDL_PROC(void, glClearColor, (GLclampf, GLclampf, GLclampf, GLclampf))
SDL_PROC(void, glDeleteTextures, (GLsizei, const GLuint *))
SDL_PROC(void, glDisable, (GLenum))
SDL_PROC(void, glDrawArrays, (GLenum, GLint, GLsizei))
SDL_PROC(void, glEnable, (GLenum))
SDL_PROC(void, glFinish, (void))
SDL_PROC(void, glGenTextures, (GLsizei, GLuint *))
SDL_PROC(const GLubyte *, glGetString, (GLenum))
SDL_PROC(GLenum, glGetError, (void))
SDL_PROC(void, glGetIntegerv, (GLenum, GLint *))
SDL_PROC(void, glPixelStorei, (GLenum, GLint))
SDL_PROC(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid *))
SDL_PROC(void, glScissor, (GLint, GLint, GLsizei, GLsizei))
SDL_PROC(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *))
SDL_PROC(void, glTexParameteri, (GLenum, GLenum, GLint))
SDL_PROC(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid *))
SDL_PROC(void, glViewport, (GLint, GLint, GLsizei, GLsizei))
