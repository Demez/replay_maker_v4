#include "main.h"
#include "args.h"
#include "clip/clip.h"
#include "encoder/encoder.h"
#include "util.h"

#include <thread>

char                g_output_dir[ 512 ];
char                g_temp_video_dir[ 512 ];

enc_output_video_t* g_output_videos  = nullptr;

std::atomic< bool > g_encode_started = false;
bool                g_encode_running = false;
bool                g_encode_pause   = false;

encoder_t           g_encoder_data{};

// Encode Thread
std::thread*        g_encode_thread  = nullptr;


static void         encode_worker()
{
	g_encode_running = true;
	encode_videos();
}


void encode_thread_start()
{
	if ( g_encode_thread )
	{
		log_printf( log_error, "Encode thread already started, user should not be able to reach this!\n" );
		return;
	}

	g_encoder_data.scan_index = 0;
	g_encode_started          = false;
	g_fullscreen              = false;

	const char* cmd[]         = { "set", "pause", "yes", NULL };
	int         cmd_ret       = p_mpv_command_async( g_mpv, 0, cmd );

	g_encode_thread = new std::thread( encode_worker );
}


void encode_thread_stop()
{
	g_encode_running = false;
	g_encode_started = false;

	if ( g_encode_thread )
	{
		g_encode_thread->join();
		g_encode_thread = nullptr;
	}

	// TODO: FREE MEMORY !!!!
}


bool encode_check_state()
{
	if ( !g_encode_running || !g_running )
		return false;
	
	while ( g_encode_pause )
	{
		SDL_Delay( 500 );
	}

	return true;
}


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


bool collect_video_info()
{
	g_output_videos = ch_calloc< enc_output_video_t >( g_clip_data->output_count );

	if ( !g_output_videos )
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
	  g_clip_data->preset_count, g_clip_data->prefix_count, g_clip_data->output_count );

	// check this for each encode preset
	for ( u32 out_i = 0; out_i < g_clip_data->output_count; out_i++ )
	{
		clip_output_video_t& output     = g_clip_data->output[ out_i ];
		enc_output_video_t&  enc_output = g_output_videos[ out_i ];
		enc_output.output               = &output;
		g_encoder_data.scan_index       = out_i;

		log_printf( "%s%s\n", g_clip_data->prefix[ output.prefix ].prefix, output.name );

		// ----------------------------------------------------------------------------------------
		// determine encode presets for this output video

		// figure out what encode presets this runs on
		for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
		{
			clip_input_video_t& input = output.input[ in_i ];

			for ( u32 preset_i = 0; preset_i < input.encode_settings.presets_count; preset_i++ )
			{
				bool preset_already_added = false;
				for ( u32 search_i = 0; search_i < enc_output.presets_count; search_i++ )
				{
					if ( enc_output.presets[ search_i ] == input.encode_settings.presets[ preset_i ] )
					{
						preset_already_added = true;
						break;
					}
				}

				if ( preset_already_added )
					continue;

				// add it to this list
				u32* new_data = ch_realloc< u32 >( enc_output.presets, enc_output.presets_count + 1 );

				if ( !new_data )
				{
					log_printf( "failed to allocate data for storing output video presets\n" );
					return false;
				}

				enc_output.presets                               = new_data;
				enc_output.presets[ enc_output.presets_count++ ] = input.encode_settings.presets[ preset_i ];
			}
		}

		log_printf( "%d Encode Presets Used: ", enc_output.presets_count );

		for ( u32 preset_i = 0; preset_i < enc_output.presets_count; preset_i++ )
		{
			log_printf( "\"%s\" ", g_clip_data->preset[ enc_output.presets[ preset_i ] ].name );
		}

		log_printf( "\n" );

		// ----------------------------------------------------------------------------------------
		// get input video metadata

		bool all_valid            = true;

		// find all unique input videos, there will be duplicates for different encode presets
		// this way we don't need get metadata for the same video multiple times

		for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
		{
			clip_input_video_t& input = output.input[ in_i ];

			if ( input.file_missing || !fs_exists( input.path ) )
			{
				all_valid          = false;
				input.file_missing = true;
				break;
			}
		}

		// ----------------------------------------------------------------------------------------
		// print data for each encode preset

		for ( u32 preset_i = 0; preset_i < enc_output.presets_count; preset_i++ )
		{
			clip_encode_preset_t& preset = g_clip_data->preset[ enc_output.presets[ preset_i ] ];
			log_printf( "\nEncode Preset: %s\n", preset.name );

			log_printf(
			  "    Output:   %s/%s%s%s.%s\n",
			  preset.out_folder_append ? preset.out_folder_append : "",
			  preset.out_prefix ? preset.out_prefix : "",
			  g_clip_data->prefix[ output.prefix ].prefix,
			  output.name,
			  preset.ext );

			float duration         = 0.f;
			bool  duration_invalid = false;
			for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
			{
				clip_input_video_t& input = output.input[ in_i ];

				if ( !used_in_preset( input.encode_settings, preset_i ) )
					continue;

				for ( u32 time_i = 0; time_i < input.time_range_count; time_i++ )
				{
					if ( !valid_time_range( input.time_range[ time_i ], input.metadata ) )
						duration_invalid = true;

					duration += input.time_range[ time_i ].end - input.time_range[ time_i ].start;
				}
			}

			log_printf( "    Duration: %.4f%s\n\n", duration, duration_invalid ? " [INVALID]" : "" );

			// print input videos for this preset and their time ranges
			for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
			{
				clip_input_video_t& input = output.input[ in_i ];

				// validate preset
				if ( !used_in_preset( input.encode_settings, preset_i ) )
					continue;

				log_printf( "    %s%s\n", input.path, input.file_missing ? " [INVALID]" : "" );

				if ( input.file_missing )
					continue;

				for ( u32 time_i = 0; time_i < input.time_range_count; time_i++ )
				{
					bool  valid          = valid_time_range( input.time_range[ time_i ], input.metadata );
					float range_duration = input.time_range[ time_i ].end - input.time_range[ time_i ].start;

					log_printf( "        %.4f - %.4f (%.4f)%s\n", input.time_range[ time_i ].start, input.time_range[ time_i ].end, range_duration, valid ? "" : " [INVALID]" );
				}
			}
		}

		output.state = e_output_state_wait;
		log_printf( "----------------------------------------------------\n" );
	}

	return true;
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

	if ( g_clip_data->output_count == 0 )
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

