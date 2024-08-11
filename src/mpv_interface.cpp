#include "main.h"

// some reference here
// https://github.com/mpv-player/mpv-examples/blob/master/libmpv/sdl/main.c

void*               g_mpv_module = nullptr;
mpv_handle*         g_mpv        = nullptr;
mpv_render_context* g_mpv_gl     = nullptr;

bool                g_wakeup_on_mpv_render_update, g_wakeup_on_mpv_events;

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


#define LOAD_FUNC( func )                                          \
	p_##func = (func##_t)sys_load_func( g_mpv_module, #func );      \
	if ( p_##func == nullptr )                                     \
	{                                                              \
		printf( "SDL_LoadFunction failed: %s\n", sys_get_error() ); \
		return false;                                              \
	}


bool load_mpv_dll()
{
	g_mpv_module = sys_load_library( L"libmpv-2.dll" );

	if ( g_mpv_module == nullptr )
	{
		wprintf( L"SDL_LoadObject failed: %s\n", sys_get_error() );
		return false;
	}

//	p_mpv_free = (mpv_free_t)sys_load_func( g_mpv_module, "mpv_free" );
//	if ( p_mpv_free == nullptr )
//	{
//		printf( "SDL_LoadFunction failed: %s\n", sys_get_error() );
//		return false;
//	}

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
	//SDL_Event event = { g_wakeup_on_mpv_events };
	//SDL_PushEvent( &event );
}


static void on_mpv_render_update( void* ctx )
{
	g_wakeup_on_mpv_render_update = true;
	//SDL_Event event = { g_wakeup_on_mpv_render_update };
	//SDL_PushEvent( &event );
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

	int                    enabled   = 1;

	mpv_opengl_init_params params_gl = {
		get_proc_address_mpv,
		nullptr,
	};

	mpv_render_param params[] = {
		// { MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL },
		{ MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_SW },
		// { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &params_gl },
		// Tell libmpv that you will call mpv_render_context_update() on render
		// context update callbacks, and that you will _not_ block on the core
		// ever (see <libmpv/render.h> "Threading" section for what libmpv
		// functions you can call at all when this is active).
		// In particular, this means you must call e.g. mpv_command_async()
		// instead of mpv_command().
		// If you want to use synchronous calls, either make them on a separate
		// thread, or remove the option below (this will disable features like
		// DR and is not recommended anyway).
		{ MPV_RENDER_PARAM_ADVANCED_CONTROL, &enabled },
		{}
	};

	// This makes mpv use the currently set GL context. It will use the callback
	// (passed via params) to resolve GL builtin functions, as well as extensions.
	// mpv_render_context* mpv_gl;
	// if ( p_mpv_render_context_create( &g_mpv_gl, g_mpv, params ) < 0 )
	// {
	// 	printf( "mpv_render_context_create failed!\n" );
	// 	return false;
	// }

	// We use events for thread-safe notification of the SDL main loop.
	// Generally, the wakeup callbacks (set further below) should do as least
	// work as possible, and merely wake up another thread to do actual work.
	// On SDL, waking up the mainloop is the ideal course of action. SDL's
	// SDL_PushEvent() is thread-safe, so we use that.
	//g_wakeup_on_mpv_render_update = SDL_RegisterEvents( 1 );
	//g_wakeup_on_mpv_events        = SDL_RegisterEvents( 1 );
	//
	//if ( g_wakeup_on_mpv_render_update == (Uint32)-1 || g_wakeup_on_mpv_events == (Uint32)-1 )
	//{
	//	printf( "SDL_RegisterEvents failed: %s\n", SDL_GetError() );
	//	return false;
	//}

	// When normal mpv events are available.
	p_mpv_set_wakeup_callback( g_mpv, on_mpv_events, NULL );

	// When there is a need to call mpv_render_context_update(), which can
	// request a new frame to be rendered.
	// (Separate from the normal event handling mechanism for the sake of
	//  users which run OpenGL on a different thread.)
	// p_mpv_render_context_set_update_callback( g_mpv_gl, on_mpv_render_update, NULL );

	int64_t wid = (s64)g_mpv_window;

	// attach to main window
	p_mpv_set_property( g_mpv, "wid", MPV_FORMAT_INT64, &wid );  // INT64
	// p_mpv_set_option( g_mpv, "wid", MPV_FORMAT_FLAG, g_main_window );

	return true;
}


void stop_mpv()
{
}


