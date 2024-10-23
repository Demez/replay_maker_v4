#pragma once

#include "clip/clip.h"


enum e_target_size_state
{
	e_target_size_state_none,
	e_target_size_state_smaller,
	e_target_size_state_bigger,
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


// encoder output video
struct enc_output_video_t
{
	clip_output_video_t* output;

	// array of video metadata, index is equal to input video index in output
	video_metadata_t*    metadata;

	// presets this output video uses
	u32*                 presets;
	u32                  presets_count;

	bool                 valid;

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


extern const char*         g_video_files;
extern const char*         g_output_dir;
extern const char*         g_temp_video_dir;

extern clip_data_t*        g_clip_data;
extern enc_output_video_t* g_output_videos;


void                       run_encoding();
float                      get_video_bitrate( const char* path );

