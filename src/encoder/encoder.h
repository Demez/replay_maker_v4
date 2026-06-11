#pragma once

#include "clip/clip.h"

#include <atomic>
#include <mutex>
#include <string>

#include "vector.hpp"


constexpr int MAX_TARGET_SIZE_RETRY = 10;


enum e_target_size_state
{
	e_target_size_state_none,
	e_target_size_state_smaller,
	e_target_size_state_bigger,
};


enum e_enc_state
{
	e_enc_state_wait,
	e_enc_state_running,
	e_enc_state_finished,
	e_enc_state_skipped,
	e_enc_state_failed,
};


struct target_size_pass_t
{
	u32                 attempt         = 0;
	bool                success         = false;

	float*              prev_bitrates   = nullptr;  // unused
	float*              max_bitrates    = nullptr;
	float*              min_bitrates    = nullptr;
	float*              ffmpeg_bitrates = nullptr;

	float               prev_file_size  = 0;

	e_target_size_state last_state      = e_target_size_state_none;
};


struct video_segment_t
{
	char*             path;
	u32               source;  // refers to source in current group
	clip_time_range_t time;

	float             bitrate = 0.f;
};


// data used for processing a video
// used per encode preset
struct enc_segment_data_t
{
	video_segment_t*   data;
	u32                count;
	u32                index = UINT32_MAX;  // currently running segment
	target_size_pass_t target_size{};
};


struct enc_video_data_t
{
	clip_t&             clip;
	clip_group_t&       group;
	enc_segment_data_t& segment;
};


struct enc_export_info_t
{
	e_enc_state        state;
	enc_segment_data_t segment;
};


// encoder output video
struct enc_clip_t
{
	clip_t*                       clip  = nullptr;
	bool                          valid = false;

	e_enc_state                   state = e_enc_state_wait;
	ChVector< enc_export_info_t > exports{};
	u32                           export_index           = 0;

	// ffmpeg output
	char*                         ffmpeg_output          = nullptr;
	size_t                        ffmpeg_output_capacity = 0;
	size_t                        ffmpeg_cursor_pos      = 0;

	// markers for raw encodes here?
};


struct encoder_t
{
	std::string output_dir;
	std::string temp_video_dir;
	std::string log_dir;

	std::mutex  ffmpeg_output_lock;

	//enc_clip_t*         output_videos  = nullptr;

	// status info
	u32         scan_index = 0;

	std::mutex  info_lock;

	u32         clip_index       = 0;
	u32         clip_group_i     = 0;
	u32         clip_group_src_i = 0;

	u32         clip_index_prev  = 0;

	u32         encode_preset    = 0;
};


extern char                g_output_dir[ 512 ];
extern char                g_temp_video_dir[ 512 ];

extern enc_clip_t*         g_encoder_clips;

extern encoder_t           g_encoder_data;

extern std::atomic< bool > g_encode_started;

extern bool                g_encode_running;
extern bool                g_encode_finished;
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
