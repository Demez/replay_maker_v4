#include "main.h"

// some reference here
// https://github.com/mpv-player/mpv-examples/blob/master/libmpv/sdl/main.c

void*                         g_mpv_module = nullptr;

static ChVector< mpv_data_t > g_mpv;
static u32                    g_mpv_index = 0;

mpv_data_t                    g_mpv_extra_vid{};
bool                          g_mpv_extra_vid_on = false;

GLuint                        g_fbo              = 0;
GLuint                        g_fbo_tex          = 0;

std::vector< std::string >    g_mpv_exts;

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


#define LOAD_FUNC( func )                                      \
	p_##func = (func##_t)sys_load_func( g_mpv_module, #func ); \
	if ( p_##func == nullptr )                                 \
	{                                                          \
		char* sys_error = sys_get_error();                     \
		printf( "sys_load_func failed: %s\n", sys_error );     \
		free( sys_error );                                     \
		return false;                                          \
	}


bool load_mpv_dll()
{
	#if _WIN32
	g_mpv_module = sys_load_library( L"libmpv-2.dll" );
	#else
	g_mpv_module = sys_load_library( "libmpv.so" );
	#endif

	if ( g_mpv_module == nullptr )
	{
		char* sys_error = sys_get_error();
		printf( "Failed to load MPV: %s\n", sys_error );
		free( sys_error );
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
	stop_mpv();

	// clear mpv function pointers
	p_mpv_create             = nullptr;
	p_mpv_client_api_version = nullptr;

	sys_close_library( g_mpv_module );
}


void mpv_draw_frame()
{
	// mpv_handle*         mpv = get_mpv();
	// mpv_render_context* gl  = get_mpv_gl();

	u64 _time = sys_get_time_ms();

	mpv_data_t* mpv_data = get_mpv_data();

	if ( !mpv_data )
		return;

	mpv_handle*         mpv = mpv_data->mpv;
	mpv_render_context* gl  = mpv_data->gl;

	s64    video_width = 0, video_height = 0;

	video_width  = mpv_data->dwidth;
	video_height = mpv_data->dheight;

	// p_mpv_get_property( mpv, "dwidth", MPV_FORMAT_INT64, &video_width );
	// p_mpv_get_property( mpv, "dheight", MPV_FORMAT_INT64, &video_height );

	s64   window_scale;
	//	p_mpv_get_property( g_mpv, "current-window-scale", MPV_FORMAT_INT64, &window_scale );

	// Fit image in window size
	float factor[ 2 ] = { 1.f, 1.f };

	factor[ 0 ]       = (float)app::mpv_size[ 0 ] / (float)video_width;
	factor[ 1 ]       = (float)app::mpv_size[ 1 ] / (float)video_height;

	float zoom_level = std::min( factor[ 0 ], factor[ 1 ] );

	int   new_width  = video_width * zoom_level;
	int   new_height = video_height * zoom_level;

	int   pos_x       = app::mpv_size[ 0 ] / 2 - ( new_width / 2 );
	int   pos_y       = app::mpv_size[ 1 ] / 2 - ( new_height / 2 );

	int   offset_x    = app::mpv_size[ 0 ] - new_width;
	int   offset_y    = app::mpv_size[ 1 ] - new_height;

	glBindFramebuffer( GL_FRAMEBUFFER, g_fbo );
	////glBindRenderbuffer( GL_RENDERBUFFER, g_mpv_rbo );
	//
	glViewport( 0, 0, app::mpv_size[ 0 ], app::mpv_size[ 1 ] );
	// glClearColor( 0.15, 0.15, 0.15, 1.0 );
	// glClear( GL_COLOR_BUFFER_BIT );

	mpv_opengl_fbo   fbo{ g_fbo, app::mpv_size[ 0 ], app::mpv_size[ 1 ], GL_RGB };
	// mpv_opengl_fbo   fbo{ g_mpv_fbo, app::window_size[ 0 ], app::window_size[ 1 ], GL_RGB };
	int              yes  = 1;

	mpv_render_param rp[] = {
		{ MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
		{ MPV_RENDER_PARAM_FLIP_Y, &yes },
		{ MPV_RENDER_PARAM_INVALID, NULL },
	};

	int err = p_mpv_render_context_render( gl, rp );

	// glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_mpv_rbo );

	//glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	//glBindFramebuffer( GL_READ_BUFFER, g_mpv_fbo );
	//
	//glBlitFramebuffer( 0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	//glBindRenderbuffer( GL_RENDERBUFFER, 0 );

	// glViewport( 0, app::window_size[ 1 ] - app::mpv_size[ 1 ], app::mpv_size[ 0 ], app::mpv_size[ 1 ] );
	glViewport( app::window_size[ 0 ] - app::mpv_size[ 0 ], app::window_size[ 1 ] - app::mpv_size[ 1 ], app::mpv_size[ 0 ], app::mpv_size[ 1 ] );
	// glViewport( 0, 0, app::window_size[ 0 ], app::window_size[ 1 ] );

	//glEnable( GL_SCISSOR_TEST );
	//glScissor( pos_x, pos_y, new_width, new_height );

	glClearColor( 0.15, 0.15, 0.15, 1.0 );
	glClear( GL_COLOR_BUFFER_BIT );

	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, g_fbo_tex );

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

	//u64 _end_time = sys_get_time_ms();
	//
	//if ( _end_time > _time )
	//	printf( "MPV DRAW TIME - %u\n", _end_time - _time );
}


void mpv_update_texture()
{
	if ( g_mpv.empty() )
		return;

	if ( !g_fbo_tex )
		return;

	glBindTexture( GL_TEXTURE_2D, g_fbo_tex );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, app::mpv_size[ 0 ], app::mpv_size[ 1 ], 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr );
	// glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, app::window_size[ 0 ], app::window_size[ 1 ], 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_fbo_tex, 0 );

	if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		printf( "FBO incomplete!\n" );

	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}


void mpv_create_texture()
{
	if ( g_fbo_tex )
		return;

	glGenFramebuffers( 1, &g_fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, g_fbo );

	//glGenRenderbuffers( 1, &g_mpv_rbo );
	glGenTextures( 1, &g_fbo_tex );

	mpv_update_texture();
}


static void* mpv_get_proc( void* ctx, const char* name )
{
	return ( void* )( SDL_GL_GetProcAddress( name ) );
}


bool start_mpv( mpv_data_t& mpv )
{
	// get the mpv version
	unsigned long mpv_version = p_mpv_client_api_version();
	printf( "mpv version: %lu\n", mpv_version );

	mpv.mpv = p_mpv_create();

	if ( mpv.mpv == nullptr )
	{
		printf( "mpv_create failed!\n" );
		return false;
	}

	// Disable VO
	p_mpv_set_option_string( mpv.mpv, "vo", "libmpv" );

	// Stops the main thread from being blocked somehow
	// https://github.com/celluloid-player/celluloid/pull/982
	p_mpv_set_option_string( mpv.mpv, "video-timing-offset", "0" );

	// Start Paused
	p_mpv_set_option_string( mpv.mpv, "pause", "" );

	if ( p_mpv_initialize( mpv.mpv ) < 0 )
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

	p_mpv_render_context_create( &mpv.gl, mpv.mpv, params );

	// When there is a need to call mpv_render_context_update(), which can
	// request a new frame to be rendered.
	// (Separate from the normal event handling mechanism for the sake of
	//  users which run OpenGL on a different thread.)
	// p_mpv_render_context_set_update_callback( g_mpv_gl, on_mpv_render_update, nullptr );

	// When normal mpv events are available.
	// p_mpv_set_wakeup_callback( mpv.mpv, on_mpv_events, NULL );

	// Create Framebuffer to draw on

	mpv_create_texture();

	p_mpv_set_property_string( mpv.mpv, "keep-open", "always" );

	int observe_ret = p_mpv_observe_property( mpv.mpv, e_mpv_cmd_observe, "time-pos", MPV_FORMAT_DOUBLE );
	observe_ret     = p_mpv_observe_property( mpv.mpv, e_mpv_cmd_observe, "duration", MPV_FORMAT_DOUBLE );
	observe_ret     = p_mpv_observe_property( mpv.mpv, e_mpv_cmd_observe, "pause", MPV_FORMAT_FLAG );

	observe_ret     = p_mpv_observe_property( mpv.mpv, e_mpv_cmd_observe, "dwidth", MPV_FORMAT_INT64 );
	observe_ret     = p_mpv_observe_property( mpv.mpv, e_mpv_cmd_observe, "dheight", MPV_FORMAT_INT64 );

	observe_ret     = p_mpv_observe_property( mpv.mpv, e_mpv_cmd_observe, "volume", MPV_FORMAT_DOUBLE );

	observe_ret     = p_mpv_observe_property( mpv.mpv, e_mpv_cmd_observe, "audio", MPV_FORMAT_STRING );
	observe_ret     = p_mpv_observe_property( mpv.mpv, e_mpv_cmd_observe, "current-tracks/audio/title", MPV_FORMAT_STRING );

	if ( g_mpv_exts.empty() )
	{
		// Load supported extensions
		char* exts = p_mpv_get_property_string( mpv.mpv, "video-exts" );

		if ( !exts )
		{
			printf( "no supported extensions from mpv??\n" );
			return false;
		}

		char* ext_cur  = exts;
		char* ext_next = strchr( ext_cur, ',' );

		while ( ext_next != nullptr )
		{
			std::string ext = ".";
			ext.append( ext_cur, ext_next - ext_cur );
			g_mpv_exts.push_back( ext );

			ext_cur  = ext_next + 1;
			ext_next = strchr( ext_cur, ',' );
		}
	}

	return true;
}


void mpv_update( mpv_data_t& data );


void close_mpv( mpv_data_t& mpv )
{
	free( mpv.current_video );
	mpv.current_video = nullptr;

	if ( mpv.mpv )
	{
		mpv_update( mpv );

		//p_mpv_unobserve_property( mpv.mpv, e_mpv_cmd_observe );

		//p_mpv_free( mpv.audio_track );
		//p_mpv_free( mpv.audio_track_title );
	}

	// crash ??? what am i missing
	// p_mpv_destroy( mpv.mpv );
}


void stop_mpv()
{
	for ( mpv_data_t& mpv : g_mpv )
	{
		close_mpv( mpv );
	}

	g_mpv.clear();

	if ( g_mpv_extra_vid_on )
		close_mpv( g_mpv_extra_vid );

	g_mpv_extra_vid_on = false;
}


// ----------------------------------------------------


mpv_data_t* get_mpv_data( u32 index )
{
	if ( index == UINT32_MAX )
		index = g_mpv_index;

	if ( g_mpv_extra_vid_on && index == EXTRA_VID_ID )
		return &g_mpv_extra_vid;

	if ( g_mpv.empty() )
		return nullptr;

	if ( index >= g_mpv.size() )
		return nullptr;

	return &g_mpv[ index ];
}


// this returns the currently used mpv handle
// will be used for videos with multiple sources for faster switching
mpv_handle* get_mpv()
{
	mpv_data_t* mpv = get_mpv_data( g_mpv_index );

	if ( !mpv )
		return nullptr;

	return mpv->mpv;
}


mpv_render_context* get_mpv_gl()
{
	mpv_data_t* mpv = get_mpv_data( g_mpv_index );

	if ( !mpv )
		return nullptr;

	return mpv->gl;
}


u32 get_mpv_count()
{
	return g_mpv.size();
}


u32 get_mpv_count_plus()
{
	if ( g_mpv_extra_vid_on )
		return g_mpv.size() + 1;

	return g_mpv.size();
}


void set_mpv_index( u32 index )
{
	if ( index != EXTRA_VID_ID && index >= g_mpv.size() )
		return;

	g_mpv_index = index;

	for ( u32 i = 0; i < g_mpv.size(); i++ )
	{
		mpv_data_t* mpv = get_mpv_data( i );

		if ( mpv && mpv->mpv )
		{
			// if the video was playing, pause the other mpv clients and play the one we swapped to
			if ( i != g_mpv_index )
			{
				const char* cmd[]   = { "set", "pause", "yes", NULL };
				int         cmd_ret = p_mpv_command_async( mpv->mpv, 0, cmd );
			}
		}
	}

	// mpv has an extra video if it's loose
	if ( g_mpv_extra_vid_on && g_mpv_index != EXTRA_VID_ID )
	{
		mpv_data_t* mpv = get_mpv_data( EXTRA_VID_ID );

		if ( mpv )
		{
			const char* cmd[]   = { "set", "pause", "yes", NULL };
			int         cmd_ret = p_mpv_command_async( mpv->mpv, 0, cmd );
		}
	}
}


u32 get_mpv_index()
{
	return g_mpv_index;
}


void set_mpv_count( u32 count )
{
	if ( count == g_mpv.size() )
		return;

	u32 size = g_mpv.size();

	if ( count > size )
	{
		g_mpv.resize( count );

		for ( u32 i = size; i < count; i++ )
		{
			start_mpv( g_mpv[ i ] );
		}
	}
	else
	{
		for ( u32 i = size; i != count; i-- )
		{
			p_mpv_free( g_mpv[ i - 1 ].mpv );
		}

		g_mpv.resize( count );
	}
}


char* mpv_get_current_video()
{
	mpv_data_t* mpv = get_mpv_data( g_mpv_index );

	if ( !mpv )
		return nullptr;

	return mpv->current_video;
}


void set_mpv_extra_video()
{
	if ( !g_mpv_extra_vid_on )
		start_mpv( g_mpv_extra_vid );

	g_mpv_extra_vid_on = true;
}


void remove_mpv_extra_video()
{
	if ( g_mpv_extra_vid_on )
	{
		close_mpv( g_mpv_extra_vid );
		memset( &g_mpv_extra_vid, 0, sizeof( mpv_data_t ) );
	}

	g_mpv_extra_vid_on = false;
}


// ----------------------------------------------------


template< typename T >
T get_mpv_value( void* data, T fallback )
{
	if ( data == nullptr )
		return fallback;
	else
		return *(T*)data;
}


char* get_mpv_string( char*& old, mpv_event_property* property, char* fallback )
{
	if ( old )
		free( old );

	old = nullptr;

	if ( property->format != MPV_FORMAT_STRING )
		return fallback;
	
	if ( property->data == nullptr )
		return fallback;

	// do you not free async string returns? can't find info on it in documentation
	// this crashes the program if i do this, either on first call, or a little later
	// p_mpv_free( temp_str );

	return util_strdup( *(char**)property->data );
}


void get_media_info( mpv_data_t& mpv )
{
	if ( !mpv.current_video )
		return;

	mpv.track_count       = 0;
	mpv.track_count_audio = 0;
	mpv.track_count_video = 0;

	mpv_error ret          = (mpv_error)p_mpv_get_property( mpv.mpv, "track-list/count", MPV_FORMAT_INT64, &mpv.track_count );

	for ( s32 i = 0; i < mpv.track_count; i++ )
	{
		char cmd[ 64 ] = { 0 };
		snprintf( cmd, 64, "track-list/%d/type", i );

		char* type = nullptr;
		ret        = (mpv_error)p_mpv_get_property( mpv.mpv, cmd, MPV_FORMAT_STRING, &type );

		if ( !type )
			continue;

		if ( strcmp( type, "video" ) == 0 )
			mpv.track_count_video++;

		else if ( strcmp( type, "audio" ) == 0 )
			mpv.track_count_audio++;
	}
}


void mpv_update( mpv_data_t& data )
{
	if ( !data.mpv )
		return;

	mpv_event* mpv_event = p_mpv_wait_event( data.mpv, 0 );

	while ( mpv_event && mpv_event->event_id != MPV_EVENT_NONE )
	{
		if ( mpv_event->event_id == MPV_EVENT_NONE )
			return;

		if ( mpv_event->event_id == MPV_EVENT_PROPERTY_CHANGE )
		{
			struct mpv_event_property* property = (struct mpv_event_property*)mpv_event->data;

			if ( property->name )
			{
				if ( strcmp( "time-pos", property->name ) == 0 )
				{
					// what the fuck is this?
					// if ( property->data == nullptr )
					// 	data.time_pos = 0.0;
					// else
					// 	data.time_pos = *(double*)property->data;
					data.time_pos = get_mpv_value( property->data, 0.0 );
				}
				else if ( strcmp( "duration", property->name ) == 0 )
				{
					data.duration = get_mpv_value( property->data, 0.0 );
				}
				else if ( strcmp( "pause", property->name ) == 0 )
				{
					data.pause = get_mpv_value< s32 >( property->data, 0 );
				}
				else if ( strcmp( "dwidth", property->name ) == 0 )
				{
					data.dwidth = get_mpv_value< s64 >( property->data, 0 );
				}
				else if ( strcmp( "dheight", property->name ) == 0 )
				{
					data.dheight = get_mpv_value< s64 >( property->data, 0 );
				}
				else if ( strcmp( "volume", property->name ) == 0 )
				{
					data.volume = get_mpv_value< double >( property->data, 0 );
				}
				else if ( strcmp( "audio", property->name ) == 0 )
				{
					data.audio_track = get_mpv_string( data.audio_track, property, nullptr );
				}
				else if ( strcmp( "current-tracks/audio/title", property->name ) == 0 )
				{
					data.audio_track_title = get_mpv_string( data.audio_track_title, property, nullptr );
				}
			}
		}
		else if ( mpv_event->event_id == MPV_EVENT_GET_PROPERTY_REPLY )
		{
			struct mpv_event_property* property = (struct mpv_event_property*)mpv_event->data;

			if ( property->name )
			{
				//if ( strcmp( "audio", property->name ) == 0 )
				//{
				//	data.audio_track = get_mpv_value< char* >( property->data, nullptr );
				//}
				if ( strcmp( "current-tracks/audio/title", property->name ) == 0 )
				{
					//if ( data.audio_track_title )
					//	p_mpv_free( data.audio_track_title );

					//data.audio_track_title = get_mpv_value< char* >( property->data, nullptr );
				}
			}
		}
		else if ( mpv_event->event_id == MPV_EVENT_PLAYBACK_RESTART )
		{
			data.loaded_file = true;
			get_media_info( data );
		}
		else if ( mpv_event->event_id == MPV_EVENT_COMMAND_REPLY )
		{
			struct mpv_event_command* cmd_reply = (struct mpv_event_command*)mpv_event->data;

			if ( mpv_event->reply_userdata == e_mpv_cmd_loadfile )
			{
				if ( mpv_event->error != 0 )
				{
					free( data.current_video );
					data.current_video = nullptr;
					data.loaded_file   = false;
				}
			}
			else if ( mpv_event->reply_userdata == e_mpv_cmd_seek )
			{
				//printf( "FINISH SEEK - %u\n", sys_get_time_ms(), data.seek_queued_time );
				// if ( data.seek_queued )
				data.seek_queued = false;
			}
		}

		mpv_event = p_mpv_wait_event( data.mpv, 0 );
	}
}


void mpv_update()
{
	for ( u32 i = 0; i < get_mpv_count(); i++ )
		mpv_update( g_mpv[ i ] );

	if ( g_mpv_extra_vid_on )
		mpv_update( g_mpv_extra_vid );
}


void mpv_window_resize()
{
	mpv_update_texture();
}


// ----------------------------------------------------


void mpv_cmd_set_video_zoom( float zoom )
{
	// convert float to string
	char zoom_str[ 16 ];
	gcvt( zoom, 4, zoom_str );

	const char* cmd[]   = { "set", "video-zoom", zoom_str, nullptr };
	int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
}


void mpv_cmd_add_video_zoom( float zoom )
{
	// convert float to string
	char zoom_str[ 16 ];
	gcvt( zoom, 4, zoom_str );

	const char* cmd[]   = { "set", "video-zoom", zoom_str, nullptr };
	int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
}


void mpv_cmd_loadfile( const char* file, u32 index )
{
	if ( !file )
		return;

	mpv_data_t* mpv = get_mpv_data( index );

	if ( !mpv || !mpv->mpv )
		return;

	if ( mpv->current_video && strcmp( file, mpv->current_video ) == 0 )
		return;

	if ( !fs_is_file( file ) )
		return;

	printf( "Loading Video: %s\n", file );

	const char* cmd[]   = { "loadfile", file, NULL };
	int         cmd_ret = p_mpv_command_async( mpv->mpv, e_mpv_cmd_loadfile, cmd );

	free( mpv->current_video );
	mpv->current_video = util_strdup( file );
}


void mpv_cmd_close_video( u32 index )
{
	mpv_data_t* mpv = get_mpv_data( index );

	if ( !mpv || !mpv->mpv )
		return;

	const char* cmd[]   = { "stop", NULL };
	int         cmd_ret = p_mpv_command_async( mpv->mpv, NULL, cmd );

	free( mpv->current_video );
	mpv->current_video = nullptr;
}


void mpv_cmd_toggle_playback()
{
	const char* cmd[]   = { "cycle", "pause", NULL };
	int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
}


bool mpv_cmd_seek( mpv_data_t* mpv, double seconds )
{
	if ( !mpv )
		return false;

	// wait for current seek to finish
	if ( mpv->seek_queued )
		return false;

	mpv->seek_queued      = true;
	mpv->seek_queued_time = sys_get_time_ms();

	char time_pos_str[ 16 ];
	gcvt( seconds, 4, time_pos_str );

	const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
	int         cmd_ret   = p_mpv_command_async( mpv->mpv, e_mpv_cmd_seek, cmd );

	return true;
}


bool mpv_cmd_seek( double seconds )
{
	return mpv_cmd_seek( get_mpv_data(), seconds );
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

