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


static bool         parse_ffprobe_int( bool& failed, char*& line_start, const char* name, size_t name_len, int& out_value )
{
	if ( !util_strncmp( name, line_start, name_len ) )
		return false;

	char* end;
	long  value = strtol( line_start + name_len, &end, 10 );

	if ( end == line_start + name_len )
	{
		printf( "failed to parse int video info - \"%s\"\n", name );
		failed = true;
		return false;
	}

	out_value  = value;
	line_start = end;

	return true;
}


static bool parse_ffprobe_float( bool& failed, char*& line_start, const char* name, size_t name_len, float& out_value )
{
	if ( !util_strncmp( name, line_start, name_len ) )
		return false;

	char* end;
	out_value = strtof( line_start + name_len, &end );

	if ( end == line_start + name_len )
	{
		printf( "failed to parse float video info - \"%s\"\n", name );
		failed = true;
		return false;
	}

	line_start = end;
	return true;
}


//static char* parse_ffprobe_str( char*& line_start, char* line_end, const char* name, size_t name_len )
//{
//	if ( !util_strncmp( name, line_start, name_len ) )
//		return nullptr;
//
//	size_t value_len =
//
//	char* end;
//	out_value = strtof( line_start + name_len, &end );
//
//	if ( end != line_start + name_len )
//	{
//		printf( "failed to parse float video info - \"%s\"\n", name );
//		return false;
//	}
//
//	line_start = end;
//	return true;
//}


bool get_video_metadata( const char* path, video_metadata_t& metadata )
{
	// build command line
	char cmd[ 512 ] = { 0 };
	// strcat( cmd, "ffprobe.exe -threads 6 -v error -show_streams -select_streams v:0 -show_format -of default=noprint_wrappers=1 \"" );
	strcat( cmd, "ffprobe.exe -threads 6 -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate:format=bit_rate,duration,height,r_frame_rate -of default=noprint_wrappers=1 \"" );
	strcat( cmd, path );
	strcat( cmd, "\"" );

	str_buf_t output{};
	bool      success = sys_execute_read( cmd, output );

	if ( !success )
		return false;

	if ( output.data[ 0 ] == '\0' )
		return false;

	// parse it
	char* line_end   = strchr( output.data, '\n' );
	char* line_start = output.data;

	while ( line_end )
	{
		// go back one because of the carriage return on windows
		char*  real_line_end = line_end--;
		size_t line_len      = line_end - line_start;

		bool   failed        = false;

		// uh
		if ( parse_ffprobe_int( failed, line_start, "width=", 6, metadata.width ) ) {}
		else if ( parse_ffprobe_int( failed, line_start, "height=", 7, metadata.height ) ) {}
		else if ( parse_ffprobe_float( failed, line_start, "duration=", 9, metadata.duration ) ) {}
		else if ( parse_ffprobe_float( failed, line_start, "bit_rate=", 9, metadata.bitrate ) ) {}
		else if ( parse_ffprobe_float( failed, line_start, "r_frame_rate=", 13, metadata.fps_num ) )
		{
			// get the denominator
			char* end;
			char* parse_start = line_start + 1;
			metadata.fps_den  = strtof( parse_start, &end );

			if ( end == parse_start )
			{
				failed = true;
				printf( "failed to parse r_frame_rate denominator\n" );
			}

			line_start   = end;

			metadata.fps = metadata.fps_num / metadata.fps_den;
		}

		if ( failed )
		{
			free( output.data );
			return false;
		}

		// prepare next line, go over 2 because we went back from the carriage return
		line_start = line_end + 2;

		if ( line_start[ 0 ] == '\0' )
			break;

		line_end = strchr( line_start, '\n' );
	}

	free( output.data );

	return true;
}


float get_video_bitrate( const char* path )
{
	// build command line
	char cmd[ 512 ] = { 0 };
	strcat( cmd, "ffprobe.exe -threads 6 -v error -select_streams v:0 -show_entries format=bit_rate -of default=noprint_wrappers=1:nokey=1 \"" );
	strcat( cmd, path );
	strcat( cmd, "\"" );

	str_buf_t output{};
	bool      success = sys_execute_read( cmd, output );

	if ( !success )
		return 0.f;

	if ( output.data[ 0 ] == '\0' )
		return false;

	char* end;
	float bitrate = strtof( output.data, &end );

	if ( end == output.data )
	{
		printf( "failed to parse bitrate - \"%s\"\n", path );
		return 0.f;
	}

	return bitrate;
}


void print_video_info()
{
}


// makes sure the time range desired is valid for this input video
bool valid_time_range( clip_time_range_t& range, video_metadata_t& metadata )
{
	if ( range.start > range.end )
		return false;

	if ( range.start > metadata.duration )
		return false;

	float duration = range.end - range.start;

	if ( duration > metadata.duration )
		return false;

	return true;
}


bool used_in_preset( clip_encode_override_t& override, u32 preset_i )
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
		enc_output.metadata             = ch_calloc< video_metadata_t >( output.input_count );

		g_encoder_data.scan_index       = out_i;

		if ( !enc_output.metadata )
		{
			log_printf( "failed to allocate memory for storing video metadata\n" );
			return false;
		}

		log_printf( "%s%s\n", g_clip_data->prefix[ output.prefix ].prefix, output.name );

		// ----------------------------------------------------------------------------------------
		// determine encode presets for this output video

		// figure out what encode presets this runs on
		for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
		{
			clip_input_video_t& input = output.input[ in_i ];

			for ( u32 preset_i = 0; preset_i < input.encode_overrides.presets_count; preset_i++ )
			{
				bool preset_already_added = false;
				for ( u32 search_i = 0; search_i < enc_output.presets_count; search_i++ )
				{
					if ( enc_output.presets[ search_i ] == input.encode_overrides.presets[ preset_i ] )
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
				enc_output.presets[ enc_output.presets_count++ ] = input.encode_overrides.presets[ preset_i ];
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
		enc_output.missing_inputs = ch_calloc< bool >( output.input_count );

		for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
		{
			clip_input_video_t& input = output.input[ in_i ];

			// did we get metadata for this video already?
			if ( in_i > 0 )
			{
				u32 metadata_i = 0;
				for ( ; metadata_i < output.input_count; metadata_i++ )
				{
					if ( metadata_i == in_i )
						continue;

					// check for a matching video path
					if ( strcmp( input.path, output.input[ metadata_i ].path ) == 0 )
						break;
				}

				if ( metadata_i < output.input_count )
				{
					// copy it
					enc_output.metadata[ in_i ] = enc_output.metadata[ metadata_i ];
					continue;
				}
			}

			// we have not encountered this video yet, so check if it actually exists, and then parse it
			if ( !fs_exists( input.path ) )
			{
				all_valid                         = false;
				enc_output.missing_inputs[ in_i ] = true;
				break;
			}

			if ( !get_video_metadata( input.path, enc_output.metadata[ in_i ] ) )
			{
				log_printf( "Failed to get video metadata - \"%s\"", input.path );
				all_valid                         = false;
				enc_output.missing_inputs[ in_i ] = true;
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

				if ( !used_in_preset( input.encode_overrides, preset_i ) )
					continue;

				for ( u32 time_i = 0; time_i < input.time_range_count; time_i++ )
				{
					if ( !valid_time_range( input.time_range[ time_i ], enc_output.metadata[ in_i ] ) )
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
				if ( !used_in_preset( input.encode_overrides, preset_i ) )
					continue;

				log_printf( "    %s%s\n", input.path, enc_output.missing_inputs[ in_i ] ? " [INVALID]" : "" );

				if ( enc_output.missing_inputs[ in_i ] )
					continue;

				for ( u32 time_i = 0; time_i < input.time_range_count; time_i++ )
				{
					bool  valid          = valid_time_range( input.time_range[ time_i ], enc_output.metadata[ in_i ] );
					float range_duration = input.time_range[ time_i ].end - input.time_range[ time_i ].start;

					log_printf( "        %.4f - %.4f (%.4f)%s\n", input.time_range[ time_i ].start, input.time_range[ time_i ].end, range_duration, valid ? "" : " [INVALID]" );
				}
			}
		}

		enc_output.valid = true;
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

