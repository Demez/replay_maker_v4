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
void*                         g_mpv_window  = nullptr;

HGLRC                         g_gl_context;
HDC                           g_graphics_handle;

int                           g_grabbed_divider_idx = -1;

ivec2                         g_mouse_pos;
static bool                   g_mouse_in_window     = false;

static ATOM                   g_wc_mpv              = 0;

static HCURSOR                g_cursor_default;
static HCURSOR                g_cursor_resize_v;
static HCURSOR                g_cursor_resize_h;

bool                          g_main_window_focused = false;

static bool                   g_pause_window_events = false;

void                          win32_render_imgui();
void                          win32_render_divider( u32 index );
int                           win32_mouse_in_divider();  // returns divider index
void                          win32_update_dividers();
void                          win32_on_resize();
void                          win32_render_all();

void                          win32_mpv_full_window_enter();
void                          win32_mpv_full_window_exit();
void                          win32_mpv_full_window_toggle();

bool                          win32_register_drag_drop( HWND window );
void                          win32_remove_drag_drop( HWND window );

ATOM                          create_window_class( const wchar_t* class_name, WNDPROC wnd_proc, bool main_window, bool brush, int bg_brush );


void                          sys_pause_window_events( bool paused )
{
	// DWORD dw_style        = (DWORD)GetWindowLong( (HWND)g_main_window, GWL_STYLE );

	g_pause_window_events = paused;
}


// ----------------------------------------------------


#define MPV_CMD( ... )                                              \
	{                                                               \
		const char* cmd[]   = { __VA_ARGS__, NULL };                \
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd ); \
	}


constexpr const char* MONITOR_ZOOM_HACK     = "1.1";
constexpr const char* NEW_MONITOR_ZOOM_HACK = "1.0";


// mpv key binds list
void handle_mpv_keybind( int key )
{
	switch ( key )
	{
		case ' ':
		{
			mpv_cmd_toggle_playback();
			break;
		}
		case 'f':
		case VK_NUMPAD4:
		{
			win32_mpv_full_window_toggle();
			break;
		}
		case VK_NUMPAD0:
		{
			MPV_CMD( "set", "video-zoom", "0" );
			MPV_CMD( "set", "video-pan-x", "0" );
			MPV_CMD( "set", "video-pan-y", "0" );
			break;
		}
		case VK_NUMPAD7:
		{
			MPV_CMD( "set", "video-zoom", MONITOR_ZOOM_HACK );
			MPV_CMD( "set", "video-pan-x", "0.25" );
			break;
		}
		case VK_NUMPAD8:
		{
			MPV_CMD( "set", "video-zoom", MONITOR_ZOOM_HACK );
			MPV_CMD( "set", "video-pan-x", "-0.25" );
			break;
		}
		case VK_NUMPAD9:
		{
			MPV_CMD( "set", "video-zoom", "1.0" );
			MPV_CMD( "set", "video-pan-x", "-0.25" );
			break;
		}
	}
}


static bool  g_fullscreen   = false;
static ivec2 g_old_mpv_size;
static ivec2 g_old_window_size;


void win32_mpv_full_window_enter()
{
	g_fullscreen = true;

	HWND window  = (HWND)g_main_window;

	// get window pos
	RECT window_rect;
	GetWindowRect( (HWND)g_main_window, &window_rect );

	// save old mpv window size
	g_old_mpv_size[ 0 ]    = g_mpv_size[ 0 ];
	g_old_mpv_size[ 1 ]    = g_mpv_size[ 1 ];

	g_old_window_size[ 0 ] = g_window_size[ 0 ];
	g_old_window_size[ 1 ] = g_window_size[ 1 ];

	// set mpv window size
	g_mpv_size[ 0 ]        = g_window_size[ 0 ];
	g_mpv_size[ 1 ]        = g_window_size[ 1 ];
}


void win32_mpv_full_window_exit()
{
	g_fullscreen = false;
	
	RECT window_rect;
	GetClientRect( (HWND)g_main_window, &window_rect );

	int width  = window_rect.right - window_rect.left;
	int height = window_rect.bottom - window_rect.top;

	// scale the mpv window size based on 
	int diff_x = width - g_old_window_size[ 0 ];
	int diff_y = height - g_old_window_size[ 1 ];

	// restore mpv window size
	g_mpv_size[ 0 ] = g_old_mpv_size[ 0 ] + diff_x;
	g_mpv_size[ 1 ] = g_old_mpv_size[ 1 ] + diff_y;

	win32_on_resize();
	win32_render_all();
}


void win32_mpv_full_window_toggle()
{
	if ( !g_fullscreen )
	{
		win32_mpv_full_window_enter();
	}
	else
	{
		win32_mpv_full_window_exit();
	}
}


void win32_update_mpv_window_size()
{
	SetWindowPos( (HWND)g_mpv_window, HWND_TOP, 0, 0, g_mpv_size[ 0 ], g_mpv_size[ 1 ], SWP_NOZORDER | SWP_NOACTIVATE );
}


void enable_sidebar( bool enabled )
{
	g_show_sidebar           = enabled;

	static int old_mpv_width = g_mpv_size[ 0 ];

	RECT       window_rect;
	GetClientRect( (HWND)g_main_window, &window_rect );

	int width  = window_rect.right - window_rect.left;
	int height = window_rect.bottom - window_rect.top;

	if ( !enabled )
	{
		old_mpv_width   = g_mpv_size[ 0 ];
		g_mpv_size[ 0 ] = width;
	}
	else
	{
		g_mpv_size[ 0 ] = old_mpv_width;
	}

	win32_update_mpv_window_size();
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

	//if ( uMsg == WM_SETCURSOR )
	//{
	//	ImGui::SetMouseCursor( g_desired_cursor );
	//}
	
	ImGui_ImplWin32_WndProcHandler( hwnd, uMsg, wParam, lParam );

	// resize events
	switch ( uMsg )
	{
		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpMMI      = (LPMINMAXINFO)lParam;
			lpMMI->ptMinTrackSize.x = 640;
			lpMMI->ptMinTrackSize.y = 480;
		}

		case WM_MOUSEMOVE:
		{
			g_mouse_pos[ 0 ] = GET_X_LPARAM( lParam );
			g_mouse_pos[ 1 ] = GET_Y_LPARAM( lParam );
			g_mouse_in_window = true;

			win32_update_dividers();
		}

		case WM_MOUSELEAVE:
		{
			g_mouse_in_window = false;
		}

		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		{
			ImGuiIO& io = ImGui::GetIO();

			if ( io.WantCaptureKeyboard || io.WantTextInput )
			{
				ImGui_ImplWin32_WndProcHandler( hwnd, uMsg, wParam, lParam );
			}
			else if ( uMsg == WM_KEYDOWN )
			{
				handle_mpv_keybind( wParam );
			}

			break;
		}

		case WM_SETFOCUS:
		{
			g_main_window_focused = true;
			break;
		}

		case WM_KILLFOCUS:
		{
			// clear what is grabbed
			g_grabbed_divider_idx  = -1;
			g_main_window_focused = false;
			break;
		}

		case WM_DPICHANGED:
		case WM_SIZE:
		{
			win32_on_resize();
			// DO NOT BREAK HERE
		}
		case WM_PAINT:
		{
			win32_render_all();
			break;
		}


		case WM_CLOSE:
		case WM_QUIT:
		{
			g_running = false;
			break;
		}
	}

	return DefWindowProc( hwnd, uMsg, wParam, lParam );
}


LRESULT __stdcall win32_window_proc_mpv( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if ( g_pause_window_events )
	{
		if ( uMsg == WM_GETMINMAXINFO )
			return S_OK;

		return DefWindowProc( hwnd, uMsg, wParam, lParam );
		//return S_OK;  // DefWindowProc( hwnd, uMsg, wParam, lParam );
	}

	switch ( uMsg )
	{
		case WM_MOUSEMOVE:
		{
			g_mouse_pos[ 0 ]  = GET_X_LPARAM( lParam );
			g_mouse_pos[ 1 ]  = GET_Y_LPARAM( lParam );
			g_mouse_in_window = true;

			win32_update_dividers();
		}

		case WM_KEYDOWN:
		{
			if ( !g_fullscreen )
				break;

			handle_mpv_keybind( wParam );
			break;
		}
		case WM_LBUTTONDOWN:
		{
			win32_update_dividers();

			if ( g_grabbed_divider_idx == -1 )
			{
				mpv_cmd_toggle_playback();
			}
		}
	}

	return DefWindowProc( hwnd, uMsg, wParam, lParam );
}


// TODO: check out CS_CLASSDC
// https://learn.microsoft.com/en-us/windows/win32/winmsg/window-class-styles
static ATOM create_window_class( const wchar_t* class_name, WNDPROC wnd_proc, bool main_window, bool brush, int bg_brush )
{
	WNDCLASSEX wc = { 0 };
	ZeroMemory( &wc, sizeof( wc ) );

	wc.cbClsExtra = 0;
	wc.cbSize     = sizeof( wc );
	wc.cbWndExtra = 0;
	wc.hInstance  = GetModuleHandle( NULL );
	wc.hCursor    = NULL;

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
	wc.hbrBackground  = brush ? (HBRUSH)GetStockObject( bg_brush ) : 0;  // does this affect perf? i hope not
	wc.style          = CS_HREDRAW | CS_VREDRAW;                         // redraw if size changes
	wc.style          = 0;                                               // redraw if size changes
	wc.lpszClassName  = class_name;
	wc.lpszMenuName   = 0;
	wc.lpfnWndProc    = wnd_proc;

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


static bool create_gl_context( HWND window )
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

	g_graphics_handle = ::GetDC( window );
	g_gl_context      = wglCreateContext( g_graphics_handle );

	BOOL contextSuccess     = wglMakeCurrent( g_graphics_handle, g_gl_context );
}


int g_win32_color_index[ 3 ] = {
	FOREGROUND_RED,
	FOREGROUND_GREEN,
	FOREGROUND_BLUE,
};


int win32_parse_color( int rgb[ 3 ] )
{
	int final_color = 0;

	for ( int i = 0; i < 3; i++ )
	{
		if ( rgb[ i ] == 0 )
			continue;

		final_color |= g_win32_color_index[ i ];
	}

	// check intensity by getting the max value
	// int max_color = MAX( MAX( rgb[ 0 ], rgb[ 1 ] ), rgb[ 2 ] );
	int max_color = ( rgb[ 0 ] + rgb[ 1 ] + rgb[ 2 ] ) / 3;

	// if ( max_color > 7 )
	// 	final_color |= FOREGROUND_INTENSITY;

	int count   = 0;
	int average_added = 0;
	
	// ^for ( int i = 0; i < 3; i++ )
	// ^{
	// ^	if ( rgb[ i ] > 0 )
	// ^	{
	// ^		average_added += rgb[ i ];
	// ^	}
	// ^}
	// ^
	// ^int average = average_added / count;
	// ^
	// ^if ( average > 7 )
	// ^	final_color |= FOREGROUND_INTENSITY;

	return final_color;
}


bool win32_create_windows( int width, int height )
{
//	HANDLE con_out = GetStdHandle( STD_OUTPUT_HANDLE );
//
//	for ( int r = 0; r < 0xF; r++ )
//	{
//		for ( int g = 0; g < 0xF; g++ )
//		{
//			for ( int b = 0; b < 0xF; b++ )
//			{
//				int rgb[ 3 ]          = { r, g, b };
//				int win32_color_flags = win32_parse_color( rgb );
//
//				SetConsoleTextAttribute( con_out, win32_color_flags );
//				printf( "COLOR TEST - %x %x %x - %d %d %d\n", r, g, b, r, g, b );
//			}
//		}
//	}
//
//	SetConsoleTextAttribute( con_out, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE );


	if ( OleInitialize( NULL ) != S_OK )
	{
		printf( "Plat_Init(): Failed to Initialize Ole\n" );
		return false;
	}
	
	g_cursor_default  = LoadCursor( 0, IDC_ARROW );
	g_cursor_resize_v = LoadCursor( 0, IDC_SIZENS );
	g_cursor_resize_h = LoadCursor( 0, IDC_SIZEWE );

	ATOM wc_main  = create_window_class( L"replay_maker", win32_window_proc_main, true, false, NULL_BRUSH );
	g_wc_mpv      = create_window_class( L"replay_maker_mpv", win32_window_proc_mpv, false, true, BLACK_BRUSH );

	if ( wc_main == 0 ||  g_wc_mpv == 0 )
	{
		printf( "Failed to create window classes\n" );
		return false;
	}

	HWND main_window;
	HWND mpv_window;

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

	// create mpv window
	{
		const LPTSTR _ClassName( MAKEINTATOM( g_wc_mpv ) );

		// DWORD        dwStyle   = WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_EX_CONTROLPARENT | WS_EX_NOACTIVATE;
		// DWORD        dwStyle   = WS_CHILD | WS_VISIBLE | WS_THICKFRAME | WS_EX_CONTROLPARENT;
		DWORD        dwStyle   = WS_CHILD | WS_VISIBLE | WS_EX_CONTROLPARENT;
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
			sys_print_last_error();
			return false;
		}

		// SetParent( mpv_window, main_window );
		ShowWindow( mpv_window, SW_SHOWNORMAL );

		win32_register_drag_drop( mpv_window );
	}

	g_main_window = main_window;
	g_mpv_window  = mpv_window;

	if ( !create_gl_context( main_window ) )
	{
		printf( "Failed to create gl context for imgui\n" );
		sys_print_last_error();
		return false;
	}

	return true;
}


void win32_exit()
{
	// shutdown opengl
	wglMakeCurrent( g_graphics_handle, 0 );
	wglDeleteContext( g_gl_context );

	if ( g_main_window )
	{
		win32_remove_drag_drop( (HWND)g_main_window );
		DestroyWindow( (HWND)g_main_window );
	}

	if ( g_mpv_window )
	{
		win32_remove_drag_drop( (HWND)g_mpv_window );
		DestroyWindow( (HWND)g_mpv_window );
	}

	g_main_window = nullptr;
	g_mpv_window  = nullptr;

	OleUninitialize();
}


void win32_render_divider( u32 index )
{
//	RECT main_window_rect;
//	GetClientRect( (HWND)g_main_window, &main_window_rect );
//
//	int main_width  = main_window_rect.right - main_window_rect.left;
//	int main_height = main_window_rect.bottom - main_window_rect.top;
//
//	// vertical divider
//	if ( index == 0 )
//	{
//		// video controls divider
//		SetWindowPos( (HWND)g_imgui_window_dividers[ index ], HWND_TOP, 0, g_mpv_size[ 1 ], main_width, DIVIDER_SIZE, SWP_NOCOPYBITS | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOACTIVATE );
//	}
//	// horizontal divider
//	else if ( index == 1 )
//	{
//		// replay info divider
//		SetWindowPos( (HWND)g_imgui_window_dividers[ index ], HWND_TOP, g_mpv_size[ 0 ], 0, DIVIDER_SIZE, g_mpv_size[ 1 ], SWP_NOCOPYBITS | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOACTIVATE );
//	}
}


void win32_render_imgui()
{
	RECT main_window_rect;
	GetClientRect( (HWND)g_main_window, &main_window_rect );

	int    main_width        = main_window_rect.right - main_window_rect.left;
	int    main_height       = main_window_rect.bottom - main_window_rect.top;

	int    window_size[ 2 ] = { main_width, main_height };

	ImVec4 clear_color = ImVec4( 0.1f, 0.1f, 0.1f, 1.00f );

	BOOL ret = wglMakeCurrent( g_graphics_handle, g_gl_context );

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	draw_imgui_window( window_size );
	
	// Rendering
	ImGui::Render();
	glViewport( 0, 0, main_width, main_height );
	glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
	glClear( GL_COLOR_BUFFER_BIT );

	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
	
	// Present
	::SwapBuffers( g_graphics_handle );
}


void win32_on_resize()
{
	// calc new window size
	ivec2 old_window_size = { g_window_size[ 0 ], g_window_size[ 1 ] };
	ivec2 new_window_size = { 0, 0 };

	RECT main_window_rect;
	GetClientRect( (HWND)g_main_window, &main_window_rect );

	new_window_size[ 0 ] = main_window_rect.right - main_window_rect.left;
	new_window_size[ 1 ] = main_window_rect.bottom - main_window_rect.top;

	ivec2 size_diff{};
	size_diff[ 0 ]  = new_window_size[ 0 ] - old_window_size[ 0 ];
	size_diff[ 1 ]  = new_window_size[ 1 ] - old_window_size[ 1 ];

	// calc new mpv window size
	g_mpv_size[ 0 ] += size_diff[ 0 ];
	g_mpv_size[ 1 ] += size_diff[ 1 ];

	g_window_size[ 0 ] = new_window_size[ 0 ];
	g_window_size[ 1 ] = new_window_size[ 1 ];

	// win32_update_mpv_window_size();
}


void win32_render_all()
{
	// win32_on_resize();

	// update window dividers
	if ( !g_fullscreen )
	{
		win32_render_imgui();
	}

	win32_update_mpv_window_size();
}


struct rect_t
{

};


static bool point_in_rect( ivec2 point, RECT rect )
{
	return point[ 0 ] >= rect.left && point[ 0 ] <= rect.right && point[ 1 ] <= rect.bottom && point[ 1 ] >= rect.top;
}


int win32_mouse_in_divider()
{
	// calc divider rectangle
	RECT main_window_rect;
	GetClientRect( (HWND)g_main_window, &main_window_rect );

	int  window_width  = main_window_rect.right - main_window_rect.left;
	int  window_height = main_window_rect.bottom - main_window_rect.top;

	// rectangle 0 - playback controls divider
	RECT rect_0;
	rect_0.top    = g_mpv_size[ 1 ] - DIVIDER_SIZE;
	rect_0.bottom = g_mpv_size[ 1 ] + DIVIDER_SIZE;
	rect_0.left   = 0;
	rect_0.right  = g_mpv_size[ 0 ];

	// rectangle 1 - replay info
	RECT rect_1;
	rect_1.top    = 0;
	rect_1.bottom = window_height;
	rect_1.left   = g_mpv_size[ 0 ] - DIVIDER_SIZE;
	rect_1.right  = g_mpv_size[ 0 ] + DIVIDER_SIZE;

	ImGuiIO& io = ImGui::GetIO();

	if ( point_in_rect( g_mouse_pos, rect_0 ) )
	{
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse;
		SetCursor( g_cursor_resize_v );
		return 0;
	}
	else if ( g_show_sidebar && point_in_rect( g_mouse_pos, rect_1 ) )
	{
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse;
		SetCursor( g_cursor_resize_h );
		return 1;
	}

	io.ConfigFlags &= ~( ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse );
	ImGui::SetMouseCursor( ImGuiMouseCursor_Arrow );
	SetCursor( g_cursor_default );
	return -1;

	//if ( point_in_rect( g_mouse_pos, rect_0 ) )
	//{
	//	return 0;
	//}
	//else if ( point_in_rect( g_mouse_pos, rect_1 ) )
	//{
	//	return 1;
	//}
	//
	//return -1;
}


void win32_move_divider( u32 index )
{
//	ImGuiIO& io = ImGui::GetIO();
//
//	if ( index == 0 )
//	{
//		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse;
//		SetCursor( g_cursor_resize_v );
//	}
//	else if ( index == 1 )
//	{
//		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse;
//		SetCursor( g_cursor_resize_h );
//	}
//	else
//	{
//		io.ConfigFlags &= ~( ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse );
//		ImGui::SetMouseCursor( ImGuiMouseCursor_Arrow );
//		SetCursor( g_cursor_default );
//	}

	// check if we released the key, we could release it outside the window mid drag, as it lags behind
	if ( ( GetKeyState( VK_LBUTTON ) & 0x8000 ) == 0 )
	{
		g_grabbed_divider_idx = -1;
		return;
	}

	g_grabbed_divider_idx = index;

	POINT cursor_main;
	GetCursorPos( &cursor_main );

	RECT main_window_client_rect;
	GetClientRect( (HWND)g_main_window, &main_window_client_rect );

	ScreenToClient( (HWND)g_main_window, &cursor_main );

	if ( index == 0 )
	{
		// vertical divider
		//cursor_main.y -= g_grab_cursor_offset[ 1 ];
		g_mpv_size[ 1 ] = CLAMP( cursor_main.y, 0L, main_window_client_rect.bottom - main_window_client_rect.top );
	}
	else if ( index == 1 )
	{
		// horizontal divider
		//cursor_main.x -= g_grab_cursor_offset[ 0 ];
		g_mpv_size[ 0 ] = CLAMP( cursor_main.x, 0L, main_window_client_rect.right - main_window_client_rect.left );
	}

	// update mpv window size
	win32_update_mpv_window_size();

	win32_render_all();
}


void win32_update_dividers()
{
#if 0
	ImGuiIO& io = ImGui::GetIO();

	if ( g_grabbed_divider_idx != -1 )
	{
		win32_move_divider( g_grabbed_divider_idx );

		io.ConfigFlags &= ~( ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse );
		ImGui::SetMouseCursor( ImGuiMouseCursor_Arrow );
		SetCursor( g_cursor_default );

		return;
	}

	int div_index = win32_mouse_in_divider();

	if ( div_index < 0 )
	{
		io.ConfigFlags &= ~( ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse );
		ImGui::SetMouseCursor( ImGuiMouseCursor_Arrow );
		SetCursor( g_cursor_default );
	}

	win32_move_divider( div_index );
#else
	//if ( !g_mouse_in_window )
	//	return;

	// calc divider rectangle
	RECT main_window_rect;
	GetClientRect( (HWND)g_main_window, &main_window_rect );

	int window_width  = main_window_rect.right - main_window_rect.left;
	int window_height = main_window_rect.bottom - main_window_rect.top;

	// rectangle 0 - playback controls divider
	RECT rect_0;
	rect_0.top    = g_mpv_size[ 1 ] - DIVIDER_SIZE;
	rect_0.bottom = g_mpv_size[ 1 ] + DIVIDER_SIZE;
	rect_0.left   = 0;
	rect_0.right  = g_mpv_size[ 0 ];

	// rectangle 1 - replay info
	RECT rect_1;
	rect_1.top    = 0;
	rect_1.bottom = window_height;
	rect_1.left   = g_mpv_size[ 0 ] - DIVIDER_SIZE;
	rect_1.right  = g_mpv_size[ 0 ] + DIVIDER_SIZE;

	ImGuiIO& io   = ImGui::GetIO();

	if ( g_grabbed_divider_idx != -1 )
	{
		win32_move_divider( g_grabbed_divider_idx );
		return;
	}

	// NOTE: not perfect, windows keeps setting the cursor back to default even though im not using a cursor in a window class, hmm
	if ( g_grabbed_divider_idx == 0 || point_in_rect( g_mouse_pos, rect_0 ) )
	{
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse;
		SetCursor( g_cursor_resize_v );
		win32_move_divider( 0 );
	}
	else if ( g_show_sidebar && ( g_grabbed_divider_idx == 1 || point_in_rect( g_mouse_pos, rect_1 ) ) )
	{
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse;
		SetCursor( g_cursor_resize_h );
		win32_move_divider( 1 );
	}
	else if ( io.ConfigFlags & ( ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse ) ) 
	{
		io.ConfigFlags &= ~(ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse);
		ImGui::SetMouseCursor( ImGuiMouseCursor_Arrow );
		SetCursor( g_cursor_default );
		g_grabbed_divider_idx = -1;
	}
#endif
}


void win32_run()
{
	// do one render to get everything set up
	win32_render_all();

	// now we can show the main window
	ShowWindow( (HWND)g_main_window, SW_SHOWNORMAL );

	while ( g_running )
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// See the WndProc() function below for our to dispatch events to the Win32 backend.
		MSG msg;
		while ( ::PeekMessageA( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
		{
			::TranslateMessage( &msg );
			::DispatchMessage( &msg );
			if ( msg.message == WM_QUIT )
				g_running = false;
		}

		if ( !g_running )
			break;

		mpv_event* event = p_mpv_wait_event( g_mpv, 0.01f );

		// is the window minimized
		if ( ::IsIconic( (HWND)g_main_window ) )
		{
			::Sleep( 10 );
			continue;
		}

		if ( !g_fullscreen )
		{
			//win32_update_dividers();

			if ( g_grabbed_divider_idx != -1 )
			{
				win32_move_divider( g_grabbed_divider_idx );
			}

			// update window dividers
			//for ( u32 i = 0; i < g_imgui_window_count; i++ )
			//{
			//	win32_render_divider( i );
			//}
		}
		else
		{
			win32_update_mpv_window_size();
		}

		// run imgui windows
		if ( !g_fullscreen )
		{
			win32_render_imgui();
		}
	}
}

