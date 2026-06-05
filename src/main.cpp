#include "main.h"
#include "args.h"

#include <locale.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_freetype.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3_loader.h"
#include "imgui_impl_opengl3.h"

#include "mpv/client.h"
#include "mpv/render_gl.h"

#include "clip/clip.h"

// native file dialog
#include "nfd.h"

#include <time.h>


namespace font
{
	ImFont* normal        = nullptr;
	ImFont* normal_bold   = nullptr;
	ImFont* normal_italic = nullptr;

	ImFont* console       = nullptr;

	u32     size          = 17;
}


namespace app
{
	// Window
	SDL_Window* window              = nullptr;
	ivec2       mpv_size            = { 0, 0 };
	ivec2       window_size         = { 0, 0 };

	// Mouse
	ivec2       mouse_pos           = { 0, 0 };
	ivec2       mouse_delta         = { 0, 0 };

	float       frame_time          = 0.f;
	float       save_timer          = -1.f;

	// States
	bool        running             = true;
	bool        fullscreen          = false;
	bool        in_window_drag      = false;
	bool        in_draw             = false;
	bool        pause_window_events = false;
	bool        sidebar             = true;
}

static SDL_GLContext g_gl_context          = nullptr;

char*                g_videos_file_path;

std::thread*         g_clip_load_thread       = nullptr;
e_clip_parse_state   g_clip_load_thread_state = e_clip_parse_state_idle;

static ivec2         g_old_mpv_size;
static ivec2         g_old_window_size;


// ============================================================================================


bool point_in_rect( ImVec2 point, ImVec2 min_size, ImVec2 max_size )
{
	// return point[ 0 ] >= min_size.left && point[ 0 ] <= rect.right && point[ 1 ] <= rect.bottom && point[ 1 ] >= rect.top;
	return point[ 0 ] >= min_size[ 0 ] && point[ 0 ] <= max_size[ 0 ] && point[ 1 ] <= max_size[ 1 ] && point[ 1 ] >= min_size[ 1 ];
}


bool mouse_in_rect( ImVec2 min_size, ImVec2 max_size )
{
	return point_in_rect( ImVec2( app::mouse_pos[ 0 ], app::mouse_pos[ 1 ] ), min_size, max_size );
}


bool mouse_hovering_area( ImVec2 min_size, ImVec2 max_size )
{
	//if ( g_hovered_divider )
	//	return false;

	bool area_hovered = mouse_in_rect( min_size, max_size );

	if ( area_hovered && ImGui::IsPopupOpen( "", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel ) )
	{
		bool          hovered_popup = false;
		ImGuiContext* context       = ImGui::GetCurrentContext();

		if ( context )
		{
			// Check popups to see if mouse is hovering over any of them
			for ( int i = 0; i < context->OpenPopupStack.Size; i++ )
			{
				ImGuiPopupData& data   = context->OpenPopupStack[ i ];
				ImGuiWindow*    window = data.Window;

				if ( mouse_in_rect( window->Pos, { window->Pos.x + window->Size.x, window->Pos.y + window->Size.y } ) )
					return false;
			}
		}
	}

	return area_hovered;
}


// ===============================================================================================
// Video List, Settings, Recently Opened


void save_settings()
{
	size_t      exe_dir_len           = 0;
	const char* exe_dir               = sys_get_exe_folder( &exe_dir_len );
	char        settings_path[ 4096 ] = { 0 };

	memcpy( settings_path, exe_dir, exe_dir_len * sizeof( char ) );
	strcat( settings_path, SEP_S "replay_maker_config.json5" );

	clip_save_settings( settings_path );
}


void save_videos()
{
	if ( !g_videos_file_path )
		return;

	if ( g_clip_load_thread_state != e_clip_parse_state_idle )
		return;

	if ( clip_save_videos( g_videos_file_path ) )
	{
		app::save_timer = 5000.f;
	}
}


void write_recently_opened()
{
#if 1
	char recent[ 4096 ] = { 0 };

	for ( u8 i = 0; i < g_recently_opened_count; i++ )
	{
		strcat( recent, g_recently_opened[ i ] );
		strcat( recent, "\n");
	}

	fs_save_file( g_recently_opened_path, recent, strlen( recent ) );
#else
	FILE* fp = fopen( g_recently_opened_path, "w" );

	if ( !fp )
	{
		printf( "Failed to write recently opened file!\n" );
		return;
	}

	for ( u8 i = 0; i < g_recently_opened_count; i++ )
	{
		fwrite( g_recently_opened[ i ], strlen( g_recently_opened[ i ] ) * sizeof( char ), 1, fp );
		fwrite( "\n", sizeof( char ), 1, fp );
	}

	fclose( fp );
#endif
}


void load_recently_opened()
{
	if ( !fs_is_file( g_recently_opened_path ) )
		return;

	size_t len             = 0;
	char*  recently_opened = fs_read_file( g_recently_opened_path, &len );

	// find out how many entries are here
	char*  line_end        = strchr( recently_opened, '\n' );
	char*  line_start      = recently_opened;

	while ( line_end )
	{
		if ( *( line_end - 1 ) == '\r' )
			line_end--;

		size_t dist = line_end - line_start;
		char*  file = ch_malloc< char >( dist + 1 );
		memcpy( file, line_start, dist * sizeof( char ) );
		file[ dist ] = '\0';

		g_recently_opened[ g_recently_opened_count++ ] = file;

		if ( *line_end == '\r' )
			line_end++;

		line_start = ++line_end;
		line_end   = strchr( line_start, '\n' );
	}

	free( recently_opened );
}


void update_recently_opened( const char* clips_file )
{
	// this could have come from in the list itself
	char* temp_dup = strdup( clips_file );

	// shift everything back
	if ( g_recently_opened_count > 0 )
	{
		// check to see if this is the top most file
		if ( strcmp( clips_file, g_recently_opened[ 0 ] ) == 0 )
		{
			free( temp_dup );
			return;
		}

		// check to see if this file was opened before
		bool found = false;
		for ( u8 i = 0; i < g_recently_opened_count; i++ )
		{
			// TODO: already exists here, reuse that pointer
			if ( strcmp( clips_file, g_recently_opened[ i ] ) == 0 )
			{
				// g_recently_opened[ 0 ] = g_recently_opened[ i ];
				free( g_recently_opened[ i ] );
				memcpy( &g_recently_opened[ i ], &g_recently_opened[ i + 1 ], sizeof( char* ) * ( ( MAX_RECENT_OPEN - 1 ) - i ) );
				found = true;
				break;
			}
		}

		if ( !found )
		{
			if ( g_recently_opened_count == MAX_RECENT_OPEN )
				free( g_recently_opened[ MAX_RECENT_OPEN - 1 ] );
			else
				g_recently_opened_count++;
		}

		// shift everything down by one
		memcpy( g_recently_opened + 1, g_recently_opened, ( MAX_RECENT_OPEN - 1 ) * sizeof( char* ) );
		memset( &g_recently_opened[ 0 ], 0, sizeof( char* ) );
	}
	else
	{
		if ( g_recently_opened_count < MAX_RECENT_OPEN )
			g_recently_opened_count++;
	}

	g_recently_opened[ 0 ] = temp_dup;

	write_recently_opened();
}


// ===============================================================================================
// Background Clip Data Parsing


void clip_thread_worker( char* path )
{
	clip_parse_videos( path );
	g_clip_load_thread_state = e_clip_parse_state_finished;
	free( path );
}


void clip_thread_open_file( const char* path )
{
	if ( g_clip_load_thread_state != e_clip_parse_state_idle )
	{
		return;
	}

	replay_editor_reset();

	g_clip_load_thread       = new std::thread( clip_thread_worker, strdup( path ) );
	g_clip_load_thread_state = e_clip_parse_state_running;
}


e_clip_parse_state clip_thread_state()
{
	if ( g_clip_load_thread_state == e_clip_parse_state_finished )
	{
		g_clip_load_thread->join();
		delete g_clip_load_thread;
		g_clip_load_thread       = nullptr;

		g_clip_load_thread_state = e_clip_parse_state_idle;
		return e_clip_parse_state_finished;
	}

	return g_clip_load_thread_state;
}


bool clip_thread_loading()
{
	e_clip_parse_state state = clip_thread_state();
	return state == e_clip_parse_state_running;
}


bool clip_thread_idle()
{
	e_clip_parse_state state = clip_thread_state();
	return state == e_clip_parse_state_idle;
}


// ===============================================================================================
// UI Sizing


void sys_mpv_full_window_enter()
{
	app::fullscreen        = true;

	// save old mpv window size
	g_old_mpv_size[ 0 ]    = app::mpv_size[ 0 ];
	g_old_mpv_size[ 1 ]    = app::mpv_size[ 1 ];

	g_old_window_size[ 0 ] = app::window_size[ 0 ];
	g_old_window_size[ 1 ] = app::window_size[ 1 ];

	// set mpv window size
	app::mpv_size[ 0 ]     = app::window_size[ 0 ];
	app::mpv_size[ 1 ]     = app::window_size[ 1 ];
}


void window_on_resize()
{
	// calc new window size
	ivec2 old_window_size = { app::window_size[ 0 ], app::window_size[ 1 ] };
	ivec2 new_window_size = { 0, 0 };

	SDL_GetWindowSize( app::window, &new_window_size[ 0 ], &new_window_size[ 1 ] );

	ivec2 size_diff{};
	size_diff[ 0 ] = new_window_size[ 0 ] - old_window_size[ 0 ];
	size_diff[ 1 ] = new_window_size[ 1 ] - old_window_size[ 1 ];

	// calc new mpv window size
	app::mpv_size[ 0 ] += size_diff[ 0 ];
	app::mpv_size[ 1 ] += size_diff[ 1 ];

	app::window_size[ 0 ] = new_window_size[ 0 ];
	app::window_size[ 1 ] = new_window_size[ 1 ];

	mpv_window_resize();
}


void sys_mpv_full_window_exit()
{
	app::fullscreen = false;

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	// scale the mpv window size based on
	int diff_x      = width - g_old_window_size[ 0 ];
	int diff_y      = height - g_old_window_size[ 1 ];

	// restore mpv window size
	app::mpv_size[ 0 ] = g_old_mpv_size[ 0 ] + diff_x;
	app::mpv_size[ 1 ] = g_old_mpv_size[ 1 ] + diff_y;

	window_on_resize();
	window_render_all();
}


void sys_mpv_full_window_toggle()
{
	if ( !app::fullscreen )
	{
		sys_mpv_full_window_enter();
	}
	else
	{
		sys_mpv_full_window_exit();
	}
}


void enable_sidebar( bool enabled )
{
	app::sidebar           = enabled;

	static int old_mpv_width = app::mpv_size[ 0 ];

	int        width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	if ( !enabled )
	{
		old_mpv_width   = app::mpv_size[ 0 ];
		app::mpv_size[ 0 ] = width;
	}
	else
	{
		app::mpv_size[ 0 ] = old_mpv_width;
	}

	mpv_window_resize();
}


void style_imgui()
{
	ImGuiStyle& style        = ImGui::GetStyle();

	// style.FramePadding.x     = 4;
	// style.FramePadding.y     = 4;

	style.WindowPadding.x    = 6;
	style.WindowPadding.y    = 6;
	style.ItemSpacing.x      = 6;
	style.ItemSpacing.y      = 6;
	style.ItemInnerSpacing.x = 6;
	style.ItemInnerSpacing.y = 6;

	style.ChildRounding      = 3;
	style.FrameRounding      = 3;
	style.GrabRounding       = 3;
	style.PopupRounding      = 3;
	style.ScrollbarRounding  = 3;
}


void update_dpi( float dpi_override )
{
	float scale = 0.f;

	if ( dpi_override == 0.f )
	{
		scale = std::max( 0.25f, SDL_GetWindowDisplayScale( app::window ) );
	}
	else
	{
		scale = CLAMP( dpi_override, 0.25f, 5.f );
	}

	ImGui::GetStyle() = ImGuiStyle();

	style_imgui();

	ImGui::GetStyle().ScaleAllSizes( scale );
	ImGui::GetStyle().FontScaleDpi = scale;
}


// ===============================================================================================


bool sdl_window_resize_watcher( void* userdata, SDL_Event* event )
{
	if ( app::in_draw || app::pause_window_events )
		return true;

	if ( SDL_GetWindowFlags( app::window ) & SDL_WINDOW_MINIMIZED )
		return true;

	switch ( event->type )
	{
		case SDL_EVENT_WINDOW_MINIMIZED:
			return true;

		case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
			update_dpi();
			break;

			// Redraw window - Window is being resized
			// NOTE: this is also called when dragging the window around
#ifdef _WIN32
		case SDL_EVENT_WINDOW_EXPOSED:
		{
			// clear focusing of any windows
			ImGui::SetNextFrameWantCaptureKeyboard( false );
			ImGui::SetWindowFocus( nullptr );

			app::in_window_drag = true;
			window_render_all();
			app::in_window_drag = false;
			break;
		}
#endif
		case SDL_EVENT_WINDOW_RESIZED:
		{
			// clear focusing of any windows
			ImGui::SetNextFrameWantCaptureKeyboard( false );
			ImGui::SetWindowFocus( nullptr );
			window_on_resize();
			window_render_all();
			break;
		}

		default:
			break;
	}

	return true;
}


void main_loop()
{
	ImGuiIO& io = ImGui::GetIO();

	u64      start_time   = sys_get_time_ms();
	u64      current_time = start_time;
	float    time         = 0.f;

	static std::string drop_file;

	while ( app::running )
	{
		app::mouse_delta[ 0 ] = 0;
		app::mouse_delta[ 1 ] = 0;

		// Handle Events
		SDL_Event event;
		while ( SDL_PollEvent( &event ) )
		{
			ImGui_ImplSDL3_ProcessEvent( &event );

			switch ( event.type )
			{
				// The system requests a file open
				case SDL_EVENT_DROP_FILE:
				{
					drop_file = event.drop.data;
					break;
				}

				// Current set of drops is now complete (NULL filename)
				case SDL_EVENT_DROP_COMPLETE:
				{
					replay_editor_load_loose_video( drop_file.c_str() );
					SDL_RaiseWindow( app::window );
					drop_file.clear();
					break;
				}

				case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
					update_dpi();
					break;

				case SDL_EVENT_MOUSE_MOTION:
					app::mouse_pos[ 0 ] = event.motion.x;
					app::mouse_pos[ 1 ] = event.motion.y;
					app::mouse_delta[ 0 ] += event.motion.xrel;
					app::mouse_delta[ 1 ] += event.motion.yrel;
					break;

#if !_WIN32
				case SDL_EVENT_WINDOW_RESIZED:
					int width, height;
					SDL_GetWindowSize( app::window_sdl, &width, &height );
					io.DisplaySize.x = width;
					io.DisplaySize.y = height;

					window_on_resize();
					break;
#endif

				case SDL_EVENT_QUIT:
				case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
					app::running = false;
					break;
			}
		}
		
		// -----------------------------------------------------------------------------------
		// Update Frame Time

		current_time    = sys_get_time_ms();
		app::frame_time = ( current_time / 1000.f ) - ( start_time / 1000.f );

		start_time      = current_time;

		// don't let the time go too crazy, usually happens when in a breakpoint
		// time                 = std::min( real_time, 0.1f );

		// g_total_time += ( time * 1000.f );

		if ( app::save_timer > 0.f )
			app::save_timer -= app::frame_time;

		if ( !app::running )
			break;

		// called so mpv doesn't get flooded with too many events, and becomes unresponsive
		mpv_event* mpv_event = p_mpv_wait_event( get_mpv(), 0 );

		while ( mpv_event && mpv_event->event_id != MPV_EVENT_NONE )
		{
			mpv_event = p_mpv_wait_event( get_mpv(), 0 );
		}

		// is the window minimized
		if ( SDL_GetWindowFlags( app::window ) & SDL_WINDOW_MINIMIZED )
		{
			SDL_Delay( 10 );
			continue;
		}

		window_render_all();

		handle_keybinds();
	}
}


void load_font( const char* path, int size, ImFont*& dst, ImFontConfig& font_cfg, bool load_symbols )
{
	font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;

	// Main Font
	dst = ImGui::GetIO().Fonts->AddFontFromFileTTF( path, size, &font_cfg );

#ifdef _WIN32
	// All fonts will be merged into this one above
	font_cfg.MergeMode = true;

	// Japanese Characters
	dst                = ImGui::GetIO().Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\YuGothM.ttc", size, &font_cfg );

	// Symbols/Emoji's
	if ( load_symbols )
	{
		// font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor | ImGuiFreeTypeLoaderFlags_Bitmap;
		font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;

		// Segoe UI Symbol
		dst = ImGui::GetIO().Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\seguisym.ttf", size, &font_cfg );

		//char font_path[ 512 ]{};
		//snprintf( font_path, 512, "%s/seguiemj.ttf", exe_path );

		// ImGui::GetIO().Fonts->AddFontFromFileTTF( font_path, font_data.height, &font_cfg );
		dst = ImGui::GetIO().Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\seguiemj.ttf", size, &font_cfg );
	}
#endif
}


auto main( int argc, char* argv[] ) -> int
{
	// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/setlocale-wsetlocale?view=msvc-170#utf-8-support
	// allows c ansi functions to use utf-8
	// only works on Windows 10 version 1803 (10.0.17134.0) and above
	// setlocale( LC_ALL, ".UTF-8" );
	args_init( argc, argv );

	sys_init();

	if ( !log_init() )
		return 1;

	if ( !undo_history_init() )
	{
		printf( "Failed to allocate undo history system\n" );
		return 1;
	}

	if ( NFD_Init() != NFD_OKAY )
	{
		printf( "Failed to Init NativeFileDialog\n" );
		return 1;
	}

	// ------------------------------------------
	// Load MPV First before anything else

	if ( !load_mpv_dll() )
	{
		printf( "load_mpv_dll failed!\n" );
		return 1;
	}

	// ------------------------------------------

	if ( !IMGUI_CHECKVERSION() )
	{
		printf( "Dear ImGui version mismatch!\n" );
		return 1;
	}

	if ( !SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO ) )
	{
		printf( "Failed to init SDL\n" );
		return 1;
	}

	// ------------------------------------------

	SET_INT2( app::window_size, 1600, 900 );

	// calculate the size of the mpv window (what about DPI Scale here later?)
	app::mpv_size[ 0 ] = app::window_size[ 0 ] - 600;  // replay editor/sidebar
	app::mpv_size[ 1 ] = app::window_size[ 1 ] - 240;  // playback controls

	app::window   = SDL_CreateWindow( "Replay Maker", app::window_size[ 0 ], app::window_size[ 1 ], SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN );

	if ( !app::window )
	{
		printf( "Failed to Create Main Window!\n" );
		return 1;
	}

	SDL_SetWindowMinimumSize( app::window, 800, 600 );

	sys_set_window( app::window );

	g_gl_context = SDL_GL_CreateContext( app::window );

	if ( !g_gl_context )
	{
		printf( "Failed to create GL Context\n" );
		return 1;
	}

	SDL_GL_MakeCurrent( app::window, g_gl_context );

	if ( !gladLoadGL() )
	{
		printf( "Failed to load GL\n" );
		return 1;
	}

	// SDL_GL_SetSwapInterval( 0 );
	
	// ------------------------------------------
	// init imgui

	if ( ImGui::CreateContext() == nullptr )
	{
		printf( "ImGui::CreateContext failed!\n" );
		return 1;
	}

	if ( !ImGui_ImplSDL3_InitForOpenGL( app::window, g_gl_context ) )
	{
		printf( "Failed to init ImGui\n" );
		return 1;
	}

	if ( !ImGui_ImplOpenGL3_Init() )
	{
		printf( "Failed to init ImGui OpenGL\n" );
		return 1;
	}

	size_t      exe_dir_len = 0;
	const char* exe_dir     = sys_get_exe_folder( &exe_dir_len );

#if _WIN32 && 1
	{
		const char* font_path = "C:\\Windows\\Fonts\\segoeui.ttf";

		{
			ImFontConfig font_cfg{};
			load_font( font_path, font::size, font::normal, font_cfg, false );
		}

		{
			ImFontConfig font_cfg{};
			snprintf( font_cfg.Name, 40, "Default - Bold" );
			font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bold;
			load_font( font_path, font::size, font::normal_bold, font_cfg, false );
		}

		{
			ImFontConfig font_cfg{};
			snprintf( font_cfg.Name, 40, "Default - Oblique" );
			font_cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Oblique;
			load_font( font_path, font::size, font::normal_italic, font_cfg, false );
		}
	}
#endif

	{
		char font_path[ 260 ] = { 0 };

		memcpy( font_path, exe_dir, exe_dir_len * sizeof( char ) );
		strcat( font_path, SEP_S "CascadiaCode.ttf" );

		ImFontConfig font_cfg{};
		load_font( font_path, font::size, font::console, font_cfg, false );
	}

	ImGui_ImplOpenGL3_CreateDeviceObjects();

	style_imgui();

	// ------------------------------------------
	// Startup and Load MPV

	set_mpv_count( 1 );
	set_mpv_extra_video();

	// ------------------------------------------

	{
		char settings_path[ 4096 ] = { 0 };

		memcpy( settings_path, exe_dir, exe_dir_len * sizeof( char ) );
		strcat( settings_path, SEP_S "replay_maker_config.json5" );

		clip_parse_settings( settings_path );
	}

	{
		char recent_path[ 256 ] = { 0 };

		memcpy( recent_path, exe_dir, exe_dir_len * sizeof( char ) );
		strcat( recent_path, SEP_S RECENTLY_OPENED_FILE );

		g_recently_opened_path = util_strdup( recent_path );
	}

//	{
//		char videos_path[ 4096 ] = { 0 };
//
//		memcpy( videos_path, exe_dir, exe_dir_len * sizeof( char ) );
//		strcat( videos_path, SEP_S "test_video.json5" );
//
//		clip_parse_videos( g_clip_data, videos_path );
//
//		g_videos_file_path = strdup( videos_path );
//	}

	g_recently_opened = ch_calloc< char* >( MAX_RECENT_OPEN );

	load_recently_opened();

	if ( argc > 1 )
	{
		if ( fs_is_file( argv[ 1 ] ) )
		{
			// assume these are clips
			clip_thread_open_file( argv[ 1 ] );

			update_recently_opened( argv[ 1 ] );
			g_videos_file_path = util_strdup( argv[ 1 ] );
		}
	}

	if ( !encode_init() )
	{
		printf( "encoder failed to start!\n" );
		return 1;
	}

	log_printf( "Startup Complete!\n" );

	if ( !SDL_AddEventWatch( sdl_window_resize_watcher, nullptr ) )
	{
		printf( "Failed to add SDL Event Watch\n" );
	}
	
	// do one render to get everything set up
	window_render_all();

	SDL_ShowWindow( app::window );

	main_loop();

	// ------------------------------------------
	// exit

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();

	ImGui::DestroyContext();

	sys_shutdown();

	// close mpv
	unload_mpv_dll();

	for ( u8 i = 0; i < g_recently_opened_count; i++ )
		free( g_recently_opened[ i ] );

	free( g_recently_opened );
	free( g_recently_opened_path );

	undo_history_shutdown();

	return 0;
}

