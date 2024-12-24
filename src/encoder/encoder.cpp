#include "encoder.h"
#include "clip/clip.h"
#include "util.h"

#include <time.h>

constexpr int FFMPEG_CMD_SIZE = 1024;


char* gen_ffmpeg_cmd( clip_encode_preset_t& preset, clip_input_video_t& input, u32 time_range_i )
{
	static char ffmpeg_cmd[ FFMPEG_CMD_SIZE ] = { 0 };
	memset( ffmpeg_cmd, 0, FFMPEG_CMD_SIZE * sizeof( char ) );

	strcat( ffmpeg_cmd, "ffmpeg -y -hide_banner " );

	clip_time_range_t& time_range = input.time_range[ time_range_i ];

	if ( time_range.start > 0.f )
	{
		strcat( ffmpeg_cmd, "-ss " );
		size_t offset = strlen( ffmpeg_cmd );
		snprintf( ffmpeg_cmd + offset, FFMPEG_CMD_SIZE - offset, "%.5f ", time_range.start );
	}

	if ( time_range.end > 0.f )
	{
		strcat( ffmpeg_cmd, "-to " );
		size_t offset = strlen( ffmpeg_cmd );
		snprintf( ffmpeg_cmd + offset, FFMPEG_CMD_SIZE - offset, "%.5f ", time_range.end );
	}

	strcat( ffmpeg_cmd, "-i \"" );
	strcat( ffmpeg_cmd, input.path );
	strcat( ffmpeg_cmd, "\" " );

	strcat( ffmpeg_cmd, preset.ffmpeg_cmd );

	return ffmpeg_cmd;
}


bool run_ffmpeg_check( const char* cmd, const char* path )
{
	printf( "\nFFMPEG CMD: %s\n\n", cmd );

	int ret = sys_execute( cmd );

	if ( ret != 0 )
	{
		printf( "ffmpeg returned with error %d\n", ret );
		return false;
	}

	if ( fs_is_file( path ) )
	{
		u64 size = fs_file_size( path );

		if ( size == 0 )
		{
			printf( "ffmpeg created an empty video - \"%s\"\n", path );
			return false;
		}
	}
	else
	{
		printf( "ffmpeg did not create a video - \"%s\"\n", path );
		return false;
	}

	return true;
}


bool only_uses_encode_preset( clip_encode_override_t& override, u32 preset_i )
{
	bool valid_preset = override.presets_count == 0;

	for ( u32 i = 0; i < override.presets_count; i++ )
	{
		if ( override.presets[ i ] == preset_i )
		{
			if ( override.preset_exclude )
				valid_preset = false;
			else
				return true;
				// valid_preset = true;
		}
	}

	return valid_preset;
}


bool uses_encode_preset( clip_encode_override_t& override, u32 preset_i )
{
	bool valid_preset = override.presets_count == 0;

	for ( u32 i = 0; i < override.presets_count; i++ )
	{
		if ( override.presets[ i ] == preset_i )
		{
			if ( override.preset_exclude )
				valid_preset = false;
			else
				return true;
				// valid_preset = true;
		}
	}

	return valid_preset;
}


u32 get_used_encode_preset_index( clip_encode_override_t& override, u32 preset_i )
{
	for ( u32 i = 0; i < override.presets_count; i++ )
	{
		if ( override.presets[ i ] == preset_i )
		{
			if ( override.preset_exclude )
				return UINT32_MAX;
			else
				return i;
		}
	}

	return UINT32_MAX;
}


void add_metadata_cmd( clip_output_video_t& output, char* ffmpeg_cmd, bool add_markers, u32 preset_i )
{
	// write ffmpeg metadata.txt file
	char metadata_path[ 256 ] = { 0 };
	strcat( metadata_path, g_temp_video_dir );
	strcat( metadata_path, PATH_SEP_STR );
	strcat( metadata_path, output.name );
	strcat( metadata_path, "__metadata.txt" );

	char metadata_file[ 2048 ] = { 0 };
	strcat( metadata_file, ";FFMETADATA1\n" );

	char* time_file_path = nullptr;

	for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
	{
		clip_input_video_t& input = output.input[ in_i ];

		// don't use this one if this file is from this preset
		if ( input.encode_overrides.presets_count <= 1 && uses_encode_preset( input.encode_overrides, preset_i ) )
			continue;

		if ( !time_file_path )
			time_file_path = input.path;

		// take all inputs videos that don't use the current encode preset, and use their time ranges as markers for this one
		// a bit strange, but it makes sense for the raw encodes only lol
		if ( !add_markers )
			break;

		// look for a matching input video first

#if 0
		clip_input_video_t* src_input   = nullptr;
		u32                 time_offset = 0;
		u32                 time_end    = 0;

		for ( u32 src_i = 0; src_i < output.input_count; src_i++ )
		{
			src_input = &output.input[ src_i ];

			// this video doesn't use this encode preset
			if ( !uses_encode_preset( src_input->encode_overrides, preset_i ) )
				continue;

			for ( u32 time_i = 0; time_i < input.time_range_count; time_i++ )
			{
			}

			// make sure this is the same path
			if ( strcmp( src_input->path, input.path ) != 0 )
			{
				time_offset = time_end;
				continue;
			}

			break;
		}
#endif

		u32 marker_i = 0;
		for ( u32 time_i = 0; time_i < input.time_range_count; time_i++ )
		{
			clip_time_range_t&  time_range  = input.time_range[ time_i ];

			clip_input_video_t* src_input   = nullptr;
			float               time_offset = 0.f;
			float               time_end               = 0.f;

			//u32                 dst_input_preset_index = get_used_encode_preset_index( input.encode_overrides, preset_i );
			//
			//if ( dst_input_preset_index == UINT32_MAX )
			//	continue;

			for ( u32 src_i = 0; src_i < output.input_count; src_i++ )
			{
				src_input = &output.input[ src_i ];

				// this video doesn't use this encode preset
				// if ( !uses_encode_preset( src_input->encode_overrides, preset_i ) )
				// 	continue;

				u32 src_input_preset_index = get_used_encode_preset_index( src_input->encode_overrides, preset_i );
				
				if ( src_input_preset_index == UINT32_MAX )
					continue;

				bool skip_time = false;
				for ( u32 src_time_i = 0; src_time_i < src_input->time_range_count; src_time_i++ )
				{
					clip_time_range_t& src_time_range = src_input->time_range[ src_time_i ];

					// if this raw time range starts later than the discord time range, probably don't use this
					if ( src_time_range.start > time_range.start )
					{
						skip_time = true;
						break;
					}
					
					// is the raw time range before the discord time range?
					if ( src_time_range.start <= time_range.start )
					{
						time_offset = src_time_range.start;
						time_end    = src_time_range.end;
					}
				}
				
				// stop offsetting the start time
				if ( skip_time )
					break;

				// make sure this is the same path
				if ( strcmp( src_input->path, input.path ) != 0 )
				{
					// skip to the next one
					time_offset = time_end;
					continue;
				}

				break;
			}

			float  start_time  = ( time_range.start - time_offset ) * 1000;
			float  end_time    = ( time_range.end - time_offset ) * 1000;

			char*  path_unix   = fs_replace_path_seps_unix( input.path );
			char*  preset_name = g_clip_data->preset[ preset_i ].name;

			// TODO: this probably breaks on videos with more than one input
			// i think we need to offset the start/end times with the raw input video start time? idfk
			size_t str_offset = strlen( metadata_file );
			snprintf(
			  metadata_file + str_offset, 2048 - str_offset,
			  "[CHAPTER]\nTIMEBASE=1/1000\nSTART=%.6f\nEND=%.6f\ntitle='%d - %s - %s'\n\n"
			  "[CHAPTER]\nTIMEBASE=1/1000\nSTART=%.6f\nEND=%.6f\ntitle='%d - %s - %s'\n\n",
			  start_time, start_time, marker_i, path_unix ? path_unix : input.path, preset_name,
			  end_time, end_time, marker_i + 1, path_unix ? path_unix : input.path, preset_name
			);

			marker_i += 2;

			free( path_unix );
		}
	}

	if ( !fs_write_file( metadata_path, metadata_file, strlen( metadata_file ) ) )
	{
		printf( "failed to write metadata file - \"%s\"\n", metadata_path );
		return;
	}

	time_t cur_time = time( nullptr );

	char str_time_encode[ 64 ]{ 0 };
	char str_time_modified[ 64 ]{ 0 };
	char str_time_created[ 64 ]{ 0 };

	struct tm* tm_info;

	tm_info = localtime( &cur_time );
	strftime( str_time_encode, 26, "%Y-%m-%d %H-%M-%S", tm_info );

	u64 creation = 0, modified = 0;
	sys_get_file_times( time_file_path, &creation, nullptr, &modified );

	time_t time_creation = (time_t)creation;
	time_t time_modified = (time_t)modified;

	tm_info              = localtime( &time_creation );
	strftime( str_time_created, 26, "%Y-%m-%d %H-%M-%S", tm_info );

	tm_info = localtime( &time_modified );
	strftime( str_time_modified, 26, "%Y-%m-%d %H-%M-%S", tm_info );

	size_t offset = strlen( ffmpeg_cmd );
	snprintf(
	  ffmpeg_cmd + offset, FFMPEG_CMD_SIZE - offset,
	  "-i \"%s\" -map_metadata 0 -map_metadata 1 -metadata demez_date_encoded=\"%s\" -metadata demez_date_modified=\"%s\" -metadata demez_date_created=\"%s\" "
	  "-metadata demez_date_encoded_u=%lld -metadata demez_date_modified_u=%lld -metadata demez_date_created_u=%lld ",
	  metadata_path, str_time_encode, str_time_modified, str_time_created, (u64)cur_time, modified, creation );
}


void create_output_video( clip_output_video_t& output, char* full_out_path, enc_video_data_t& video_data, bool add_markers, u32 preset_i )
{
	// write ffmpeg concat.txt file
	FILE* fp = fopen( "concat.txt", "wb" );

	if ( fp == nullptr )
	{
		printf( "failed to open file handle to write concat.txt\n" );
		return;
	}

	for ( u32 i = 0; i < video_data.segment_count; i++ )
	{
		fwrite( "file '", 6, 1, fp );
		fwrite( video_data.segment[ i ].path, strlen( video_data.segment[ i ].path ), 1, fp );
		fwrite( "'\n", 2, 1, fp );
	}

	fclose( fp );

	char ffmpeg_cmd[ FFMPEG_CMD_SIZE ] = { 0 };
	strcat( ffmpeg_cmd, "ffmpeg -y -hide_banner -safe 0 -f concat -i concat.txt " );
	add_metadata_cmd( output, ffmpeg_cmd, add_markers, preset_i );
	strcat( ffmpeg_cmd, " -c copy -map 0 \"" );
	strcat( ffmpeg_cmd, full_out_path );
	strcat( ffmpeg_cmd, "\"" );

	printf( "command line: %s\n\n", ffmpeg_cmd );

	if ( !run_ffmpeg_check( ffmpeg_cmd, full_out_path ) )
		return;

	// TODO: delete concat.txt
	// TODO: write date created and modified
	// always just use the date created and modified from input video 0, unless we add something to override that later
}


float calc_bitrate_from_bits_per_pixel( float bpp, float width, float height, float fps )
{
	return ( width * height * fps * bpp ) / 1000;
}


float calc_bits_per_pixel( float rate, float width, float height, float fps )
{
	// data rate / (resolution * frames per second) = BPP
	return rate / ( width * height * fps );
}


void calc_target_bitrates( enc_video_data_t& video_data, clip_encode_preset_t& preset )
{
	float total_duration = 0.f;
	float total_bitrate  = 0.f;

	for ( u32 seg_i = 0; seg_i < video_data.segment_count; seg_i++ )
	{
		video_segment_t&    segment    = video_data.segment[ seg_i ];
		clip_input_video_t& input      = video_data.output->input[ segment.input ];
		clip_time_range_t&  time_range = input.time_range[ segment.time ];

		float               duration   = time_range.end - time_range.start;

		// idk why we multiply by 8, was done in the python version, something with "KB to MB to Kbit"?
		float               bitrate    = ( preset.target_size / duration ) * 8;

		// subtract audio bitrate
		bitrate -= preset.audio_bitrate;

		segment.bitrate = bitrate;

		total_duration += duration;
		total_bitrate += bitrate;
	}

	float average_bitrate = ( ( preset.target_size * 0.001 * 8192 ) / total_duration ) - preset.audio_bitrate;
	float bitrate_mult    = average_bitrate / total_bitrate;

	for ( u32 seg_i = 0; seg_i < video_data.segment_count; seg_i++ )
	{
		video_segment_t& segment = video_data.segment[ seg_i ];
		segment.bitrate *= bitrate_mult;
	}
}


struct target_size_pass_t
{
	u32                 attempt         = 0;

	float*              prev_bitrates   = nullptr;  // unused
	float*              max_bitrates    = nullptr;
	float*              min_bitrates    = nullptr;
	float*              ffmpeg_bitrates = nullptr;

	float               prev_file_size  = 0;

	e_target_size_state last_state      = e_target_size_state_none;
};


// returns -1 if we failed and should cancel this, 0 if we need to try again, 1 if we suceeded
int run_encode_inputs_target_size_pass( enc_video_data_t& video_data, clip_encode_preset_t& preset, target_size_pass_t& pass_data )
{
	for ( u32 seg_i = 0; seg_i < video_data.segment_count; seg_i++ )
	{
		video_segment_t&    segment    = video_data.segment[ seg_i ];
		clip_input_video_t& input      = video_data.output->input[ segment.input ];
		clip_time_range_t&  time_range = input.time_range[ segment.time ];

		// for raw encodes only right now, need to setup discord stuff later
		char*               ffmpeg_cmd = gen_ffmpeg_cmd( preset, input, segment.time );
		size_t              buf_len    = strlen( ffmpeg_cmd );

		snprintf( ffmpeg_cmd + buf_len, FFMPEG_CMD_SIZE - buf_len, " -b:v %.4fk", segment.bitrate );

		// TEST
		// snprintf( ffmpeg_cmd + buf_len, FFMPEG_CMD_SIZE - buf_len, " -b:v %.4fk -maxrate %.4fk -bufsize %.4fk -undershoot-pct 100 -overshoot-pct 100", segment.bitrate, segment.bitrate, segment.bitrate / 2.f );
		buf_len = strlen( ffmpeg_cmd );

		if ( preset.audio_bitrate )
		{
			snprintf( ffmpeg_cmd + buf_len, FFMPEG_CMD_SIZE - buf_len, " -b:a %dk", preset.audio_bitrate );
			buf_len = strlen( ffmpeg_cmd );
		}

		snprintf( ffmpeg_cmd + buf_len, FFMPEG_CMD_SIZE - buf_len, " \"%s\"\0", segment.path );

		if ( !run_ffmpeg_check( ffmpeg_cmd, segment.path ) )
			return -1;
	}

	// check the segments created

	// get the bitrates and total file size from the videos ffmpeg created
	float total_file_size = 0;
	for ( u32 seg_i = 0; seg_i < video_data.segment_count; seg_i++ )
	{
		video_segment_t& segment           = video_data.segment[ seg_i ];
		float            bitrate           = get_video_bitrate( segment.path ) * 0.001;  // bytes to kb
		pass_data.ffmpeg_bitrates[ seg_i ] = bitrate;

		// bytes to kb
		total_file_size += (fs_file_size( segment.path ) * 0.001);
	}

	// is this the same file size as before?
	if ( pass_data.prev_file_size == total_file_size )
	{
		printf( "Attempt %d: the video is the exact same file size as the previous attempt, cancelling\n", pass_data.attempt + 1 );
		return -1;
	}

	pass_data.prev_file_size = total_file_size;

	// is this within the target size range?
	if ( total_file_size < preset.target_size_max && total_file_size > preset.target_size_min )
		return 1;

	// no it isn't, find new bitrates for each segment and try again
	bool smaller = total_file_size < preset.target_size_min;
	e_target_size_state new_state = smaller ? e_target_size_state_smaller : e_target_size_state_bigger;

	if ( smaller )
		printf( "Attempt %d: Output video is smaller than target file size\n", pass_data.attempt + 1 );
	else
		printf( "Attempt %d: Output video is larger than target file size\n", pass_data.attempt + 1 );

	bool state_changed = false;

	if ( smaller && pass_data.last_state == e_target_size_state_bigger )
	{
		state_changed = true;
		printf( "  cool we managed to go from being too big to being too small, god ffmpeg why\n" );
	}
	else if ( !smaller && pass_data.last_state == e_target_size_state_smaller )
	{
		state_changed = true;
		printf( "  cool we managed to go from being too small to being too big, god ffmpeg why\n" );
	}

	// reencode videos at an adjusted bitrate based on what ffmpeg felt like encoding it as
	for ( u32 seg_i = 0; seg_i < video_data.segment_count; seg_i++ )
	{
		video_segment_t& segment        = video_data.segment[ seg_i ];
		float            ffmpeg_bitrate = pass_data.ffmpeg_bitrates[ seg_i ];

		// division
		float            bitrate_base    = 0.f;  // numerator
		float            bitrate_div    = 0.f;  // denominator

		if ( smaller )
		{
			// if the video is smaller than the min file size, and the bitrate we tried this pass is higher than a previous min bitrate
			// then set that as the new min bitrate, so we know not to go lower than that
			// pass_data.min_bitrates[ seg_i ] = MIN()
			if ( pass_data.min_bitrates[ seg_i ] < segment.bitrate )
				pass_data.min_bitrates[ seg_i ] = segment.bitrate;

			bitrate_div  = MIN( ffmpeg_bitrate, segment.bitrate );
			bitrate_base = MAX( ffmpeg_bitrate, segment.bitrate );
		}
		else
		{
			// if the video is larger than the max file size, and the bitrate we tried this pass is lower than a previous max bitrate
			// then set that as the new max bitrate, so we know not to go higher than that
			if ( pass_data.max_bitrates[ seg_i ] > segment.bitrate )
				pass_data.max_bitrates[ seg_i ] = segment.bitrate;

			bitrate_div  = MAX( ffmpeg_bitrate, segment.bitrate );
			bitrate_base = MIN( ffmpeg_bitrate, segment.bitrate );
		}

		if ( state_changed )
		{
			segment.bitrate = ( pass_data.max_bitrates[ seg_i ] + pass_data.min_bitrates[ seg_i ] ) / 2.f;

			printf( "  SEGMENT %d: Trying new target bitrate of %.4f\n", seg_i, segment.bitrate );
			printf( "  SEGMENT %d: Current Min/Max Bitrates: %.4f/%.4f\n", seg_i, pass_data.min_bitrates[ seg_i ], pass_data.max_bitrates[ seg_i ] );
		}
		else
		{
			// could be something like 0.00017052145616207467, so wtf
			float bitrate_mult = bitrate_base / bitrate_div;

			if ( bitrate_mult < 0.2 )
			{
				printf( "  SEGMENT %d: wait wtf we just calculated a bitrate multiplier below 0.2, clamping to 0.2\n", seg_i );
				bitrate_mult = 0.2;
			}

			float old_bitrate = segment.bitrate;
			segment.bitrate *= bitrate_mult;

			if ( bitrate_mult > 4 )
			{
				printf( "  SEGMENT %d: Bitrate Multiplier is greater than 3, fuck this, it's probably fine\n", seg_i );
				return -1;
			}

			else if ( segment.bitrate <= 0.01 )
			{
				printf( "  SEGMENT %d: wtf we just calculated a bitrate of 0.01 or below, screw this\n", seg_i );
				return -1;
			}

			printf(
			  "  SEGMENT %d: Trying new target bitrate of %.4f\n"
			  "    %.4f * %.4f  ->  %.4f * (%.4f / %.4f)\n",
			  seg_i, segment.bitrate,
			  old_bitrate, bitrate_mult,
			  old_bitrate, bitrate_base, bitrate_div );
		}
	}

	pass_data.last_state = new_state;

	// try again
	return 0;
}


// encoding for a target file size, useful for creating discord videos
bool run_encode_inputs_target_size( enc_video_data_t& video_data, clip_encode_preset_t& preset )
{
	// calculate target bitrates
	calc_target_bitrates( video_data, preset );

	target_size_pass_t pass_data{};
	pass_data.prev_bitrates   = ch_calloc< float >( video_data.segment_count );
	pass_data.max_bitrates    = ch_calloc< float >( video_data.segment_count );
	pass_data.min_bitrates    = ch_calloc< float >( video_data.segment_count );
	pass_data.ffmpeg_bitrates = ch_calloc< float >( video_data.segment_count );

	if ( !pass_data.prev_bitrates || !pass_data.max_bitrates || !pass_data.min_bitrates || !pass_data.ffmpeg_bitrates )
		return false;

	// set max bitrate really high by default so it lowers in passes
	// either min or max bitrates should get closer to each other with each pass
	for ( u32 seg_i = 0; seg_i < video_data.segment_count; seg_i++ )
		pass_data.max_bitrates[ seg_i ] = FLT_MAX;

	int ret = 0;

	// max of 10 attempts
	for ( ; pass_data.attempt < 10; pass_data.attempt++ )
	{
		ret = run_encode_inputs_target_size_pass( video_data, preset, pass_data );

		if ( ret != 0 )
			break;
	}

	free( pass_data.prev_bitrates );
	free( pass_data.max_bitrates );
	free( pass_data.min_bitrates );
	free( pass_data.ffmpeg_bitrates );

	return ret == 1;
}


// standard encode
#if 1
bool run_encode_inputs_standard( enc_video_data_t& video_data, clip_encode_preset_t& preset )
{
	// create all video segments
	for ( u32 seg_i = 0; seg_i < video_data.segment_count; seg_i++ )
	{
		video_segment_t&    segment    = video_data.segment[ seg_i ];
		clip_input_video_t& input      = video_data.output->input[ segment.input ];
		clip_time_range_t&  time_range = input.time_range[ segment.time ];

		// for raw encodes only right now, need to setup discord stuff later
		char*  ffmpeg_cmd       = gen_ffmpeg_cmd( preset, input, segment.time );
		size_t buf_len          = strlen( ffmpeg_cmd );

		snprintf( ffmpeg_cmd + buf_len, FFMPEG_CMD_SIZE - buf_len, " \"%s\"\0", segment.path );

		if ( !run_ffmpeg_check( ffmpeg_cmd, segment.path ) )
			return false;
	}

	return true;
}
#else
bool run_encode_inputs_standard( clip_output_video_t& output, char**& segment_paths, u32& segment_i, clip_encode_preset_t& preset, u32 preset_i )
{
	// create all video segments
	bool failed = false;

	for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
	{
		clip_input_video_t& input        = output.input[ in_i ];

		// verify the encode preset
		bool                valid_preset = input.encode_overrides.presets_count == 0;

		for ( u32 i = 0; i < input.encode_overrides.presets_count; i++ )
		{
			if ( input.encode_overrides.presets[ i ] == preset_i )
			{
				if ( input.encode_overrides.preset_exclude )
					valid_preset = false;
				else
					valid_preset = true;

				break;
			}
		}

		// don't use this one
		if ( !valid_preset )
			continue;

		char* input_name = fs_get_filename_no_ext( input.path );

		for ( u32 time_i = 0; time_i < input.time_range_count; time_i++ )
		{
			// add it to the segment list
			if ( array_append_err( segment_paths, segment_i, "failed to allocate memory to store video segment path\n" ) )
			{
				failed = true;
				break;
			}

			// for raw encodes only right now, need to setup discord stuff later
			char*  ffmpeg_cmd       = gen_ffmpeg_cmd( preset, input, time_i );
			size_t buf_len          = strlen( ffmpeg_cmd );
			char   temp_name[ 256 ] = { 0 };

			snprintf( temp_name, 256, "%s/%d__%s.%s", g_temp_video_dir, segment_i, input_name, preset.ext );
			segment_paths[ segment_i ] = strdup( temp_name );
			segment_i++;

			snprintf( ffmpeg_cmd + buf_len, FFMPEG_CMD_SIZE - buf_len, " \"%s\"\0", temp_name );

			if ( !run_ffmpeg_check( ffmpeg_cmd, temp_name ) )
			{
				failed = true;
				break;
			}
		}

		free( input_name );

		if ( failed )
			return false;
	}

	return true;
}
#endif


enc_video_data_t get_video_segments( enc_output_video_t& enc_output, clip_output_video_t& output, u32 preset_i )
{
	enc_video_data_t video_data{};
	video_data.enc_output        = &enc_output;
	video_data.output            = &output;

	clip_encode_preset_t& preset = g_clip_data->preset[ preset_i ];

	bool                  failed = false;

	for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
	{
		clip_input_video_t& input        = output.input[ in_i ];

		// verify the encode preset
#if 1
		if ( !uses_encode_preset( input.encode_overrides, preset_i ) )
			continue;
#else
		bool                valid_preset = input.encode_overrides.presets_count == 0;

		for ( u32 i = 0; i < input.encode_overrides.presets_count; i++ )
		{
			if ( input.encode_overrides.presets[ i ] == preset_i )
			{
				if ( input.encode_overrides.preset_exclude )
					valid_preset = false;
				else
					valid_preset = true;

				break;
			}
		}

		// don't use this one
		if ( !valid_preset )
			continue;
#endif

		char* input_name = fs_get_filename_no_ext( input.path );

		for ( u32 time_i = 0; time_i < input.time_range_count; time_i++ )
		{
			// add it to the segment list
			if ( array_append_err( video_data.segment, video_data.segment_count, "failed to allocate memory to store video segment path\n" ) )
			{
				failed = true;
				break;
			}

			char temp_name[ 256 ] = { 0 };

			snprintf( temp_name, 256, "%s/%d__%s.%s", g_temp_video_dir, video_data.segment_count, input_name, preset.ext );
			video_data.segment[ video_data.segment_count ].path  = strdup( temp_name );
			video_data.segment[ video_data.segment_count ].input = in_i;
			video_data.segment[ video_data.segment_count ].time  = time_i;
			video_data.segment_count++;
		}

		free( input_name );

		if ( failed )
		{
			free( video_data.segment );
			video_data.segment_count = 0;
			return video_data;
		}
	}

	return video_data;
}


void run_encode_preset( char* out_dir, clip_encode_preset_t& preset, u32 preset_i )
{
	printf( "Encoding for %s preset\n", preset.name );
	char full_out_path[ 4096 ] = { 0 };

	for ( u32 out_i = 0; out_i < g_clip_data->output_count; out_i++ )
	{
		clip_output_video_t& output     = g_clip_data->output[ out_i ];
		clip_prefix_t&       prefix     = g_clip_data->prefix[ output.prefix ];
		enc_output_video_t&  enc_output = g_output_videos[ out_i ];

		if ( !enc_output.valid )
		{
			printf( "skipping invalid video: \"%s\"\n", output.name );
			continue;
		}

		// check if this video is used on this preset
		bool valid_preset = !enc_output.presets_count;
		for ( u32 i = 0; i < enc_output.presets_count; i++ )
		{
			if ( enc_output.presets[ i ] != preset_i )
				continue;

			valid_preset = true;
			break;
		}

		if ( !valid_preset )
			continue;

		printf( "\n----------------------------------------------------\n\n" );

		memset( full_out_path, 0, sizeof( char ) * 4096 );
		strcat( full_out_path, out_dir );

		if ( preset.out_prefix )
			strcat( full_out_path, preset.out_prefix );

		strcat( full_out_path, prefix.prefix );
		strcat( full_out_path, output.name );
		strcat( full_out_path, "." );
		strcat( full_out_path, preset.ext );

		printf( "Output Video \"%s\"\n", full_out_path );

		// check if the video exists
		if ( fs_is_file( full_out_path ) )
		{
			// make sure it's not an empty file
			if ( fs_file_size( full_out_path ) > 0 )
			{
				printf( "Output File exists - skipping\n" );
				continue;
			}
		}

		// ----------------------------------------------------------------------------
		// get the list of input videos and time ranges we will use for this preset
		enc_video_data_t video_data = get_video_segments( enc_output, output, preset_i );

		if ( video_data.segment_count == 0 )
			continue;
		
		bool skip_output = false;

		// create all video segments
		if ( preset.target_size )
			skip_output = !run_encode_inputs_target_size( video_data, preset );
		
		else
			skip_output = !run_encode_inputs_standard( video_data, preset );

		// if we want to skip this or we just have no video segments
		if ( !skip_output )
		{
			// concat them together
			create_output_video( output, full_out_path, video_data, !preset.target_size, preset_i );
		}

		// free data
		for ( u32 i = 0; i < video_data.segment_count; i++ )
			free( video_data.segment[ i ].path );

		free( video_data.segment );
	}
}


void run_encoding()
{
	char* out_dir = ch_calloc< char >( 4096 );

	if ( !out_dir )
		return;

	for ( u32 preset_i = 0; preset_i < g_clip_data->preset_count; preset_i++ )
	{
		clip_encode_preset_t& preset = g_clip_data->preset[ preset_i ];

		memset( out_dir, 0, sizeof( char ) * 4096 );
		strcat( out_dir, g_output_dir );

		if ( preset.out_folder_append )
		{
			strcat( out_dir, PATH_SEP_STR );
			strcat( out_dir, preset.out_folder_append );

			if ( !fs_make_dir_check( out_dir ) )
				continue;
		}

		strcat( out_dir, PATH_SEP_STR );
		run_encode_preset( out_dir, preset, preset_i );
	}

	free( out_dir );
}

