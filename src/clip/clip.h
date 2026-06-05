#pragma once

#include "util.h"
#include "clip/json5.h"

#include <unordered_map>
#include <string>


constexpr size_t MAX_LEN_EXT               = 5;
constexpr size_t MAX_LEN_PRESET_NAME       = 64;

constexpr u32    CLIP_VIDEO_FORMAT_VER     = 4;
constexpr u32    CLIP_VIDEO_FORMAT_VER_MIN = 3;
constexpr u32    CLIP_SETTINGS_VER         = 1;


enum e_encode_preset
{
	e_encode_preset_invalid,
	e_encode_preset_move,         // only moves the file
	e_encode_preset_target_size,  // re-encodes until it gets close to the target size
	e_encode_preset_raw,          // high quality re-encoding to reduce file size
	e_encode_preset_raw_chapters, // doesn't re-encode it, but only adds chapters to the source video, as we expect it to be encoded already
};


enum e_clip_state
{
	e_clip_state_invalid,  // there is something wrong with the video in some way that prevents us from processing it
	e_clip_state_wait,
	e_clip_state_running,
	e_clip_state_finished,
	e_clip_state_already_finished,
	e_clip_state_user_skipped,
	e_clip_state_failed,

	e_clip_state_count,
};


struct video_metadata_t
{
	int   width    = 0.f;
	int   height   = 0.f;
	float bitrate  = 0.f;
	float duration = 0.f;
	float fps      = 0.f;

	// stored as 24/1 fps
	float fps_num  = 0.f;
	float fps_den  = 0.f;
};


struct clip_time_range_t
{
	float start;
	float end;
};


struct clip_source_t
{
	char*                  path;
	char*                  filename;

	//clip_time_range_t*     time_range;        // OBSOLETE
	//u32                    time_range_count;  // OBSOLETE

	//clip_encode_settings_t encode_settings;  // OBSOLETE

	video_metadata_t       metadata;
	bool                   file_missing;
};


struct clip_source_usage_t
{
	ChVector< clip_time_range_t > time_range;
	u32                           source_index;
};


struct clip_group_t
{
	ChVector< clip_source_usage_t > sources;
	ChVector< u32 >                 presets;
};


struct clip_t
{
	char*                    name;

	clip_source_t*           source;
	u32                      source_count;

	u32                      prefix;

	// these are the real outputs this video has, use for format 4
	ChVector< clip_group_t > groups;

	e_clip_state           state;
	bool                     enabled;  // if false, don't encode this video
};


struct clip_encode_preset_t
{
	char  name[ MAX_LEN_PRESET_NAME ];
	char  ext[ MAX_LEN_EXT ];  // webm or mkv

	// folder to place videos encoded with this preset in
	// if this is set to "raw", then videos will be placed in "output/raw/*" instead of "output/*"
	char* out_folder_append;
	char* out_prefix;  // adds "raw_" to raw encoded videos before the prefix setting is applied

	char* ffmpeg_cmd;
	char* move_folder;

	// for discord videos
	u32   target_size;  // if this is not 0, it will keep re-encoding until it gets close to this size
	u32   target_size_max;
	u32   target_size_min;

	u32   audio_bitrate;

	// u32   inherit_from;  // id of a preset to inherit from?
	// u32   use_prefix_from;  // id of a preset to use as a output prefix

	// bool  two_pass;
	bool  use_full_video;  // use full video instead of time range by default, can be overridden if we have time ranges, and uses the source time as markers (TODO: take markers from what preset?)
};


struct clip_prefix_t
{
	char  name[ MAX_LEN_PRESET_NAME ];
	char* prefix;
};


namespace clip_data
{
	extern u32                   version;

	extern clip_t*               clip;
	extern u32                   clip_count;

	extern clip_encode_preset_t* preset;
	extern u32                   preset_count;

	extern clip_prefix_t*        prefix;
	extern u32                   prefix_count;

	extern clip_t*               current_clip;
	extern u32                   current_clip_index;
	extern u32                   current_source;
	extern u32                   current_group_source;
	extern u32                   current_group;
}


// ----------------------------------------------------------------------------
// Clip Data Mangagement


void                  clip_data_reset();

bool                  clip_parse_settings( const char* path );
bool                  clip_parse_videos( const char* path );

void                  clip_get_video_metadata( clip_source_t& source );
void                  clip_check_video( clip_t& clip );
void                  clip_check_videos();

void                  clip_save_settings( const char* path );
bool                  clip_save_videos( const char* path );

// clip_data_t* clip_load_from_json5( const char* path );

// void clip_save_to_json5( const char* path,);

clip_prefix_t*        clip_create_prefix();
clip_encode_preset_t* clip_create_encode_preset();

u32                   clip_add_prefix( const char* name, const char* prefix );
clip_encode_preset_t* clip_add_encode_preset( const char* name, const char* ext );


// ----------------------------------------------------------------------------
// Video Management


clip_t*               clip_add_entry( const char* name );

void                  clip_remove_entry( clip_t* clip );
void                  clip_remove_entry( u32 entry_i );

void                  clip_move_entry( u32 entry_id, u32 insert_position );

void                  clip_remove_source( clip_t* clip, u32 source_i );

// Groups
clip_group_t*         clip_get_group( clip_t* clip, u32 group_index );
std::string           clip_group_get_name( clip_group_t& group );
u32                   clip_group_add_source( clip_t* clip, u32 group_index, const char* path );
void                  clip_group_remove_source( clip_t* clip, u32 group_index, u32 group_src_i );
// void                  clip_group_remove_source( clip_t* output, u32 group_index, const char* path );

void                  clip_group_add_preset( clip_t& clip, clip_group_t& group, u32 preset_i );

void                  clip_group_remove_preset( clip_t& clip, clip_group_t& group, u32 preset_i );
void                  clip_group_remove_preset( clip_t& clip, u32 group_index, u32 preset_i );

//u32                   clip_duplicate_input( clip_t* output, u32 input_i );

void                  clip_group_add_time_range( clip_t* clip, clip_group_t& group, u32 source_i, float start_time, float end_time );
void                  clip_group_remove_time_range( clip_t* clip, clip_group_t& group, u32 source_i, u32 time_range );

// direction = false for back, true for forward
void                  clip_group_shift_time_range( clip_t* clip, clip_group_t& group, u32 source_i, u32 time_range, bool direction );

//void                  clip_duplicate_time_range( clip_t* output, u32 input_i, u32 time_range );

