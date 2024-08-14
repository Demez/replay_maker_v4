#pragma once

#include <cstdio>
#include <cstring>
#include <stdlib.h>

#include "mpv/client.h"
#include "mpv/render.h"
#include "mpv/render_gl.h"


// ----------------------------------------------------


using u8  = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

using s8  = char;
using s16 = short;
using s32 = int;
using s64 = long long;

using f32 = float;
using f64 = double;

using module = void*;
using window_id = void*;

struct ImGuiContext;

// ----------------------------------------------------


// struct ivec2
// {
// 	int x, y;
// };


using ivec2 = int[ 2 ];
using vec2 = float[ 2 ];


// struct vec2
// {
// 	float x, y;
// };


// ----------------------------------------------------

#define ARR_SIZE( arr ) ( sizeof( arr ) / sizeof( arr[ 0 ] ) )
#define MIN( a, b )     ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define MAX( a, b )     ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

#define SET_INT2( var, x, y ) \
	( var )[ 0 ] = x; \
	( var )[ 1 ] = y;


template< typename T >
T CLAMP( T value, T low, T high )
{
	return ( value < low ) ? low : ( ( value > high ) ? high : value );
}


template< typename T >
T* ch_malloc( size_t count )
{
	T* data = (T*)malloc( count * sizeof( T ) );
	
	if ( data == nullptr )
	{
		printf( "malloc failed\n" );
		return nullptr;
	}

	memset( data, 0, count * sizeof( T ) );
	return data;

	// return (T*)malloc( count * sizeof( T ) );
}


template< typename T >
T* ch_calloc( size_t count )
{
	return (T*)calloc( count, sizeof( T ) );
}


template< typename T >
T* ch_realloc( T* data, size_t count )
{
	return (T*)realloc( data, count * sizeof( T ) );
}


// ----------------------------------------------------

// library loading
module                             sys_load_library( const wchar_t* path );
void                               sys_close_library( module mod );
void*                              sys_load_func( module mod, const char* path );
const wchar_t*                     sys_get_error();

// also known as "sys_to_utf16"
wchar_t*                           sys_to_wchar( const char* spStr, int sSize );
wchar_t*                           sys_to_wchar( const char* spStr );

char*                              sys_to_utf8( const wchar_t* spStr, int sSize );
char*                              sys_to_utf8( const wchar_t* spStr );

bool                               win32_create_windows( int width, int height, int imgui_window_count );
void                               win32_run();

void                               run_logic();
void                               draw_imgui_window( int index, int size[ 2 ] );

constexpr int                      IMGUI_WINDOW_COUNT = 2;

extern bool                        g_running;

extern void*                       g_main_window;
extern void*                       g_mpv_window;
extern void**                      g_imgui_window;
extern void**                      g_imgui_window_borders;
//extern window_border_data_t*       g_imgui_window_border_data;
extern int                         g_imgui_window_count;
extern ImGuiContext**              g_imgui_contexts;

extern ivec2                       g_mpv_size;
// extern int                         g_mpv_width, g_mpv_height;
// extern ivec2                       g_imgui_window_size[ IMGUI_WINDOW_COUNT ];
// extern int                         g_imgui_window_size[ 2 ][ 2 ];
extern ivec2                       g_window_size;

constexpr int                      BORDER_SIZE = 6;

// ----------------------------------------------------
// MPV

bool                               load_mpv_dll();
void                               unload_mpv_dll();

void*                              get_proc_address_mpv( void* fn_ctx, const char* name );

bool                               start_mpv();
void                               stop_mpv();

void                               calc_imgui_window_size( int index, ivec2& size );
void                               calc_playback_window_size( ivec2& size );
void                               calc_replay_window_size( ivec2& size );

void                               mpv_cmd_loadfile( const wchar_t* file );
void                               mpv_cmd_loadfile( const char* file );
void                               mpv_cmd_toggle_playback();

extern mpv_handle*                 g_mpv;
extern mpv_render_context*         g_mpv_gl;
extern bool                        g_wakeup_on_mpv_render_update, g_wakeup_on_mpv_events;

// ----------------------------------------------------


// mpv function pointer typedefs, cause im not compiling the mpv source just to link to it

#define MPV_PTR( return_type, func, ... ) \
	typedef return_type ( *func##_t )( __VA_ARGS__ ); \
	extern func##_t p_##func

// client.h
MPV_PTR( unsigned long, mpv_client_api_version, void );
MPV_PTR( const char*,   mpv_error_string, int error );
MPV_PTR( void,          mpv_free, void* data );
MPV_PTR( const char*,   mpv_client_name, mpv_handle* ctx );
MPV_PTR( int64_t,       mpv_client_id, mpv_handle* ctx );
MPV_PTR( mpv_handle*,   mpv_create, void );
MPV_PTR( int,           mpv_initialize, mpv_handle* ctx );
MPV_PTR( void,          mpv_destroy, mpv_handle* ctx );
MPV_PTR( void,          mpv_terminate_destroy, mpv_handle* ctx );
MPV_PTR( mpv_handle*,   mpv_create_client, mpv_handle* ctx, const char* name );
MPV_PTR( mpv_handle*,   mpv_create_weak_client, mpv_handle* ctx, const char* name );
MPV_PTR( int,           mpv_load_config_file, mpv_handle* ctx, const char* filename );
MPV_PTR( int64_t,       mpv_get_time_ns, mpv_handle* ctx );
MPV_PTR( int64_t,       mpv_get_time_us, mpv_handle* ctx );
MPV_PTR( void,          mpv_free_node_contents, mpv_node* node );
MPV_PTR( int,           mpv_set_option, mpv_handle* ctx, const char* name, mpv_format format, void* data );
MPV_PTR( int,           mpv_set_option_string, mpv_handle* ctx, const char* name, const char* data );
MPV_PTR( int,           mpv_command, mpv_handle *ctx, const char **args );
MPV_PTR( int,           mpv_command_node, mpv_handle *ctx, mpv_node *args, mpv_node *result );
MPV_PTR( int,           mpv_command_ret, mpv_handle *ctx, const char **args, mpv_node *result );
MPV_PTR( int,           mpv_command_string, mpv_handle *ctx, const char *args );
MPV_PTR( int,           mpv_command_async, mpv_handle *ctx, uint64_t reply_userdata, const char **args );
MPV_PTR( int,           mpv_command_node_async, mpv_handle *ctx, uint64_t reply_userdata, mpv_node *args );
MPV_PTR( int,           mpv_abort_async_command, mpv_handle *ctx, uint64_t reply_userdata );
MPV_PTR( int,           mpv_set_property, mpv_handle *ctx, const char *name, mpv_format format, void *data );
MPV_PTR( int,           mpv_set_property_string, mpv_handle *ctx, const char *name, const char *data );
MPV_PTR( int,           mpv_del_property, mpv_handle *ctx, const char *name );
MPV_PTR( int,           mpv_set_property_async, mpv_handle *ctx, uint64_t reply_userdata, const char *name, mpv_format format, void *data );
MPV_PTR( int,           mpv_get_property, mpv_handle *ctx, const char *name, mpv_format format, void *data );
MPV_PTR( char*,         mpv_get_property_string, mpv_handle *ctx, const char *name );
MPV_PTR( char*,         mpv_get_property_osd_string, mpv_handle *ctx, const char *name );
MPV_PTR( int,           mpv_get_property_async, mpv_handle *ctx, uint64_t reply_userdata, const char *name, mpv_format format );
MPV_PTR( int,           mpv_observe_property, mpv_handle *mpv, uint64_t reply_userdata, const char *name, mpv_format format );
MPV_PTR( int,           mpv_unobserve_property, mpv_handle *mpv, uint64_t registered_reply_userdata );
MPV_PTR( const char*,   mpv_event_name, mpv_event_id event );
MPV_PTR( int,           mpv_event_to_node, mpv_node *dst, mpv_event *src );
MPV_PTR( int,           mpv_request_event, mpv_handle *ctx, mpv_event_id event, int enable );
MPV_PTR( int,           mpv_request_log_messages, mpv_handle *ctx, const char *min_level );
MPV_PTR( mpv_event*,    mpv_wait_event, mpv_handle* ctx, double timeout );
MPV_PTR( void,          mpv_wakeup, mpv_handle *ctx );
MPV_PTR( void,          mpv_set_wakeup_callback, mpv_handle *ctx, void (*cb)(void *d), void *d );
MPV_PTR( void,          mpv_wait_async_requests, mpv_handle *ctx );
MPV_PTR( int,           mpv_hook_add, mpv_handle *ctx, uint64_t reply_userdata, const char *name, int priority );
MPV_PTR( int,           mpv_hook_continue, mpv_handle *ctx, uint64_t id );
MPV_PTR( int,           mpv_get_wakeup_pipe, mpv_handle *ctx );

// render.h
MPV_PTR( int,           mpv_render_context_create, mpv_render_context** res, mpv_handle* mpv, mpv_render_param* params );
MPV_PTR( int,           mpv_render_context_set_parameter, mpv_render_context* ctx, mpv_render_param param );
MPV_PTR( int,           mpv_render_context_get_info, mpv_render_context* ctx, mpv_render_param param );
MPV_PTR( void,          mpv_render_context_set_update_callback, mpv_render_context* ctx, mpv_render_update_fn callback, void* callback_ctx );
MPV_PTR( uint64_t,      mpv_render_context_update, mpv_render_context* ctx );
MPV_PTR( int,           mpv_render_context_render, mpv_render_context* ctx, mpv_render_param *params );
MPV_PTR( void,          mpv_render_context_report_swap, mpv_render_context* ctx );
MPV_PTR( void,          mpv_render_context_free, mpv_render_context* ctx );

#undef MPV_PTR

