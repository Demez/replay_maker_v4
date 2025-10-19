#include "main.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <ole2.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()
#include <GL/GL.h>
#include <direct.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
// #include "imgui_impl_opengl3_loader.h"
#include "imgui_impl_opengl3.h"


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );


void*                         g_main_window = nullptr;

static bool                   g_mouse_in_window     = false;

bool                          g_main_window_focused = false;
static bool                   g_pause_window_events = false;

bool                          win32_register_drag_drop( HWND window );
void                          win32_remove_drag_drop( HWND window );

ATOM                          create_window_class( const wchar_t* class_name, WNDPROC wnd_proc, bool main_window, bool brush, int bg_brush );


void                          sys_pause_window_events( bool paused )
{
	// DWORD dw_style        = (DWORD)GetWindowLong( (HWND)g_main_window, GWL_STYLE );

	g_pause_window_events = paused;
}


LRESULT __stdcall win32_window_proc_main( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if ( g_pause_window_events )
	{
		if ( uMsg == WM_GETMINMAXINFO || uMsg == WM_SIZE )
			return S_FALSE;

		return DefWindowProc( hwnd, uMsg, wParam, lParam );
		//return S_OK;  // DefWindowProc( hwnd, uMsg, wParam, lParam );
	}

	// resize events
	switch ( uMsg )
	{
		case WM_DPICHANGED:
		case WM_SIZE:
		{
			window_on_resize();
			// fallthrough
		}
		case WM_PAINT:
		{
			window_render_all();
			break;
		}
	}

	return DefWindowProc( hwnd, uMsg, wParam, lParam );
}


// TODO: check out CS_CLASSDC
// https://learn.microsoft.com/en-us/windows/win32/winmsg/window-class-styles
static ATOM create_window_class( const wchar_t* class_name, WNDPROC wnd_proc, bool brush, int bg_brush )
{
	WNDCLASSEX wc = { 0 };
	ZeroMemory( &wc, sizeof( wc ) );

	wc.cbClsExtra = 0;
	wc.cbSize     = sizeof( wc );
	wc.cbWndExtra = 0;
	wc.hInstance  = GetModuleHandle( NULL );
	wc.hCursor    = NULL;
	wc.hIcon   = LoadIcon( NULL, IDI_APPLICATION );
	wc.hIconSm = LoadIcon( 0, IDI_APPLICATION );

	// wc.hbrBackground       = (HBRUSH)GetStockObject( bg_brush );  // does this affect perf? i hope not
	wc.hbrBackground  = brush ? (HBRUSH)GetStockObject( bg_brush ) : 0;  // does this affect perf? i hope not
	wc.style          = CS_HREDRAW | CS_VREDRAW;                         // redraw if size changes
	wc.style          = 0;                                               // redraw if size changes
	wc.lpszClassName  = class_name;
	wc.lpszMenuName   = 0;
	wc.lpfnWndProc    = wnd_proc;

	return RegisterClassEx( &wc );
}


// thanks win32
// According to the wiki, this function
// "Calculates the required size of the window rectangle, based on the desired size of the client rectangle.
// The window rectangle can then be passed to the CreateWindowEx function to create a window whose client area is the desired size."
//
// Without that, the window ends up smaller than it actually should be
// ex. 1280x720 becomes 1266x713
static void adjust_window_rect( DWORD dwStyle, int& width, int& height )
{
	RECT rect;
	rect.left   = 0;
	rect.top    = 0;
	rect.right  = width;
	rect.bottom = height;

	// https://stackoverflow.com/questions/34583160/winapi-createwindow-function-creates-smaller-windows-than-set
	// https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrectex
	AdjustWindowRectEx( &rect, dwStyle, 0, 0 );

	width  = rect.right - rect.left;
	height = rect.bottom - rect.top;
}


bool win32_create_windows( int width, int height )
{
	if ( OleInitialize( NULL ) != S_OK )
	{
		printf( "Plat_Init(): Failed to Initialize Ole\n" );
		return false;
	}

	ATOM wc_main  = create_window_class( L"replay_maker", win32_window_proc_main, false, NULL_BRUSH );

	if ( wc_main == 0 )
	{
		printf( "Failed to create window class\n" );
		return false;
	}

	HWND main_window;

	// create main window
	{
		const LPTSTR _ClassName( MAKEINTATOM( wc_main ) );

		//DWORD        dwStyle    = WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW | WS_EX_CONTROLPARENT | WS_EX_LAYERED;
		DWORD        dwStyle    = WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW | WS_EX_CONTROLPARENT;
		DWORD        dwExStyle = 0;

		int          new_width  = width;
		int          new_height = height;

		adjust_window_rect( dwStyle, new_width, new_height );

		main_window = CreateWindowEx(
		  dwExStyle,
		  _ClassName,
		  L"Replay Maker",
		  dwStyle,
		  CW_USEDEFAULT,
		  CW_USEDEFAULT,
		  new_width, new_height,
		  NULL, NULL,
		  GetModuleHandle( NULL ),
		  nullptr );

		if ( !main_window )
		{
			printf( "Failed to create window\n" );
			return false;
		}

		win32_register_drag_drop( main_window );
	}

	g_main_window = main_window;

	return true;
}


void win32_exit()
{
	if ( g_main_window )
	{
		win32_remove_drag_drop( (HWND)g_main_window );
		DestroyWindow( (HWND)g_main_window );
	}

	g_main_window = nullptr;

	OleUninitialize();
}


void win32_update()
{
	MSG msg;
	while ( ::PeekMessageA( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
	{
		::TranslateMessage( &msg );
		::DispatchMessage( &msg );
		if ( msg.message == WM_QUIT )
			g_running = false;
	}
}


void win32_run()
{
	// now we can show the main window
	ShowWindow( (HWND)g_main_window, SW_SHOWNORMAL );
}

