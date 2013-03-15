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

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "../sys/sys_local.h"

// q3rev.cpp : Defines the entry point for the application.
//

#ifdef _WIN32
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/*
===============
IN_TranslateSDLToQ3Key
===============
*/
static keyNum_t IN_TranslateWinToQ3Key( int vk )
{
	switch( vk )
	{
		case VK_LEFT:         return K_LEFTARROW;
		case VK_RIGHT:        return K_RIGHTARROW;
		case VK_DOWN:         return K_DOWNARROW;
		case VK_UP:           return K_UPARROW;
		case VK_RETURN:       return K_ENTER;
      case VK_CONTROL:      
         return K_CTRL;
      case VK_SHIFT:
         return K_SHIFT;
      case VK_ESCAPE:
         return K_ESCAPE;
	}

   if (vk >= 'A' && vk <= 'Z' || vk >= '0' && vk <= '9')
      return vk;

	return 0;
}

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);

LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= NULL;
	wcex.hCursor		= NULL;
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= "Q3CLS";
	wcex.hIconSm		= NULL;

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//

extern HINSTANCE g_hInstance;

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hWnd = CreateWindow("Q3CLS", "Quake 3", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	const char *character = NULL;
	keyNum_t key = 0;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_KEYDOWN:
		key = IN_TranslateWinToQ3Key( wParam );

		if( key )
			Com_QueueEvent( 0, SE_KEY, key, qtrue, 0, NULL );
		break;
	case WM_KEYUP:
		key = IN_TranslateWinToQ3Key( wParam );

		if( key )
			Com_QueueEvent( 0, SE_KEY, key, qfalse, 0, NULL );
		break;
   case WM_CHAR:
		Com_QueueEvent( 0, SE_CHAR, wParam, 0, 0, NULL );
      break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

/*
===============
IN_ProcessEvents
===============
*/
static void IN_ProcessEvents( void )
{
   MSG msg;

   if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
      if (!GetMessage(&msg, NULL, 0, 0))
         Sys_Quit();

      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
}

/*
===============
IN_Init
===============
*/
void IN_Init( void )
{
	MyRegisterClass(g_hInstance);
   InitInstance (g_hInstance, SW_SHOW);
}

#else

#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <sys/select.h>

#define NB_DISABLE 	0
#define NB_ENABLE	1

int kbhit( void )
{
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
	select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
	return FD_ISSET(STDIN_FILENO, &fds);
}

void nonblock(int state)
{
	struct termios ttystate;

	//get the terminal state
	tcgetattr(STDIN_FILENO, &ttystate);

	if (state==NB_ENABLE)
	{
		//turn off canonical mode
		ttystate.c_lflag &= ~ICANON;
		//minimum of number input read.
		ttystate.c_cc[VMIN] = 1;
	}
	else if (state==NB_DISABLE)
	{
		//turn on canonical mode
		ttystate.c_lflag |= ICANON;
	}
	//set the terminal attributes.
	tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

/*
===============
IN_TranslateSDLToQ3Key
===============
*/
static keyNum_t IN_TranslateCharToQ3Key( char c )
{
   switch (c) {
   case ';':         return K_LEFTARROW;
   case '#':         return K_RIGHTARROW;
   case '\'':        return K_DOWNARROW;
   case '[':         return K_UPARROW;
   case ']':         return K_CONSOLE;
   case 10:          return K_ENTER;
   }

   return c;
}

/*
===============
IN_ProcessEvents
===============
*/
static void IN_ProcessEvents( void )
{
	while (kbhit()) {
		char c = fgetc(stdin);

		int key = IN_TranslateCharToQ3Key(c);

		Com_QueueEvent( 0, SE_CHAR, c, 0, 0, NULL );

		if( key ) {
			Com_QueueEvent( 0, SE_KEY, key, qtrue, 0, NULL );
			Com_QueueEvent( 0, SE_KEY, key, qfalse, 0, NULL );
		}
	}
}

/*
===============
IN_Init
===============
*/
void IN_Init( void )
{
	nonblock(NB_ENABLE);
}

#endif

/*
===============
IN_Frame
===============
*/
void IN_Frame( void )
{
	IN_ProcessEvents( );
}

/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown( void )
{
}

/*
===============
IN_Restart
===============
*/
void IN_Restart( void )
{
}
