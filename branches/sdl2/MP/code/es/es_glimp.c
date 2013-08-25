/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef VCMODS_NOSDL
#ifdef USE_LOCAL_HEADERS
#  include "SDL.h"
#else
#  include <SDL.h>
#endif
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "../sys/sys_local.h"

typedef enum
{
   RSERR_OK,

   RSERR_INVALID_FULLSCREEN,
   RSERR_INVALID_MODE,

   RSERR_UNKNOWN
} rserr_t;

SDL_Window *SDL_window = NULL;
static SDL_GLContext *SDL_glContext = NULL;

void (* qglLockArraysEXT) (GLint first, GLsizei count) = NULL;
void (* qglUnlockArraysEXT) (void) = NULL;

extern void etc1_compress_tex_image(
   GLenum target,
   GLint level,
   GLenum internalformat,
   GLsizei width,
   GLsizei height,
   GLint border,
   GLenum format,
   GLenum type,
   const GLvoid *pixels);

static int isopaque(GLint width, GLint height, const GLvoid *pixels)
{
   unsigned char const *cpixels = (unsigned char const *)pixels;

   int i;

   for (i = 0; i < width * height; i++) {
      if (cpixels[i*4+3] != 0xff)
         return 0;
   }

   return 1;
}

void rgba4444_convert_tex_image(
   GLenum target,
   GLint level,
   GLenum internalformat,
   GLsizei width,
   GLsizei height,
   GLint border,
   GLenum format,
   GLenum type,
   const GLvoid *pixels)
{
   unsigned char const *cpixels = (unsigned char const *)pixels;

   unsigned short *rgba4444data = malloc(2 * width * height);

   int i;

   for (i = 0; i < width * height; i++) {
      unsigned char r,g,b,a;

      r = cpixels[4*i]>>4;
      g = cpixels[4*i+1]>>4;
      b = cpixels[4*i+2]>>4;
      a = cpixels[4*i+3]>>4;

      rgba4444data[i] = r << 12 | g << 8 | b << 4 | a;
   }

   glTexImage2D(target, level, format, width, height,border,format,GL_UNSIGNED_SHORT_4_4_4_4,rgba4444data);

   free(rgba4444data);
}

void myglTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
   static int opaque = 0;

   if (format == GL_RGB && type == GL_UNSIGNED_BYTE) {
      assert(level == 0);
      opaque = 1;

      etc1_compress_tex_image(target, level, format, width, height, border, format, type, pixels);
   } else if (format == GL_RGBA && type == GL_UNSIGNED_BYTE) {
      if (level == 0)
         opaque = isopaque(width, height, pixels);

      if (opaque)
         etc1_compress_tex_image(target, level, format, width, height, border, format, type, pixels);
      else
         rgba4444_convert_tex_image(target, level, format, width, height, border, format, type, pixels);
   } else
      assert(0);
}

static int firstclear = 1;

void myglClear(GLbitfield mask)
{
   if (firstclear) {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      firstclear = 0;
   }
}

/* TODO any other functions that need modifying for stereo? eg glReadPixels? */

static GLenum draw_buffer = GL_BACK;
static struct rect_t {
   GLint x, y;
   GLsizei w, h;
} viewport = {0, 0, -1, -1}, scissor = {0, 0, -1, -1};

static void fix_rect(struct rect_t *r)
{
   if (r->w == -1) { r->w = glConfig.vidWidth; }
   if (r->h == -1) { r->h = glConfig.vidHeight; }
}

static void fudge_rect(struct rect_t *out, const struct rect_t *in,
   int xshift, int xoffset)
{
   out->x = xoffset + (in->x >> xshift);
   out->y = in->y;
   out->w = (xoffset + ((in->x + in->w) >> xshift)) - out->x;
   out->h = in->h;
}

static void update_viewport_and_scissor(void)
{
   int xshift = 0, xoffset = 0;
   struct rect_t r;

   switch (draw_buffer) {
   case GL_BACK_LEFT:
      xshift = 1;
      break;
   case GL_BACK_RIGHT:
      xshift = 1;
      xoffset = glConfig.vidWidth >> 1;
      break;
   }

   fix_rect(&viewport);
   fudge_rect(&r, &viewport, xshift, xoffset);
   glViewport(r.x, r.y, r.w, r.h);

   fix_rect(&scissor);
   fudge_rect(&r, &scissor, xshift, xoffset);
   glScissor(r.x, r.y, r.w, r.h);
}

void myglDrawBuffer(GLenum mode)
{
   draw_buffer = mode;
   update_viewport_and_scissor();
}

void myglViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
   viewport.x = x;
   viewport.y = y;
   viewport.w = width;
   viewport.h = height;
   update_viewport_and_scissor();
}

void myglScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
   scissor.x = x;
   scissor.y = y;
   scissor.w = width;
   scissor.h = height;
   update_viewport_and_scissor();
}

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( void )
{
}

/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment( char *comment )
{
}

/*
===============
GLimp_Minimize

Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize(void)
{
}

/*
===============
GLimp_StartDriverAndSetMode
===============
*/

static EGLDisplay   g_EGLDisplay;
static EGLConfig    g_EGLConfig;
static EGLContext   g_EGLContext;
static EGLSurface   g_EGLWindowSurface;

static qboolean GLimp_StartDriverAndSetMode( int mode, qboolean fullscreen, NativeWindowType hWnd )
{
   /* TODO cleanup on failure... */

   /* EGL Setup */
   const EGLint s_configAttribs[] =
   {
      EGL_RED_SIZE,       5,
      EGL_GREEN_SIZE,     6,
      EGL_BLUE_SIZE,      5,
      EGL_ALPHA_SIZE,     0,
      EGL_DEPTH_SIZE,     16,
      EGL_STENCIL_SIZE,   0,
      EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
      EGL_SAMPLE_BUFFERS, 1,
      EGL_NONE
   };

   EGLint numConfigs;
   EGLint majorVersion;
   EGLint minorVersion;

#ifndef VCMODS_NOSDL
   if (!SDL_WasInit(SDL_INIT_VIDEO))
   {
      const char *driverName;

      if (SDL_Init(SDL_INIT_VIDEO) == -1)
      {
         ri.Printf( PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n",
               SDL_GetError());
         return qfalse;
      }

      driverName = SDL_GetCurrentVideoDriver( );
      ri.Printf( PRINT_ALL, "SDL using driver \"%s\"\n", driverName );
      Cvar_Set( "r_sdlDriver", driverName );

      if( SDL_GL_MakeCurrent( SDL_window, SDL_glContext ) < 0 ) {
	 ri.Printf( PRINT_DEVELOPER, "SDL_GL_MakeCurrent failed: %s\n", SDL_GetError( ) );
         return qfalse;
      }
   }
#endif

   g_EGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (g_EGLDisplay == EGL_NO_DISPLAY) {
      ri.Printf(PRINT_ALL, "eglGetDisplay() failed\n");
      return qfalse;
   }

   if (!eglInitialize(g_EGLDisplay, &majorVersion, &minorVersion)) {
      ri.Printf(PRINT_ALL, "eglInitialize() failed\n");
      return qfalse;
   }
   if (!eglSaneChooseConfigBRCM(g_EGLDisplay, s_configAttribs, &g_EGLConfig, 1, &numConfigs)) {
      ri.Printf(PRINT_ALL, "eglSaneChooseConfigBRCM() failed\n");
      return qfalse;
   }
   if (numConfigs == 0) {
      ri.Printf(PRINT_ALL, "Couldn't find suitable config!\n");
      return qfalse;
   }

   {
      EGLint r, g, b, a, depth, stencil, samples, sample_buffers;
      eglGetConfigAttrib(g_EGLDisplay, g_EGLConfig, EGL_RED_SIZE, &r);
      eglGetConfigAttrib(g_EGLDisplay, g_EGLConfig, EGL_GREEN_SIZE, &g);
      eglGetConfigAttrib(g_EGLDisplay, g_EGLConfig, EGL_BLUE_SIZE, &b);
      eglGetConfigAttrib(g_EGLDisplay, g_EGLConfig, EGL_ALPHA_SIZE, &a);
      eglGetConfigAttrib(g_EGLDisplay, g_EGLConfig, EGL_DEPTH_SIZE, &depth);
      eglGetConfigAttrib(g_EGLDisplay, g_EGLConfig, EGL_STENCIL_SIZE, &stencil);
      eglGetConfigAttrib(g_EGLDisplay, g_EGLConfig, EGL_SAMPLES, &samples);
      eglGetConfigAttrib(g_EGLDisplay, g_EGLConfig, EGL_SAMPLE_BUFFERS, &sample_buffers);
      ri.Printf(PRINT_ALL, "Chose EGL config %d: r=%d,g=%d,b=%d,a=%d, "
         "depth=%d,stencil=%d, samples=%d,sample_buffers=%d\n",
         (int)g_EGLConfig, r, g, b, a, depth, stencil, samples, sample_buffers);
   }

   g_EGLContext = eglCreateContext(g_EGLDisplay, g_EGLConfig, NULL, NULL);
   if (g_EGLContext == EGL_NO_CONTEXT) {
      ri.Printf(PRINT_ALL, "eglCreateContext() failed\n");
      return qfalse;
   }

   ri.Printf(PRINT_ALL, "Using native window %d\n", (int)hWnd);

   g_EGLWindowSurface = eglCreateWindowSurface(g_EGLDisplay, g_EGLConfig, hWnd, NULL);
   if (g_EGLWindowSurface == EGL_NO_SURFACE) {
      ri.Printf(PRINT_ALL, "eglCreateWindowSurface() failed\n");
      return qfalse;
   }

   eglMakeCurrent(g_EGLDisplay, g_EGLWindowSurface, g_EGLWindowSurface, g_EGLContext);

   {
      EGLint width, height, color, depth, stencil;
      eglQuerySurface(g_EGLDisplay, g_EGLWindowSurface, EGL_WIDTH, &width);
      eglQuerySurface(g_EGLDisplay, g_EGLWindowSurface, EGL_HEIGHT, &height);
      ri.Printf(PRINT_ALL, "Window size: %dx%d\n", width, height);
      eglGetConfigAttrib(g_EGLDisplay, g_EGLConfig, EGL_BUFFER_SIZE, &color);
      eglGetConfigAttrib(g_EGLDisplay, g_EGLConfig, EGL_DEPTH_SIZE, &depth);
      eglGetConfigAttrib(g_EGLDisplay, g_EGLConfig, EGL_STENCIL_SIZE, &stencil);
      glConfig.vidWidth = width;
      glConfig.vidHeight = height;
      glConfig.colorBits = color;
      glConfig.depthBits = depth;
      glConfig.stencilBits = stencil;
   }

   if(r_stereoEnabled->integer)
      glConfig.stereoEnabled = qtrue;
   else
      glConfig.stereoEnabled = qfalse;

   return qtrue;
}

static qboolean GLimp_HaveExtension(const char *ext)
{
   return qfalse;
}


/*
===============
GLimp_InitExtensions
===============
*/
static void GLimp_InitExtensions( void )
{
   if ( !r_allowExtensions->integer )
   {
      ri.Printf( PRINT_ALL, "* IGNORING OPENGL EXTENSIONS *\n" );
      return;
   }

   ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );

   glConfig.textureCompression = TC_NONE;

   // GL_EXT_texture_compression_s3tc
   if ( GLimp_HaveExtension( "GL_ARB_texture_compression" ) &&
        GLimp_HaveExtension( "GL_EXT_texture_compression_s3tc" ) )
   {
      if ( r_ext_compressed_textures->value )
      {
         glConfig.textureCompression = TC_S3TC_ARB;
         ri.Printf( PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n" );
      }
      else
      {
         ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_compression_s3tc\n" );
      }
   }
   else
   {
      ri.Printf( PRINT_ALL, "...GL_EXT_texture_compression_s3tc not found\n" );
   }

   // GL_S3_s3tc ... legacy extension before GL_EXT_texture_compression_s3tc.
   if (glConfig.textureCompression == TC_NONE)
   {
      if ( GLimp_HaveExtension( "GL_S3_s3tc" ) )
      {
         if ( r_ext_compressed_textures->value )
         {
            glConfig.textureCompression = TC_S3TC;
            ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
         }
         else
         {
            ri.Printf( PRINT_ALL, "...ignoring GL_S3_s3tc\n" );
         }
      }
      else
      {
         ri.Printf( PRINT_ALL, "...GL_S3_s3tc not found\n" );
      }
   }


   // GL_EXT_texture_env_add
   glConfig.textureEnvAddAvailable = qtrue;

   textureFilterAnisotropic = qfalse;
}

/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
void GLimp_Init( void )
{
   qboolean success = qtrue;

   Sys_GLimpInit( );

   // create the window and set up the context
   if( !GLimp_StartDriverAndSetMode( r_mode->integer, r_fullscreen->integer,
      (NativeWindowType)ri.Cvar_Get("vc_wnd", "0", CVAR_LATCH)->integer))
   {
      success = qfalse;
   }

   if( !success )
      ri.Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem\n" );

   // This values force the UI to disable driver selection
   glConfig.driverType = GLDRV_ICD;
   glConfig.hardwareType = GLHW_GENERIC;
   glConfig.deviceSupportsGamma = qfalse;

   // get our config strings
   Q_strncpyz( glConfig.vendor_string, (char *) qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
   Q_strncpyz( glConfig.renderer_string, (char *) qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
   if (*glConfig.renderer_string && glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] == '\n')
      glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] = 0;
   Q_strncpyz( glConfig.version_string, (char *) qglGetString (GL_VERSION), sizeof( glConfig.version_string ) );
   Q_strncpyz( glConfig.extensions_string, (char *) qglGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

   // initialize extensions
   GLimp_InitExtensions( );

   ri.Cvar_Get( "r_availableModes", "", CVAR_ROM );

   // This depends on SDL_INIT_VIDEO, hence having it here
   IN_Init( SDL_window );
}


/*
===============
GLimp_EndFrame

Responsible for doing a swapbuffers
===============
*/

int last = 0;
int mod = 0;

void GLimp_EndFrame( void )
{
   eglSwapBuffers(g_EGLDisplay, g_EGLWindowSurface);
   firstclear = 1;
/*
   if ((++mod & 31) == 0) {
      DWORD time = GetCurrentTime();
      char buf[64];

      sprintf(buf, "%d\n", (time-last) / 32);
      OutputDebugString(buf);

      last = time;
   }
*/
}



// No SMP - stubs
void GLimp_RenderThreadWrapper( void *arg )
{
}

qboolean GLimp_SpawnRenderThread( void (*function)( void ) )
{
   ri.Printf( PRINT_WARNING, "ERROR: SMP support was disabled at compile time\n");
   return qfalse;
}

void *GLimp_RendererSleep( void )
{
   return NULL;
}

void GLimp_FrontEndSleep( void )
{
}

void GLimp_WakeRenderer( void *data )
{
}

