#include "main.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdlib.h>
#include <Windows.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()
#include <GL/GL.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
// #include "imgui_impl_opengl3_loader.h"
#include "imgui_impl_opengl3.h"


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

// Data stored per platform window
struct gl_window_data_t
{
	HGLRC gl_context;
	HDC   graphics_handle;
	int   size[ 2 ];
};


void*             g_main_window             = nullptr;
void*             g_mpv_window              = nullptr;

// array of hwnds for imgui windows
window_id*        g_imgui_window            = nullptr;
window_id*        g_imgui_window_borders    = nullptr;
//window_border_data_t* g_imgui_window_border_data = nullptr;
int               g_imgui_window_count      = 0;

int               g_hovered_border_idx      = -1;
int               g_grabbed_border_idx      = -1;
int               g_grab_cursor_offset[ 2 ] = { 0, 0 };

// array of gl hdc types for imgui windows
gl_window_data_t* g_imgui_window_gl         = nullptr;

void              win32_render_imgui_window( u32 index );
void              win32_render_border( u32 index );
void              win32_render_all();
//void                  win32_register_drag_drop( HWND window );


module sys_load_library( const wchar_t* path )
{
	return (module)LoadLibrary( path );
}


void sys_close_library( module mod )
{
	FreeLibrary( (HMODULE)mod );
}


void* sys_load_func( module mod, const char* name )
{
	return GetProcAddress( (HMODULE)mod, name );
}


const wchar_t* sys_get_error()
{
	DWORD errorID = GetLastError();

	if ( errorID == 0 )
		return L"";  // No error message

	LPSTR strErrorMessage = NULL;

	FormatMessage(
	  FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_ALLOCATE_BUFFER,
	  NULL,
	  errorID,
	  0,
	  (LPTSTR)&strErrorMessage,
	  0,
	  NULL );

	static wchar_t message[ 512 ];
	memset( message, 512, 0 );
	_snwprintf( message, 512, L"Win32 API Error %ud: %s", errorID, strErrorMessage );

	// Free the Win32 string buffer.
	LocalFree( strErrorMessage );

	return message;
}


void win32_print_last_error()
{
	fprintf( stderr, "Error: %s\n", sys_get_error() );
}


void win32_move_border( u32 index )
{
	// check if we released the key, we could release it outside the window mid drag, as it lags behind
	if ( ( GetKeyState( VK_LBUTTON ) & 0x8000 ) == 0 )
	{
		g_grab_cursor_offset[ 0 ] = 0;
		g_grab_cursor_offset[ 1 ] = 0;
		g_grabbed_border_idx = -1;
		return;
	}

	POINT cursor_main;
	GetCursorPos( &cursor_main );

	RECT main_window_client_rect;
	GetClientRect( (HWND)g_main_window, &main_window_client_rect );

	ScreenToClient( (HWND)g_main_window, &cursor_main );

	if ( index == 0 )
	{
		// vertical border
		cursor_main.y -= g_grab_cursor_offset[ 1 ];
		g_mpv_height = CLAMP( cursor_main.y, 0L, main_window_client_rect.bottom - main_window_client_rect.top );
	}
	else if ( index == 1 )
	{
		// horizontal border
		cursor_main.x -= g_grab_cursor_offset[ 0 ];
		g_mpv_width = CLAMP( cursor_main.x, 0L, main_window_client_rect.right - main_window_client_rect.left );
	}

	// update mpv window size
	SetWindowPos( (HWND)g_mpv_window, HWND_TOP, 0, 0, g_mpv_width, g_mpv_height, SWP_NOZORDER | SWP_NOACTIVATE );

	win32_render_all();
}


LRESULT __stdcall Win32_WindowProc_Main( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	// resize events
	switch ( uMsg )
	{
		case WM_KILLFOCUS:
		{
			// clear what is grabbed
			g_grabbed_border_idx = -1;
			break;
		}

		case WM_DPICHANGED:
		case WM_SIZE:
		case WM_PAINT:
		{
			win32_render_all();
			break;
		}
	}

	return DefWindowProc( hwnd, uMsg, wParam, lParam );
}

LRESULT __stdcall Win32_WindowProc_ImGui( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if ( !g_imgui_contexts )
	{
		return DefWindowProc( hwnd, uMsg, wParam, lParam );
	}

	u32 i = 0;
	for ( ; i < g_imgui_window_count; i++ )
	{
		if ( g_imgui_window[ i ] == hwnd )
		{
			ImGui::SetCurrentContext( g_imgui_contexts[ i ] );
			break;
		}
	}

	if ( i == g_imgui_window_count )
	{
		printf( "Failed to find imgui context for window\n" );
	}
	else
	{
		ImGui_ImplWin32_WndProcHandler( hwnd, uMsg, wParam, lParam );
	}

	// resize events
	switch ( uMsg )
	{
		// https://devblogs.microsoft.com/oldnewthing/20150504-00/?p=44944
		// NOT DONE YET !!!!!!
	//	case WM_NCHITTEST:
	//	{
	//		int xPos = GET_X_LPARAM( lParam );
	//		int yPos = GET_Y_LPARAM( lParam );
	//
	//		UINT ht   = FORWARD_WM_NCHITTEST( hwnd, xPos, yPos, DefWindowProc );
	//		switch ( ht )
	//		{
	//			case HTBOTTOMLEFT: ht = HTBOTTOM; break;
	//			case HTBOTTOMRIGHT: ht = HTBOTTOM; break;
	//			case HTTOPLEFT: ht = HTTOP; break;
	//			case HTTOPRIGHT: ht = HTTOP; break;
	//			case HTLEFT: ht = HTBORDER; break;
	//			case HTRIGHT:
	//				ht = HTBORDER;
	//				break;
	//		}
	//
	//		break;
	//	}

		case WM_DPICHANGED:
		case WM_SIZE:
		case WM_PAINT:
		{
			// %win32_render_imgui_window( i );

			break;
		}
	}

	return DefWindowProc( hwnd, uMsg, wParam, lParam );
}


LRESULT __stdcall Win32_WindowProc_Border( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	u32 i = 0;
	for ( ; i < g_imgui_window_count; i++ )
	{
		if ( g_imgui_window_borders[ i ] == hwnd )
			break;
	}

	if ( i == g_imgui_window_count )
	{
		printf( "Failed to find imgui window border hwnd!\n" );
		return DefWindowProc( hwnd, uMsg, wParam, lParam );
	}

	//window_border_data_t& border_data = g_imgui_window_border_data[ i ];

	// resize events
	switch ( uMsg )
	{
		case WM_LBUTTONDOWN:
		{
			// get mouse pos
			g_grab_cursor_offset[ 0 ] = GET_X_LPARAM( lParam );
			g_grab_cursor_offset[ 1 ] = GET_Y_LPARAM( lParam );

			if ( g_hovered_border_idx != i )
				break;

			g_grabbed_border_idx = i;
			break;
		}

		case WM_LBUTTONUP:
		case WM_NCLBUTTONUP:
		{
			g_grab_cursor_offset[ 0 ] = 0;
			g_grab_cursor_offset[ 1 ] = 0;
			g_grabbed_border_idx = -1;
			break;
		}

		case WM_MOUSEMOVE:
		{
			g_hovered_border_idx = i;

			if ( g_grabbed_border_idx != i )
				break;

			win32_move_border( i );
			win32_render_border( i );
		}

		case WM_MOUSELEAVE:
		{
			g_hovered_border_idx = -1;
			break;
		}

		case WM_DPICHANGED:
		case WM_SIZE:
		case WM_PAINT:
		{
			win32_render_border( i );
			break;
		}
	}

	return DefWindowProc( hwnd, uMsg, wParam, lParam );
}


LRESULT __stdcall Win32_WindowProc_MPV( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return DefWindowProc( hwnd, uMsg, wParam, lParam );
}


void* get_proc_address_mpv( void* fn_ctx, const char* name )
{
	// void* test = wglGetProcAddress( name );
	// return test;
	return (void*)GetProcAddress( nullptr, name );
}


// TODO: check out CS_CLASSDC
// https://learn.microsoft.com/en-us/windows/win32/winmsg/window-class-styles
static ATOM create_window_class( const wchar_t* class_name, WNDPROC wnd_proc, bool main_window, bool brush, int bg_brush, LPWSTR cursor = IDC_ARROW )
{
	WNDCLASSEX wc = { 0 };
	ZeroMemory( &wc, sizeof( wc ) );

	wc.cbClsExtra          = 0;
	wc.cbSize              = sizeof( wc );
	wc.cbWndExtra          = 0;
	wc.hInstance           = GetModuleHandle( NULL );
	wc.hCursor             = LoadCursor( NULL, cursor );

	if ( main_window )
	{
		wc.hIcon   = LoadIcon( NULL, IDI_APPLICATION );
		wc.hIconSm = LoadIcon( 0, IDI_APPLICATION );
	}
	else
	{
		wc.hIcon   = 0;
		wc.hIconSm = 0;
	}

	// wc.hbrBackground       = (HBRUSH)GetStockObject( bg_brush );  // does this affect perf? i hope not
	wc.hbrBackground       = brush ? (HBRUSH)GetStockObject( bg_brush ) : 0;  // does this affect perf? i hope not
	wc.style               = CS_HREDRAW | CS_VREDRAW;             // redraw if size changes
	wc.style               = 0;             // redraw if size changes
	wc.lpszClassName       = class_name;
	wc.lpszMenuName        = 0;
	wc.lpfnWndProc         = wnd_proc;

	ATOM window_class = RegisterClassEx( &wc );

	if ( window_class == 0 )
	{
		// somehow not running on windows nt?
		printf( "Failed to create window class\n" );
		return 0;
	}

	return window_class;
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


static bool create_gl_context( HWND window, gl_window_data_t& gl_data )
{
	HDC                   hDc = ::GetDC( window );
	PIXELFORMATDESCRIPTOR pfd = { 0 };
	pfd.nSize                 = sizeof( pfd );
	pfd.nVersion              = 1;
	pfd.dwFlags               = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType            = PFD_TYPE_RGBA;
	pfd.cColorBits            = 32;

	const int pf              = ::ChoosePixelFormat( hDc, &pfd );

	if ( pf == 0 )
		return false;

	if ( ::SetPixelFormat( hDc, pf, &pfd ) == FALSE )
		return false;

	::ReleaseDC( window, hDc );

	gl_data.graphics_handle = ::GetDC( window );
	gl_data.gl_context      = wglCreateContext( gl_data.graphics_handle );

	BOOL contextSuccess     = wglMakeCurrent( gl_data.graphics_handle, gl_data.gl_context );
}


bool win32_create_windows( int width, int height, int imgui_window_count )
{
	ATOM wc_main  = create_window_class( L"replay_maker", Win32_WindowProc_Main, true, false, NULL_BRUSH );
	ATOM wc_imgui = create_window_class( L"replay_maker_imgui", Win32_WindowProc_ImGui, false, false, NULL_BRUSH );
	ATOM wc_mpv   = create_window_class( L"replay_maker_mpv", Win32_WindowProc_MPV, false, true, BLACK_BRUSH );

	ATOM wc_border[ 2 ];
	wc_border[ 0 ] = create_window_class( L"replay_maker_element_border_v", Win32_WindowProc_Border, false, true, LTGRAY_BRUSH, IDC_SIZENS );
	wc_border[ 1 ] = create_window_class( L"replay_maker_element_border_h", Win32_WindowProc_Border, false, true, LTGRAY_BRUSH, IDC_SIZEWE );

	if ( wc_main == 0 || wc_imgui == 0 || wc_mpv == 0 || wc_border[ 0 ] == 0 || wc_border[ 1 ] == 0 )
	{
		printf( "Failed to create window classes\n" );
		return false;
	}

	HWND main_window;
	HWND mpv_window;

	// create main window
	{
		const LPTSTR _ClassName( MAKEINTATOM( wc_main ) );

		DWORD        dwStyle    = WS_CLIPCHILDREN | WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_EX_CONTROLPARENT | WS_EX_LAYERED;
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
	}

	// create mpv window
	{
		const LPTSTR _ClassName( MAKEINTATOM( wc_mpv ) );

		// DWORD        dwStyle   = WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_EX_CONTROLPARENT | WS_EX_NOACTIVATE;
		// DWORD        dwStyle   = WS_CHILD | WS_VISIBLE | WS_THICKFRAME | WS_EX_CONTROLPARENT;
		DWORD        dwStyle   = WS_CHILD | WS_VISIBLE | WS_EX_CONTROLPARENT | WS_EX_NOACTIVATE;
		DWORD        dwExStyle = 0;

		int width   = 800;
		int height  = 600;

		adjust_window_rect( dwStyle, width, height );

		mpv_window = CreateWindowEx(
		  dwExStyle,
		  _ClassName,
		  L"Replay Maker - MPV Window",
		  dwStyle,
		  CW_USEDEFAULT,
		  CW_USEDEFAULT,
		  width, height,
		  main_window,
		  NULL,
		  GetModuleHandle( NULL ),
		  nullptr );

		if ( !mpv_window )
		{
			printf( "Failed to create mpv window\n" );
			win32_print_last_error();
			return false;
		}

		// SetParent( mpv_window, main_window );
		ShowWindow( mpv_window, SW_SHOWNORMAL );
	}

	g_main_window = main_window;
	g_mpv_window  = mpv_window;

	// create imgui windows
	g_imgui_window             = (window_id*)malloc( sizeof( window_id ) * imgui_window_count );
	g_imgui_window_borders     = (window_id*)malloc( sizeof( window_id ) * imgui_window_count );
	//g_imgui_window_border_data = (window_border_data_t*)malloc( sizeof( window_border_data_t ) * imgui_window_count );
	g_imgui_window_gl          = (gl_window_data_t*)malloc( sizeof( gl_window_data_t ) * imgui_window_count );
	g_imgui_window_count       = imgui_window_count;

	if ( !g_imgui_window || !g_imgui_window_borders || !g_imgui_window_gl )
	{
		printf( "Failed to allocate memory for imgui windows\n" );
		return false;
	}

	memset( g_imgui_window, 0, sizeof( window_id ) * imgui_window_count );
	memset( g_imgui_window_borders, 0, sizeof( window_id ) * imgui_window_count );
	//memset( g_imgui_window_border_data, 0, sizeof( window_border_data_t ) * imgui_window_count );
	memset( g_imgui_window_gl, 0, sizeof( gl_window_data_t ) * imgui_window_count );

	for ( u32 i = 0; i < imgui_window_count; i++ )
	{
		const LPTSTR _ClassName( MAKEINTATOM( wc_imgui ) );

		// DWORD        dwStyle   = WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_EX_CONTROLPARENT | WS_EX_NOACTIVATE;
		// DWORD        dwStyle   = WS_CHILD | WS_VISIBLE | WS_THICKFRAME | WS_EX_CONTROLPARENT;
		DWORD        dwStyle   = WS_CHILD | WS_VISIBLE | WS_EX_CONTROLPARENT;
		// DWORD        dwStyle   = WS_VISIBLE | WS_OVERLAPPEDWINDOW;
		DWORD        dwExStyle = 0;

		int          width     = 400;
		int          height    = 400;

		adjust_window_rect( dwStyle, width, height );

		HWND window = CreateWindowEx(
			dwExStyle,
			_ClassName,
			L"Replay Maker - ImGui Window",
			dwStyle,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			width, height,
			main_window,
			NULL,
			GetModuleHandle( NULL ),
			nullptr );

		if ( !window )
		{
			printf( "Failed to create imgui window\n" );
			win32_print_last_error();
			return false;
		}

		// SetParent( window, main_window );
		ShowWindow( window, SW_SHOWNORMAL );

		if ( !create_gl_context( window, g_imgui_window_gl[ i ] ) )
		{
			printf( "Failed to create gl context for imgui window\n" );
			win32_print_last_error();
			return false;
		}

		g_imgui_window[ i ] = window;
	}

	// make the borders between the windows
	for ( u32 i = 0; i < imgui_window_count; i++ )
	{
		const LPTSTR _ClassName( MAKEINTATOM( wc_border[ i ] ) );

		// DWORD        dwStyle   = WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_EX_CONTROLPARENT | WS_EX_NOACTIVATE;
		DWORD        dwStyle   = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_EX_CONTROLPARENT;
		// DWORD        dwStyle   = WS_VISIBLE | WS_OVERLAPPEDWINDOW;
		DWORD        dwExStyle = 0;

		int          width     = 400;
		int          height    = 400;

		adjust_window_rect( dwStyle, width, height );

		HWND window = CreateWindowEx(
		  dwExStyle,
		  _ClassName,
		  L"Replay Maker - ImGui Window Border",
		  dwStyle,
		  CW_USEDEFAULT,
		  CW_USEDEFAULT,
		  width, height,
		  main_window,
		  NULL,
		  GetModuleHandle( NULL ),
		  nullptr );

		if ( !window )
		{
			printf( "Failed to create imgui border window\n" );
			win32_print_last_error();
			return false;
		}

		g_imgui_window_borders[ i ] = window;
	}

	return true;
}


void win32_render_border( u32 index )
{
	RECT main_window_rect;
	GetClientRect( (HWND)g_main_window, &main_window_rect );

	int main_width  = main_window_rect.right - main_window_rect.left;
	int main_height = main_window_rect.bottom - main_window_rect.top;

	// vertical border
	if ( index == 0 )
	{
		// video controls border
		SetWindowPos( (HWND)g_imgui_window_borders[ index ], HWND_TOP, 0, g_mpv_height, main_width, BORDER_SIZE, SWP_NOCOPYBITS | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOACTIVATE );
	}
	// horizontal border
	else if ( index == 1 )
	{
		int new_width  = main_width - g_mpv_width;
		int new_height = main_height - g_mpv_height;

		// replay info border
		SetWindowPos( (HWND)g_imgui_window_borders[ index ], HWND_TOP, g_mpv_width, 0, BORDER_SIZE, g_mpv_height, SWP_NOCOPYBITS | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOACTIVATE );
	}
}


void win32_render_imgui_window( u32 index )
{
	RECT main_window_rect;
	GetClientRect( (HWND)g_main_window, &main_window_rect );

	RECT window_rect;
	GetClientRect( (HWND)g_imgui_window[ index ], &window_rect );

	int    main_width  = main_window_rect.right - main_window_rect.left;
	int    main_height = main_window_rect.bottom - main_window_rect.top;

	int width  = window_rect.right - window_rect.left;
	int height = window_rect.bottom - window_rect.top;

	int element_size[ 2 ] = { 0, 0 };

	ImVec4 clear_color = ImVec4( 0.1f, 0.1f, 0.1f, 1.00f );

	ImGui::SetCurrentContext( g_imgui_contexts[ index ] );

	wglMakeCurrent( g_imgui_window_gl[ index ].graphics_handle, g_imgui_window_gl[ index ].gl_context );

	int video_height = g_mpv_height + BORDER_SIZE;
	int video_width  = g_mpv_width + BORDER_SIZE;

	if ( index == 0 )
	{
		element_size[ 0 ] = main_width;
		element_size[ 1 ] = main_height - video_height;

		// video controls
		SetWindowPos( (HWND)g_imgui_window[ index ], HWND_BOTTOM, 0, main_height - element_size[ 1 ], element_size[ 0 ], element_size[ 1 ], SWP_DRAWFRAME | SWP_NOZORDER | SWP_NOACTIVATE );
	}
	else if ( index == 1 )
	{
		element_size[ 0 ] = main_width - video_width;
		element_size[ 1 ] = video_height - BORDER_SIZE;

		// replay info
		SetWindowPos( (HWND)g_imgui_window[ index ], HWND_BOTTOM, video_width, 0, element_size[ 0 ], element_size[ 1 ], SWP_DRAWFRAME | SWP_NOZORDER | SWP_NOACTIVATE );
	}


	// ImGui::GetIO().DisplaySize = ImVec2( (float)width, (float)height );

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	draw_imgui_window( index, element_size );
	
	// Rendering
	ImGui::Render();
	glViewport( 0, 0, width, height );
	glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
	glClear( GL_COLOR_BUFFER_BIT );
	
	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
	
	// Present
	::SwapBuffers( g_imgui_window_gl[ index ].graphics_handle );
}


void win32_render_all()
{
	// update window borders
	for ( u32 i = 0; i < g_imgui_window_count; i++ )
	{
		win32_render_border( i );
	}

	// run imgui windows
	for ( u32 i = 0; i < g_imgui_window_count; i++ )
	{
		win32_render_imgui_window( i );
	}
}


void win32_run()
{
	ImVec4 clear_color = ImVec4( 0.45f, 0.55f, 0.60f, 1.00f );
	bool done = false;


	// SetWindowPos( (HWND)g_mpv_window, HWND_TOP, 40, 40, 500, 400, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );

	while ( !done )
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// See the WndProc() function below for our to dispatch events to the Win32 backend.
		MSG msg;
		while ( ::PeekMessageA( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
		{
			::TranslateMessage( &msg );
			::DispatchMessage( &msg );
			if ( msg.message == WM_QUIT )
				done = true;
		}

		if ( done )
			break;

		if ( ::IsIconic( (HWND)g_main_window ) )
		{
			::Sleep( 10 );
			continue;
		}

		if ( g_grabbed_border_idx != -1 )
		{
			win32_move_border( g_grabbed_border_idx );
		}

		// update window borders
		for ( u32 i = 0; i < g_imgui_window_count; i++ )
		{
			win32_render_border( i );
		}

		// update mpv window size

		// run main logic

		// run imgui windows
		for ( u32 i = 0; i < g_imgui_window_count; i++ )
		{
			// imgui window drawing

			win32_render_imgui_window( i );
		}

		// SetWindowPos( (HWND)g_mpv_window, HWND_TOP, 40, 40, 500, 400, SWP_DRAWFRAME | SWP_NOZORDER | SWP_NOACTIVATE );

		// Start the Dear ImGui frame
	// 	ImGui_ImplOpenGL3_NewFrame();
	// 	ImGui_ImplWin32_NewFrame();
	// 	ImGui::NewFrame();
	// 	
	// 	ImGui::ShowDemoWindow();
	// 	
	// 	// Rendering
	// 	ImGui::Render();
	// 	glViewport( 0, 0, 512, 512 );
	// 	glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
	// 	//glClear( GL_COLOR_BUFFER_BIT );
	// 	
	// 	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
	// 	
	// 	// Present
	// 	::SwapBuffers( g_main_window_gl.hDC );
	}
}

