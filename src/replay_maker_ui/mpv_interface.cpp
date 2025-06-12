#include "main.h"

// some reference here
// https://github.com/mpv-player/mpv-examples/blob/master/libmpv/sdl/main.c

void*               g_mpv_module = nullptr;
mpv_handle*         g_mpv        = nullptr;
mpv_render_context* g_mpv_gl     = nullptr;

bool                g_wakeup_on_mpv_render_update, g_wakeup_on_mpv_events;

static char*        g_current_video = nullptr;

#define FUNC_PTR( func ) func##_t p_##func = nullptr

// function pointers
// client.h
FUNC_PTR( mpv_client_api_version );
FUNC_PTR( mpv_error_string );
FUNC_PTR( mpv_free );
FUNC_PTR( mpv_client_name );
FUNC_PTR( mpv_client_id );
FUNC_PTR( mpv_create );
FUNC_PTR( mpv_initialize );
FUNC_PTR( mpv_destroy );
FUNC_PTR( mpv_terminate_destroy );
FUNC_PTR( mpv_create_client );
FUNC_PTR( mpv_create_weak_client );
FUNC_PTR( mpv_load_config_file );
FUNC_PTR( mpv_get_time_ns );
FUNC_PTR( mpv_get_time_us );
FUNC_PTR( mpv_free_node_contents );
FUNC_PTR( mpv_set_option );
FUNC_PTR( mpv_set_option_string );
FUNC_PTR( mpv_command );
FUNC_PTR( mpv_command_node );
FUNC_PTR( mpv_command_ret );
FUNC_PTR( mpv_command_string );
FUNC_PTR( mpv_command_async );
FUNC_PTR( mpv_command_node_async );
FUNC_PTR( mpv_abort_async_command );
FUNC_PTR( mpv_set_property );
FUNC_PTR( mpv_set_property_string );
FUNC_PTR( mpv_del_property );
FUNC_PTR( mpv_set_property_async );
FUNC_PTR( mpv_get_property );
FUNC_PTR( mpv_get_property_string );
FUNC_PTR( mpv_get_property_osd_string );
FUNC_PTR( mpv_get_property_async );
FUNC_PTR( mpv_observe_property );
FUNC_PTR( mpv_unobserve_property );
FUNC_PTR( mpv_event_name );
FUNC_PTR( mpv_event_to_node );
FUNC_PTR( mpv_request_event );
FUNC_PTR( mpv_request_log_messages );
FUNC_PTR( mpv_wait_event );
FUNC_PTR( mpv_wakeup );
FUNC_PTR( mpv_set_wakeup_callback );
FUNC_PTR( mpv_wait_async_requests );
FUNC_PTR( mpv_hook_add );
FUNC_PTR( mpv_hook_continue );
FUNC_PTR( mpv_get_wakeup_pipe );

// render.h
FUNC_PTR( mpv_render_context_create );
FUNC_PTR( mpv_render_context_set_parameter );
FUNC_PTR( mpv_render_context_get_info );
FUNC_PTR( mpv_render_context_set_update_callback );
FUNC_PTR( mpv_render_context_update );
FUNC_PTR( mpv_render_context_render );
FUNC_PTR( mpv_render_context_report_swap );
FUNC_PTR( mpv_render_context_free );


#define LOAD_FUNC( func )                                            \
	p_##func = (func##_t)sys_load_func( g_mpv_module, #func );       \
	if ( p_##func == nullptr )                                       \
	{                                                                \
		wprintf( L"sys_load_func failed: %s\n", sys_get_error_w() ); \
		return false;                                                \
	}


bool load_mpv_dll()
{
	g_mpv_module = sys_load_library( L"libmpv-2.dll" );

	if ( g_mpv_module == nullptr )
	{
		wprintf( L"sys_load_library failed: %s\n", sys_get_error_w() );
		return false;
	}

	// load mpv function pointers

	// client.h
	LOAD_FUNC( mpv_client_api_version );
	LOAD_FUNC( mpv_error_string );
	LOAD_FUNC( mpv_free );
	LOAD_FUNC( mpv_client_name );
	LOAD_FUNC( mpv_client_id );
	LOAD_FUNC( mpv_create );
	LOAD_FUNC( mpv_initialize );
	LOAD_FUNC( mpv_destroy );
	LOAD_FUNC( mpv_terminate_destroy );
	LOAD_FUNC( mpv_create_client );
	LOAD_FUNC( mpv_create_weak_client );
	LOAD_FUNC( mpv_load_config_file );
	LOAD_FUNC( mpv_get_time_ns );
	LOAD_FUNC( mpv_get_time_us );
	LOAD_FUNC( mpv_free_node_contents );
	LOAD_FUNC( mpv_set_option );
	LOAD_FUNC( mpv_set_option_string );
	LOAD_FUNC( mpv_command );
	LOAD_FUNC( mpv_command_node );
	LOAD_FUNC( mpv_command_ret );
	LOAD_FUNC( mpv_command_string );
	LOAD_FUNC( mpv_command_async );
	LOAD_FUNC( mpv_command_node_async );
	LOAD_FUNC( mpv_abort_async_command );
	LOAD_FUNC( mpv_set_property );
	LOAD_FUNC( mpv_set_property_string );
	LOAD_FUNC( mpv_del_property );
	LOAD_FUNC( mpv_set_property_async );
	LOAD_FUNC( mpv_get_property );
	LOAD_FUNC( mpv_get_property_string );
	LOAD_FUNC( mpv_get_property_osd_string );
	LOAD_FUNC( mpv_get_property_async );
	LOAD_FUNC( mpv_observe_property );
	LOAD_FUNC( mpv_unobserve_property );
	LOAD_FUNC( mpv_event_name );
	LOAD_FUNC( mpv_event_to_node );
	LOAD_FUNC( mpv_request_event );
	LOAD_FUNC( mpv_request_log_messages );
	LOAD_FUNC( mpv_wait_event );
	LOAD_FUNC( mpv_wakeup );
	LOAD_FUNC( mpv_set_wakeup_callback );
	LOAD_FUNC( mpv_wait_async_requests );
	LOAD_FUNC( mpv_hook_add );
	LOAD_FUNC( mpv_hook_continue );
	LOAD_FUNC( mpv_get_wakeup_pipe );

	// render.h
	LOAD_FUNC( mpv_render_context_create );
	LOAD_FUNC( mpv_render_context_set_parameter );
	LOAD_FUNC( mpv_render_context_get_info );
	LOAD_FUNC( mpv_render_context_set_update_callback );
	LOAD_FUNC( mpv_render_context_update );
	LOAD_FUNC( mpv_render_context_render );
	LOAD_FUNC( mpv_render_context_report_swap );
	LOAD_FUNC( mpv_render_context_free );

	return true;
}


void unload_mpv_dll()
{
	p_mpv_destroy( g_mpv );
	g_mpv                    = nullptr;

	// clear mpv function pointers
	p_mpv_create             = nullptr;
	p_mpv_client_api_version = nullptr;

	sys_close_library( g_mpv_module );
}


static void on_mpv_events( void* ctx )
{
	g_wakeup_on_mpv_events = true;
}


static void on_mpv_render_update( void* ctx )
{
	g_wakeup_on_mpv_render_update = true;
}


bool start_mpv()
{
	// get the mpv version
	unsigned long mpv_version = p_mpv_client_api_version();
	printf( "mpv version: %lu\n", mpv_version );

	g_mpv = p_mpv_create();

	if ( g_mpv == nullptr )
	{
		printf( "mpv_create failed!\n" );
		return false;
	}

	// idk what this does, but calling this embeds mpv into the main window
	//p_mpv_set_option_string( g_mpv, "vo", "libmpv" );

	if ( p_mpv_initialize( g_mpv ) < 0 )
	{
		printf( "mpv_initialize failed!\n" );
		return false;
	}

	// When normal mpv events are available.
	p_mpv_set_wakeup_callback( g_mpv, on_mpv_events, NULL );

	// When there is a need to call mpv_render_context_update(), which can
	// request a new frame to be rendered.
	// (Separate from the normal event handling mechanism for the sake of
	//  users which run OpenGL on a different thread.)
	// p_mpv_render_context_set_update_callback( g_mpv_gl, on_mpv_render_update, NULL );

	int64_t wid = (s64)g_mpv_window;
	bool    yes = true;

	// attach to main window
	p_mpv_set_property( g_mpv, "wid", MPV_FORMAT_INT64, &wid );
	p_mpv_set_property( g_mpv, "keep-open", MPV_FORMAT_FLAG, &yes );

	return true;
}


void stop_mpv()
{
}


char* mpv_get_current_video()
{
	return g_current_video;
}


// ----------------------------------------------------


void mpv_cmd_set_video_zoom( float zoom )
{
	// convert float to string
	char zoom_str[ 16 ];
	gcvt( zoom, 4, zoom_str );

	const char* cmd[]   = { "set", "video-zoom", zoom_str, nullptr };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void mpv_cmd_add_video_zoom( float zoom )
{
	// convert float to string
	char zoom_str[ 16 ];
	gcvt( zoom, 4, zoom_str );

	const char* cmd[]   = { "set", "video-zoom", zoom_str, nullptr };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void mpv_cmd_loadfile( const char* file )
{
	printf( "loading file: %s\n", file );

	const char* cmd[]   = { "loadfile", file, NULL };
	int         cmd_ret = p_mpv_command( g_mpv, cmd );

	mpv_event*  event   = p_mpv_wait_event( g_mpv, 0.01f );

	if ( g_current_video != nullptr )
		free( g_current_video );

	g_current_video = util_strdup( file );

	get_media_info();
}


void mpv_cmd_toggle_playback()
{
	const char* cmd[]   = { "cycle", "pause", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void mpv_cmd_hook_window( void* window )
{
	int64_t wid = (s64)window;
	int ret = p_mpv_set_property( g_mpv, "wid", MPV_FORMAT_INT64, &wid );

	printf( "hooked window idk\n" );
}


void mpv_cmd_hook_window_mpv()
{
	int64_t wid = (s64)g_mpv_window;
	int     ret = p_mpv_set_property( g_mpv, "wid", MPV_FORMAT_INT64, &wid );
	printf( "hooked mpv window idk\n" );
}


void mpv_handle_error( int )
{
	//p_mpv_error_string()
}

