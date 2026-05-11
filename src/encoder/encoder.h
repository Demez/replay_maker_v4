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
struct enc_output_video_t
{
	clip_output_video_t* output;

	// presets this output video uses
	u32*                 presets;
	u32                  presets_count;

	// ffmpeg output
	std::mutex           ffmpeg_output_lock;
	char*                ffmpeg_output          = nullptr;
	size_t               ffmpeg_output_capacity = 0;
	size_t               ffmpeg_cursor_pos      = 0;

	// markers for raw encodes here?
};


struct video_segment_t
{
	char* path;
	u32   input;
	u32   time;

	float bitrate = 0.f;
};


// data used for processing a video
struct enc_video_data_t
{
	enc_output_video_t*  enc_output;
	clip_output_video_t* output;

	video_segment_t*     segment;
	u32                  segment_count;
};


struct encoder_t
{
	std::string         output_dir;
	std::string         temp_video_dir;
	std::string         log_dir;

	enc_output_video_t* output_videos  = nullptr;

	// status info
	u32                 encode_preset  = 0;
	u32                 output_index   = 0;
	u32                 scan_index     = 0;
};


extern char                g_output_dir[ 512 ];
extern char                g_temp_video_dir[ 512 ];

extern clip_data_t*        g_clip_data;
extern enc_output_video_t* g_output_videos;

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
std::string                get_video_output_name( clip_output_video_t& output, clip_encode_preset_t& preset );

void                       encode_draw();
