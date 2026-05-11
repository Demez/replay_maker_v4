#include "main.h"

// some reference here
// https://github.com/mpv-player/mpv-examples/blob/master/libmpv/sdl/main.c

void*               g_mpv_module  = nullptr;
mpv_handle*         g_mpv         = nullptr;
mpv_render_context* g_mpv_gl      = nullptr;
GLuint              g_mpv_fbo     = 0;
GLuint              g_mpv_fbo_tex = 0;
GLuint              g_mpv_rbo     = 0;

bool                g_wakeup_on_mpv_render_update, g_wakeup_on_mpv_events;

static char*        g_current_video = nullptr;

s64                 g_video_width = 0, g_video_height = 0;

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


void mpv_draw_frame()
{
	p_mpv_get_property( g_mpv, "dwidth", MPV_FORMAT_INT64, &g_video_width );
	p_mpv_get_property( g_mpv, "dheight", MPV_FORMAT_INT64, &g_video_height );

	s64   window_scale;
	//	p_mpv_get_property( g_mpv, "current-window-scale", MPV_FORMAT_INT64, &window_scale );

	// Fit image in window size
	float factor[ 2 ] = { 1.f, 1.f };

	factor[ 0 ]       = (float)g_mpv_size[ 0 ] / (float)g_video_width;
	factor[ 1 ]       = (float)g_mpv_size[ 1 ] / (float)g_video_height;

	float zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

	int   new_width  = g_video_width * zoom_level;
	int   new_height = g_video_height * zoom_level;

	int   pos_x       = g_mpv_size[ 0 ] / 2 - ( new_width / 2 );
	int   pos_y       = g_mpv_size[ 1 ] / 2 - ( new_height / 2 );

	int   offset_x    = g_mpv_size[ 0 ] - new_width;
	int   offset_y    = g_mpv_size[ 1 ] - new_height;

	glBindFramebuffer( GL_FRAMEBUFFER, g_mpv_fbo );
	////glBindRenderbuffer( GL_RENDERBUFFER, g_mpv_rbo );
	//
	glViewport( 0, 0, g_mpv_size[ 0 ], g_mpv_size[ 1 ] );
	// glClearColor( 0.15, 0.15, 0.15, 1.0 );
	// glClear( GL_COLOR_BUFFER_BIT );

	mpv_opengl_fbo   fbo{ g_mpv_fbo, g_mpv_size[ 0 ], g_mpv_size[ 1 ], GL_RGB };
	// mpv_opengl_fbo   fbo{ g_mpv_fbo, g_window_size[ 0 ], g_window_size[ 1 ], GL_RGB };
	int              yes  = 1;

	mpv_render_param rp[] = {
		{ MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
		{ MPV_RENDER_PARAM_FLIP_Y, &yes },
		{ MPV_RENDER_PARAM_INVALID, NULL },
	};

	p_mpv_render_context_render( g_mpv_gl, rp );

	// glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_mpv_rbo );

	//glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	//glBindFramebuffer( GL_READ_BUFFER, g_mpv_fbo );
	//
	//glBlitFramebuffer( 0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	//glBindRenderbuffer( GL_RENDERBUFFER, 0 );

	glViewport( 0, g_window_size[ 1 ] - g_mpv_size[ 1 ], g_mpv_size[ 0 ], g_mpv_size[ 1 ] );
	// glViewport( 0, 0, g_window_size[ 0 ], g_window_size[ 1 ] );

	//glEnable( GL_SCISSOR_TEST );
	//glScissor( pos_x, pos_y, new_width, new_height );

	glClearColor( 0.15, 0.15, 0.15, 1.0 );
	glClear( GL_COLOR_BUFFER_BIT );

	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, g_mpv_fbo_tex );

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0, 1, 0, 1, -1, 1 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	glBegin( GL_QUADS );

	glTexCoord2f( 0, 0 );
	glVertex2f( 0, 0 );
	glTexCoord2f( 1, 0 );
	glVertex2f( 1, 0 );
	glTexCoord2f( 1, 1 );
	glVertex2f( 1, 1 );
	glTexCoord2f( 0, 1 );
	glVertex2f( 0, 1 );

	glEnd();

	//glDisable( GL_SCISSOR_TEST );
	glDisable( GL_TEXTURE_2D );
}


void mpv_update_texture()
{
	if ( !g_mpv_fbo_tex )
		return;

	glBindTexture( GL_TEXTURE_2D, g_mpv_fbo_tex );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, g_mpv_size[ 0 ], g_mpv_size[ 1 ], 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr );
	// glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, g_window_size[ 0 ], g_window_size[ 1 ], 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_mpv_fbo_tex, 0 );

	if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		printf( "FBO incomplete!\n" );

	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}


void mpv_create_texture()
{
	//glGenRenderbuffers( 1, &g_mpv_rbo );
	glGenTextures( 1, &g_mpv_fbo_tex );

	mpv_update_texture();
}


static void on_mpv_render_update( void* ctx )
{
	g_wakeup_on_mpv_render_update = true;
}


static void* mpv_get_proc( void* ctx, const char* name )
{
	return SDL_GL_GetProcAddress( name );
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

	// Disable VO
	p_mpv_set_option_string( g_mpv, "vo", "libmpv" );

	// Stops the main thread from being blocked somehow
	// https://github.com/celluloid-player/celluloid/pull/982
	p_mpv_set_option_string( g_mpv, "video-timing-offset", "0" );

	if ( p_mpv_initialize( g_mpv ) < 0 )
	{
		printf( "mpv_initialize failed!\n" );
		return false;
	}

	// create render context
	mpv_opengl_init_params gl_init = {
		.get_proc_address = mpv_get_proc,  // e.g. SDL_GL_GetProcAddress
	};

	mpv_render_param params[] = {
		{ MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL },
		{ MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init },
		{ MPV_RENDER_PARAM_INVALID, NULL },
	};

	p_mpv_render_context_create( &g_mpv_gl, g_mpv, params );

	// When there is a need to call mpv_render_context_update(), which can
	// request a new frame to be rendered.
	// (Separate from the normal event handling mechanism for the sake of
	//  users which run OpenGL on a different thread.)
	p_mpv_render_context_set_update_callback( g_mpv_gl, on_mpv_render_update, nullptr );

	// When normal mpv events are available.
	p_mpv_set_wakeup_callback( g_mpv, on_mpv_events, NULL );

	// Create Framebuffer to draw on
	glGenFramebuffers( 1, &g_mpv_fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, g_mpv_fbo );

	mpv_create_texture();

	p_mpv_set_property_string( g_mpv, "keep-open", "always" );

	return true;
}


void stop_mpv()
{
}


void mpv_window_resize()
{
	mpv_update_texture();
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
	if ( !g_mpv )
		return;

	printf( "loading file: %s\n", file );

	const char* cmd[]   = { "loadfile", file, NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, NULL, cmd );

	//g_mpv_video_ready   = false;

	// mpv_event*  event   = p_mpv_wait_event( g_mpv, 0.1f );
	// mpv_handle_wait_event( g_mpv, 0.1f );

	free( g_current_video );
	g_current_video  = nullptr;

	mpv_event* event = p_mpv_wait_event( g_mpv, -1 );

	while ( event->event_id != MPV_EVENT_NONE )
	{
		if ( event->event_id == MPV_EVENT_LOG_MESSAGE )
		{
			struct mpv_event_log_message* msg = (struct mpv_event_log_message*)event->data;
			printf( "[%s] %s: %s", msg->prefix, msg->level, msg->text );
		}
		else if ( event->event_id == MPV_EVENT_COMMAND_REPLY )
		{
			if ( event->error != 0 )
			{
				printf( "failed to load video - %d\n", event->error );
				return;
			}

			// Video Loaded
			//break;
		}
		else if ( event->event_id == MPV_EVENT_PLAYBACK_RESTART )
		{
			// Video Loaded
			break;
		}

		event = p_mpv_wait_event( g_mpv, -1 );
	}

	// Video Loaded
	g_current_video = util_strdup( file );

	get_media_info();

	// or use video-params?
	//	p_mpv_get_property( g_mpv, "width", MPV_FORMAT_INT64, &g_video_width );
	//	p_mpv_get_property( g_mpv, "height", MPV_FORMAT_INT64, &g_video_height );
	//
	//	p_mpv_get_property( g_mpv, "height", MPV_FORMAT_INT64, &g_video_height );
}


void mpv_cmd_close_video()
{
	if ( !g_mpv )
		return;

	const char* cmd[]   = { "stop", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, NULL, cmd );

	free( g_current_video );
	g_current_video   = nullptr;

	// g_mpv_video_ready = false;
}


void mpv_cmd_toggle_playback()
{
	const char* cmd[]   = { "cycle", "pause", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void mpv_cmd_seek_offset( double seconds )
{
	double duration = 0;
	double time_pos = 0;
	p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );
	p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

	double new_time = time_pos + seconds;
	new_time        = std::max( 0.0, std::min( duration, new_time ) );

	char time_pos_str[ 32 ];
	gcvt( new_time, 4, time_pos_str );

	const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void mpv_cmd_hook_window( void* window )
{
}


void mpv_cmd_hook_window_mpv()
{
}


void mpv_handle_error( int )
{
	//p_mpv_error_string()
}

