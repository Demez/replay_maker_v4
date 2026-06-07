#pragma once

#include "clip/clip.h"

#include <atomic>
#include <mutex>
#include <string>

enum e_target_size_state
{
	e_target_size_state_none,
	e_target_size_state_smaller,
	e_target_size_state_bigger,
};


// encoder output video
struct enc_clip_t
{
	clip_t*         clip  = nullptr;
	bool            valid = false;

	// presets this output video uses
	ChVector< u32 > presets;

	// ffmpeg output
	std::mutex      ffmpeg_output_lock;
	char*           ffmpeg_output          = nullptr;
	size_t          ffmpeg_output_capacity = 0;
	size_t          ffmpeg_cursor_pos      = 0;

	// markers for raw encodes here?
};


struct video_segment_t
{
	char* path;
	u32   source;  // refers to source in current group
	u32   time;

	float bitrate = 0.f;
};


// data used for processing a video
// used per encode preset
struct enc_video_data_t
{
	enc_clip_t&      enc_clip;
	clip_t&          clip;
	clip_group_t&    group;

	video_segment_t* segment;
	u32              segment_count;
};


struct encoder_t
{
	std::string         output_dir;
	std::string         temp_video_dir;
	std::string         log_dir;

	//enc_clip_t*         output_videos  = nullptr;

	// status info
	u32                 encode_preset  = 0;
	u32                 output_index   = 0;
	u32                 scan_index     = 0;
};


extern char                g_output_dir[ 512 ];
extern char                g_temp_video_dir[ 512 ];

extern enc_clip_t*         g_encoder_clips;

extern encoder_t           g_encoder_data;

extern std::atomic< bool > g_encode_started;

extern bool                g_encode_running;
extern bool                g_encode_pause;

void                       run_encoding();
void                       encode_videos();
bool                       encode_init();

void                       encode_thread_start();
void                       encode_thread_stop();

bool                       encode_check_state();

float                      get_video_bitrate( const char* path );
std::string                get_video_output_name( clip_t& clip, clip_encode_preset_t& preset );

void                       encode_draw();
