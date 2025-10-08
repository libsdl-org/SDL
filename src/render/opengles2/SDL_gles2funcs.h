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

SDL_PROC(void, glActiveTexture, (GLenum))
SDL_PROC(void, glAttachShader, (GLuint, GLuint))
SDL_PROC(void, glBindAttribLocation, (GLuint, GLuint, const char *))
SDL_PROC(void, glBlendEquationSeparate, (GLenum, GLenum))
SDL_PROC(void, glCompileShader, (GLuint))
SDL_PROC(GLuint, glCreateProgram, (void))
SDL_PROC(GLuint, glCreateShader, (GLenum))
SDL_PROC(void, glDeleteProgram, (GLuint))
SDL_PROC(void, glDeleteShader, (GLuint))
SDL_PROC(void, glDisableVertexAttribArray, (GLuint))
SDL_PROC(void, glEnableVertexAttribArray, (GLuint))
SDL_PROC(void, glGenFramebuffers, (GLsizei, GLuint *))
SDL_PROC(void, glGetProgramiv, (GLuint, GLenum, GLint *))
SDL_PROC(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, char *))
SDL_PROC(void, glGetShaderiv, (GLuint, GLenum, GLint *))
SDL_PROC(GLint, glGetUniformLocation, (GLuint, const char *))
SDL_PROC(void, glLinkProgram, (GLuint))
SDL_PROC(void, glShaderBinary, (GLsizei, const GLuint *, GLenum, const void *, GLsizei))
SDL_PROC(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *))
SDL_PROC(void, glUniform1i, (GLint, GLint))
SDL_PROC(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat))
SDL_PROC(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))
SDL_PROC(void, glUniformMatrix3fv, (GLint, GLsizei, GLboolean, const GLfloat *))
SDL_PROC(void, glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat *))
SDL_PROC(void, glUseProgram, (GLuint))
SDL_PROC(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *))
SDL_PROC(void, glBindFramebuffer, (GLenum, GLuint))
SDL_PROC(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))
SDL_PROC(GLenum, glCheckFramebufferStatus, (GLenum))
SDL_PROC(void, glDeleteFramebuffers, (GLsizei, const GLuint *))
SDL_PROC(GLint, glGetAttribLocation, (GLuint, const GLchar *))
SDL_PROC(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))
SDL_PROC(void, glGenBuffers, (GLsizei, GLuint *))
SDL_PROC(void, glDeleteBuffers, (GLsizei, const GLuint *))
SDL_PROC(void, glBindBuffer, (GLenum, GLuint))
SDL_PROC(void, glBufferData, (GLenum, GLsizeiptr, const GLvoid *, GLenum))
SDL_PROC(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const GLvoid *))