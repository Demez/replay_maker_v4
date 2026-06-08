#include "main.h"
#include "args.h"
#include "clip/clip.h"
#include "encoder/encoder.h"
#include "util.h"

#include <thread>

char                g_output_dir[ 512 ];
char                g_temp_video_dir[ 512 ];

enc_clip_t*         g_encoder_clips   = nullptr;

std::atomic< bool > g_encode_started  = false;
bool                g_encode_running  = false;
bool                g_encode_finished = false;
bool                g_encode_pause    = false;

encoder_t           g_encoder_data{};

// Encode Thread
std::thread*        g_encode_thread  = nullptr;


static void         encode_worker()
{
	g_encode_running = true;
	encode_videos();
	g_encode_finished = true;
	// g_encode_running  = false;

	printf( "ENCODE FINISHED\n" );
}


void encode_thread_start()
{
	if ( g_encode_finished )
		encode_thread_stop();

	if ( g_encode_thread )
	{
		log_printf( log_error, "Encode thread already started, user should not be able to reach this!\n" );
		return;
	}

	g_encoder_data.scan_index = 0;
	g_encode_started          = false;
	app::fullscreen              = false;

	const char* cmd[]         = { "set", "pause", "yes", NULL };
	int         cmd_ret       = p_mpv_command_async( get_mpv(), 0, cmd );

	g_encode_thread = new std::thread( encode_worker );
}


void encode_thread_stop()
{
	g_encode_running  = false;
	g_encode_started  = false;
	g_encode_finished = false;

	if ( g_encode_thread )
	{
		g_encode_thread->join();
		g_encode_thread = nullptr;
	}

	// TODO: FREE MEMORY !!!!
}


bool encode_check_state()
{
	//if ( g_encode_finished )
	//	encode_thread_stop();

	if ( !g_encode_running || !app::running || g_encode_finished )
		return false;
	
	while ( g_encode_pause )
	{
		SDL_Delay( 500 );
	}

	return true;
}

#if 0

bool used_in_preset( clip_encode_settings_t& override, u32 preset_i )
{
	for ( u32 i = 0; i < override.presets_count; i++ )
	{
		if ( override.presets[ i ] == preset_i )
		{
			return true;
		}
	}

	// return !override.presets_count;
	return false;
}

#endif


enc_clip_preset_t* encode_get_enc_preset( enc_clip_t& enc_clip, u32 preset_idx )
{
	// check if this video is used on this preset
	for ( u32 i = 0; i < enc_clip.presets.size(); i++ )
	{
		if ( enc_clip.presets[ i ].preset == preset_idx )
			return &enc_clip.presets[ i ];
	}

	return nullptr;
}


bool collect_video_info()
{
	if ( clip_data::prefix_count == 0 )
		return false;

	if ( clip_data::preset_count == 0 )
		return false;

	if ( clip_data::clip_count == 0 )
		return false;

	// build encoder clip data
	g_encoder_clips = ch_calloc< enc_clip_t >( clip_data::clip_count );

	if ( !g_encoder_clips )
	{
		log_printf( "failed to allocate memory for output videos\n" );
		return false;
	}
	
	for ( u32 clip_i = 0; clip_i < clip_data::clip_count; clip_i++ )
	{
		clip_t&     clip          = clip_data::clip[ clip_i ];
		enc_clip_t& enc_clip      = g_encoder_clips[ clip_i ];
		enc_clip.clip             = &clip;
		g_encoder_data.scan_index = clip_i;

		if ( !clip.enabled )
			continue;

		clip_check_video( clip, true );

		// don't allow invalid videos at all
		if ( clip.state == e_clip_state_invalid )
			return false;

		// make a quick list of all presets the clip uses
		for ( u32 group_i = 0; group_i < clip.groups.size(); group_i++ )
		{
			clip_group_t& group = clip.groups[ group_i ];

			for ( u32 preset_i = 0; preset_i < group.presets.size(); preset_i++ )
			{
				if ( group.presets[ preset_i ] > clip_data::preset_count )
					continue;

				bool preset_already_added = false;
				for ( u32 search_i = 0; search_i < enc_clip.presets.size(); search_i++ )
				{
					if ( enc_clip.presets[ search_i ].preset == group.presets[ preset_i ] )
					{
						preset_already_added = true;
						break;
					}
				}

				if ( preset_already_added )
					continue;

				enc_clip_preset_t enc_preset
				{
					.preset = group.presets[ preset_i ],
					.group  = group_i,
					//.group   = group
				};

				enc_clip.presets.push_back( enc_preset );
			}
		}

		enc_clip.valid = true;
	}

	return true;

#if 0
	g_encoder_clips = ch_calloc< enc_clip_t >( clip_data::clip_count );

	if ( !g_encoder_clips )
	{
		log_printf( "failed to allocate memory for output videos\n" );
		return false;
	}

	// verify each video and determine presets to use for videos
	log_printf( 
	  "====================================================================\n"
	  "Demez Replay Encoder\n"
	  "\n"
	  "Output Directory: \"%s\"\n"
	  "\n"
	  "%d Encode Presets\n"
	  "%d Video Prefixes\n"
	  "\n"
	  "%d Output Videos\n"
	  "====================================================================\n",
	  g_output_dir,
	  clip_data::preset_count, clip_data::prefix_count, clip_data::clip_count );

	// check this for each encode preset
	for ( u32 out_i = 0; out_i < clip_data::clip_count; out_i++ )
	{
		clip_t& clip     = clip_data::clip[ out_i ];
		enc_clip_t&  enc_clip = g_encoder_clips[ out_i ];
		enc_clip.clip               = &clip;
		g_encoder_data.scan_index       = out_i;

		log_printf( "%s%s\n", clip_data::prefix[ clip.prefix ].prefix, clip.name );

		// ----------------------------------------------------------------------------------------
		// determine encode presets for this output video

		// figure out what encode presets this runs on
		for ( u32 in_i = 0; in_i < clip.source_count; in_i++ )
		{
			clip_source_t& source = clip.source[ in_i ];

			for ( u32 preset_i = 0; preset_i < source.encode_settings.presets_count; preset_i++ )
			{
				bool preset_already_added = false;
				for ( u32 search_i = 0; search_i < enc_clip.presets_count; search_i++ )
				{
					if ( enc_clip.presets[ search_i ] == source.encode_settings.presets[ preset_i ] )
					{
						preset_already_added = true;
						break;
					}
				}

				if ( preset_already_added )
					continue;

				// add it to this list
				u32* new_data = ch_realloc< u32 >( enc_clip.presets, enc_clip.presets_count + 1 );

				if ( !new_data )
				{
					log_printf( "failed to allocate data for storing output video presets\n" );
					return false;
				}

				enc_clip.presets                               = new_data;
				enc_clip.presets[ enc_clip.presets_count++ ] = source.encode_settings.presets[ preset_i ];
			}
		}

		log_printf( "%d Encode Presets Used: ", enc_clip.presets_count );

		for ( u32 preset_i = 0; preset_i < enc_clip.presets_count; preset_i++ )
		{
			log_printf( "\"%s\" ", clip_data::preset[ enc_clip.presets[ preset_i ] ].name );
		}

		log_printf( "\n" );

		// ----------------------------------------------------------------------------------------
		// get source video metadata

		bool all_valid            = true;

		// find all unique source videos, there will be duplicates for different encode presets
		// this way we don't need get metadata for the same video multiple times

		for ( u32 in_i = 0; in_i < clip.source_count; in_i++ )
		{
			clip_source_t& source = clip.source[ in_i ];

			if ( source.file_missing || !fs_exists( source.path ) )
			{
				all_valid          = false;
				source.file_missing = true;
				break;
			}
		}

		// ----------------------------------------------------------------------------------------
		// print data for each encode preset

		for ( u32 preset_i = 0; preset_i < enc_clip.presets_count; preset_i++ )
		{
			clip_encode_preset_t& preset = clip_data::preset[ enc_clip.presets[ preset_i ] ];
			log_printf( "\nEncode Preset: %s\n", preset.name );

			log_printf(
			  "    Output:   %s/%s%s%s.%s\n",
			  preset.out_folder_append ? preset.out_folder_append : "",
			  preset.out_prefix ? preset.out_prefix : "",
			  clip_data::prefix[ clip.prefix ].prefix,
			  clip.name,
			  preset.ext );

			float duration         = 0.f;
			bool  duration_invalid = false;
			for ( u32 in_i = 0; in_i < clip.source_count; in_i++ )
			{
				clip_source_t& source = clip.source[ in_i ];

				if ( !used_in_preset( source.encode_settings, preset_i ) )
					continue;

				for ( u32 time_i = 0; time_i < source.time_range_count; time_i++ )
				{
					if ( !valid_time_range( source.time_range[ time_i ], source.metadata ) )
						duration_invalid = true;

					duration += source.time_range[ time_i ].end - source.time_range[ time_i ].start;
				}
			}

			log_printf( "    Duration: %.4f%s\n\n", duration, duration_invalid ? " [INVALID]" : "" );

			// print source videos for this preset and their time ranges
			for ( u32 in_i = 0; in_i < clip.source_count; in_i++ )
			{
				clip_source_t& source = clip.source[ in_i ];

				// validate preset
				if ( !used_in_preset( source.encode_settings, preset_i ) )
					continue;

				log_printf( "    %s%s\n", source.path, source.file_missing ? " [INVALID]" : "" );

				if ( source.file_missing )
					continue;

				for ( u32 time_i = 0; time_i < source.time_range_count; time_i++ )
				{
					bool  valid          = valid_time_range( source.time_range[ time_i ], source.metadata );
					float range_duration = source.time_range[ time_i ].end - source.time_range[ time_i ].start;

					log_printf( "        %.4f - %.4f (%.4f)%s\n", source.time_range[ time_i ].start, source.time_range[ time_i ].end, range_duration, valid ? "" : " [INVALID]" );
				}
			}
		}

		clip.state = e_clip_state_valid;
		log_printf( "----------------------------------------------------\n" );
	}

	return true;
#endif
}


void encode_videos()
{
	if ( !fs_make_dir_check( g_temp_video_dir ) )
	{
		return;
	}

	if ( !fs_make_dir_check( g_output_dir ) )
	{
		return;
	}

	if ( clip_data::clip_count == 0 )
	{
		log_printf( log_error, "no output videos found!\n" );
		return;
	}

	if ( !collect_video_info() )
		return;

	g_encode_started.store( true );

	run_encoding();
}


bool encode_init()
{
	// TODO: store in config and video json file for overrides
	// move temp to settings and don't allow override for that?
	const char* exe_dir = sys_get_exe_folder();

	if ( strlen( g_output_dir ) == 0 )
	{
		snprintf( g_output_dir, 512, "%s" SEP_S "output", exe_dir );
	}

	if ( strlen( g_temp_video_dir ) == 0 )
	{
		snprintf( g_temp_video_dir, 512, "%s" SEP_S "temp", exe_dir );
	}

	return true;
}

