#pragma once

#include "util.h"
#include "clip/json5.h"


constexpr size_t MAX_LEN_EXT         = 5;
constexpr size_t MAX_LEN_PRESET_NAME = 64;


enum e_encode_preset
{
	e_encode_preset_invalid,
	e_encode_preset_move,         // only moves the file
	e_encode_preset_target_size,  // re-encodes until it gets close to the target size
	e_encode_preset_raw,          // high quality re-encoding to reduce file size
	e_encode_preset_raw_chapters, // doesn't re-encode it, but only adds chapters to the input video, as we expect it to be encoded already
};


// controls how to use parts of videos in a encode preset
struct clip_encode_override_t
{
	char* ffmpeg_cmd;  // overrides the encode preset's ffmpeg cmd if not nullptr

	u32*  presets;
	u32   presets_count;

	// if true, the presets array will be set to exclude mode instead of
	bool  preset_exclude;
};


struct clip_time_range_t
{
	float                  start;
	float                  end;

	clip_encode_override_t encode_overrides;
};


struct clip_input_video_t
{
	char*                  path;

	clip_time_range_t*     time_range;
	u32                    time_range_count;

	clip_encode_override_t encode_overrides;
};


struct clip_output_video_t
{
	char*                  name;

	clip_input_video_t*    input;
	u32                    input_count;

	clip_encode_override_t encode_overrides;
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
	u32   audio_bitrate;

	// u32   inherit_from;  // id of a preset to inherit from?
	// u32   use_prefix_from;  // id of a preset to use as a output prefix

	// bool  two_pass;
	bool  use_full_video;  // use full video instead of time range by default, can be overridden if we have time ranges, and uses the input time as markers (TODO: take markers from what preset?)
};


struct clip_prefix_t
{
	char  name[ MAX_LEN_PRESET_NAME ];
	char* prefix;
};


struct clip_data_t
{
	char**                search_path;
	u32                   search_path_count;

	clip_output_video_t*  output;
	u32                   output_count;

	clip_encode_preset_t* preset;
	u32                   preset_count;

	clip_prefix_t*        prefix;
	u32                   prefix_count;
};


// ----------------------------------------------------------------------------


clip_data_t*          clip_create();
void                  clip_free( clip_data_t* data );
// clip_data_t* clip_load_from_json5( const char* path );

// void clip_save_to_json5( const char* path, clip_data_t* data );

u32                   clip_add_prefix( clip_data_t* data, const char* name, const char* prefix );
clip_encode_preset_t* clip_add_encode_preset( clip_data_t* data, const char* name, const char* ext );
void                  clip_add_search_path( clip_data_t* data, const char* path );

clip_output_video_t*  clip_add_output( clip_data_t* data, const char* name );
clip_input_video_t*   clip_add_input( clip_output_video_t* output, const char* path );

void                  clip_add_time_range( clip_output_video_t* output, u32 input_i, float start_time, float end_time );
void                  clip_remove_time_range( clip_output_video_t* output, u32 input_i, u32 time_range );

void                  clip_add_preset_to_encode_override( clip_data_t* data, clip_encode_override_t& override, const char* preset_name );
void                  clip_add_preset_to_encode_override( clip_data_t* data, clip_encode_override_t& override, u32 preset_index );

