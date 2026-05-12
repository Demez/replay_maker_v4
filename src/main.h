#pragma once

#include "util.h"
#include "logging.h"
#include "imgui.h"
#include "encoder/encoder.h"
#include "clip/clip.h"

#include "glad.h"
#include "SDL3/SDL.h"

#include "mpv/client.h"
#include "mpv/render.h"
#include "mpv/render_gl.h"

#include <string>


// --------------------------------------------------------------------------------------------------------


enum e_clip_parse_state
{
	e_clip_parse_state_idle,
	e_clip_parse_state_running,
	e_clip_parse_state_finished,
};


// metadata grabbed on video load from mpv
struct video_media_info_t
{
	s64 track_count = 0;
	s64 track_count_video = 0;
	s64 track_count_audio = 0;
};


extern video_media_info_t g_video_media_info;


// editor commands
enum e_cmd : u8
{
	e_cmd_mpv,  // an mpv command
	e_cmd_toggle_sidebar,
	e_cmd_toggle_fullscreen,
	e_cmd_save_file,

	// timeline controls
	e_cmd_timeline_marker_set_a,
	e_cmd_timeline_marker_set_b,
	e_cmd_timeline_marker_clear_a,
	e_cmd_timeline_marker_clear_b,
	e_cmd_timeline_marker_snap_a,
	e_cmd_timeline_marker_snap_b,
	e_cmd_timeline_section_create,
	e_cmd_timeline_section_delete,
	e_cmd_timeline_section_snap_left,
	e_cmd_timeline_section_snap_right,
};


enum e_mpv_cmd
{
	e_mpv_cmd_normal,
	e_mpv_cmd_seek,
};


constexpr ImVec4 COLOR_BTN_RED_ACTIVE{ 0.9f, 0.1f, 0.1f, 1.0f };
constexpr ImVec4 COLOR_BTN_RED_HOVER{ 0.7f, 0.2f, 0.2f, 1.0f };
constexpr ImVec4 COLOR_BTN_RED{ 0.5f, 0.1f, 0.1f, 1.0f };

constexpr ImVec4 COLOR_GREEN_ACTIVE{ 0.1f, 0.9f, 0.1f, 1.0f };
constexpr ImVec4 COLOR_GREEN_HOVER{ 0.2f, 0.7f, 0.2f, 1.0f };
constexpr ImVec4 COLOR_GREEN{ 0.1f, 0.5f, 0.1f, 1.0f };

constexpr ImVec4 COLOR_PURPLE_ACTIVE{ 0.8f, 0.0f, 1.f, 1.0f };
constexpr ImVec4 COLOR_PURPLE_HOVER{ 0.67f, 0.0f, 0.8f, 1.0f };
constexpr ImVec4 COLOR_PURPLE{ 0.55f, 0.0f, 0.65f, 1.0f };


namespace font
{
	extern ImFont* normal;
	extern ImFont* normal_bold;
	extern ImFont* normal_italic;

	extern ImFont* console;

	extern u32     size;
}


// --------------------------------------------------------------------------------------------------------
// system stuff

struct ImGuiContext;

void                               sys_mpv_full_window_enter();
void                               sys_mpv_full_window_exit();
void                               sys_mpv_full_window_toggle();

u64                                sys_get_time_ms();
void                               sys_set_window( SDL_Window* window );

// --------------------------------------------------------------------------------------------------------
// imgui helper functions

bool                               point_in_rect( ImVec2 point, ImVec2 min_size, ImVec2 max_size );
bool                               mouse_in_rect( ImVec2 min_size, ImVec2 max_size );

// --------------------------------------------------------------------------------------------------------
// MPV

bool                               load_mpv_dll();
void                               unload_mpv_dll();

bool                               start_mpv();
void                               stop_mpv();

char*                              mpv_get_current_video();
void                               mpv_draw_frame();
void                               mpv_window_resize();

void                               mpv_cmd_loadfile( const char* file );
void                               mpv_cmd_close_video();
void                               mpv_cmd_toggle_playback();
void                               mpv_cmd_seek_offset( double seconds );

void                               mpv_cmd_hook_window( void* window );
void                               mpv_cmd_hook_window_mpv();

void                               mpv_handle_error();

extern mpv_handle*                 g_mpv;
extern mpv_render_context*         g_mpv_gl;

// --------------------------------------------------------------------------------------------------------
// Keybindings


using e_mod_mask = u8;
enum : e_mod_mask
{
	e_mod_mask_none    = 0,
	e_mod_mask_ctrl_l  = ( 1 << 0 ),
	e_mod_mask_ctrl_r  = ( 1 << 1 ),
	e_mod_mask_shift_l = ( 1 << 2 ),
	e_mod_mask_shift_r = ( 1 << 3 ),
	e_mod_mask_alt_l   = ( 1 << 4 ),
	e_mod_mask_alt_r   = ( 1 << 5 ),
	e_mod_mask_gui_l   = ( 1 << 6 ),
	e_mod_mask_gui_r   = ( 1 << 7 ),
};


constexpr e_mod_mask               e_mod_mask_count = 8;


void                               handle_keybinds();


// --------------------------------------------------------------------------------------------------------

// ffprobe stuff
bool                               get_video_metadata( const char* path, video_metadata_t& metadata );
float                              get_video_bitrate( const char* path );

bool                               valid_time_range( clip_time_range_t& range, video_metadata_t& metadata );

void                               calc_imgui_window_size( int index, ivec2& size );
void                               calc_playback_window_size( ivec2& size );
void                               calc_replay_window_size( ivec2& size );

void                               get_media_info();

void                               draw_imgui_window( int window_size[ 2 ] );
void                               draw_replay_editor_window( int window_size[ 2 ] );
void                               draw_playback_controls( int window_size[ 2 ], bool draw_volume );

// void                               replay_editor_load_input( u32 output_i, u32 input_i );
void                               replay_editor_set_group( u32 output_i, u32 group_i, u32 group_src_i );
void                               replay_editor_reset();

void                               draw_preset_dropdown( clip_output_video_t& output, clip_output_group_t& group, bool edit );

void                               enable_sidebar( bool enabled );
void                               window_on_resize();
void                               window_render_all();
void                               update_dpi( float dpi_override = 0.f );

// background clip data parsing
void                               clip_thread_open_file( clip_data_t* data, const char* path );
e_clip_parse_state                 clip_thread_state();
bool                               clip_thread_loading();
bool                               clip_thread_idle();

// save encode presets and video prefixes
void                               save_settings();

// save videos to currently opened file
void                               save_videos();


void                               write_recently_opened();
void                               load_recently_opened();
void                               update_recently_opened( const char* clips_file );

extern bool                        g_running;
extern bool                        g_fullscreen;
extern bool                        g_in_window_drag;
extern bool                        g_in_drag_drop;
extern bool                        g_in_draw;
extern bool                        g_pause_window_events;
extern float                       g_dpi;

extern SDL_Window*                 g_main_window;

extern ivec2                       g_mouse_pos;
extern ivec2                       g_mouse_delta;

extern ivec2                       g_mpv_size;
extern ivec2                       g_window_size;
extern bool                        g_show_sidebar;

extern char*                       g_videos_file_path;

extern char*                       g_recently_opened_path;
extern char**                      g_recently_opened;
extern u8                          g_recently_opened_count;

extern bool                        g_preset_combo_open;

extern float                       g_frame_time;

extern float                       g_save_timer;

constexpr u8                       MAX_RECENT_OPEN = 8;

constexpr int                      DIVIDER_SIZE = 5;  // multiplied by 2

#define RECENTLY_OPENED_FILE "replay_maker_recent.txt"

// --------------------------------------------------------------------------------------------------------
// Timeline

void          timeline_reset();
void          timeline_draw();

void          timeline_seek( float seconds );
void          timeline_advance( bool prev = false );

extern ImVec2 g_timeline_size;
extern ImVec2 g_timeline_pos;

// --------------------------------------------------------------------------------------------------------
// Undo System


// UNDO HISTORY DATA TO STORE
// 
// Timeline:
// - Section Resizing
// - Adding a section
// - Deleting a section
// - Adding a marker
// - Moving a marker
// - Deleting a marker
// 
// Clip List:
// - Adding an output video
// - Deleting an output video
// - Adding a video entry
// - Deleting a video entry
// 


enum e_action
{
	e_action_invalid,

	// e_action_timeline_section_add,
	// e_action_timeline_section_delete,
	// e_action_timeline_section_resize,
	e_action_timeline_section_modify,
	e_action_timeline_marker_modify,

	e_action_clip_output_add,
	e_action_clip_output_delete,
	e_action_clip_output_rename,

	e_action_clip_input_add,
	e_action_clip_input_delete,

	e_action_count,
};


struct clip_time_range_t;


// Undo Action Structs
struct undo_action_timeline_section_t
{
	u32                output_video;
	u32                input_video;
	u32                time_range_count;
	clip_time_range_t* time_ranges;
};


struct undo_action_marker_modify_t
{
	bool  marker_active[ 2 ];
	float marker_times[ 2 ];
};


struct undo_action_output_rename_t
{
	char* name;
};


struct undo_action_t
{
	e_action type;

	union
	{
		undo_action_timeline_section_t section_modify;
		undo_action_marker_modify_t    marker_modify;
		undo_action_output_rename_t    output_rename;
	};
};


bool           undo_history_init();
void           undo_history_shutdown();

// Returns a new undo_action_t pointer, where you can fill in your data for it there, do not store this pointer anywhere
undo_action_t* undo_history_add_action( e_action type );

void           undo_history_do_undo();
void           undo_history_do_redo();


// --------------------------------------------------------------------------------------------------------
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

