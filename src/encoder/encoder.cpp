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

		// check presets
		bool                found = false;
		for ( u32 i = 0; i < input.encode_overrides.presets_count; i++ )
		{
			if ( input.encode_overrides.presets[ i ] == preset_i )
			{
				found = true;
				break;
			}
		}

		// don't use this one
		if ( found )
			continue;

		if ( !time_file_path )
			time_file_path = input.path;

		// take all inputs videos that don't use the current encode preset, and use their time ranges as markers for this one
		// a bit strange, but it makes sense for the raw encodes only lol
		if ( !add_markers )
			break;

		u32 marker_i = 0;
		for ( u32 time_i = 0; time_i < input.time_range_count; time_i++ )
		{
			clip_time_range_t& time_range = input.time_range[ time_i ];

			// TODO: this probably breaks on videos with more than one input
			// i think we need to offset the start/end times with the raw input video start time? idfk
			size_t offset = strlen( metadata_file );
			snprintf(
			  metadata_file + offset, 2048 - offset,
			  "[CHAPTER]\nTIMEBASE=1/1000\nSTART=%.6f\nEND=%.6f\ntitle='%d__%s'\n\n",
			  "[CHAPTER]\nTIMEBASE=1/1000\nSTART=%.6f\nEND=%.6f\ntitle='%d__%s'\n\n",
			  time_range.start * 1000, time_range.start * 1000, marker_i++, input.path,
			  time_range.end * 1000, time_range.end * 1000, marker_i++, input.path
			);
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


void create_output_video( clip_output_video_t& output, char* full_out_path, char** segment_paths, u32 segment_count, bool add_markers, u32 preset_i )
{
	// write ffmpeg concat.txt file
	FILE* fp = fopen( "concat.txt", "wb" );

	if ( fp == nullptr )
	{
		printf( "failed to open file handle to write concat.txt\n" );
		return;
	}

	for ( u32 i = 0; i < segment_count; i++ )
	{
		fwrite( "file '", 6, 1, fp );
		fwrite( segment_paths[ i ], strlen( segment_paths[ i ] ), 1, fp );
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


// encoding for a target file size, useful for creating discord videos
bool run_encode_inputs_target_size( clip_output_video_t& output, char**& segment_paths, u32& segment_i, clip_encode_preset_t& preset, u32 preset_i, enc_output_video_t& enc_output )
{
	return false;
}


// standard encode
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

		memset( full_out_path, 0, 4096 );

		strcat( full_out_path, out_dir );

		if ( preset.out_prefix )
			strcat( full_out_path, preset.out_prefix );

		strcat( full_out_path, prefix.prefix );
		strcat( full_out_path, output.name );
		strcat( full_out_path, "." );
		strcat( full_out_path, preset.ext );

		printf( "Output Video \"%s\"\n", full_out_path );

		// each time range is a segment, so this will count the total segments
		char** segment_paths = nullptr;
		u32    segment_i     = 0;
		bool   skip_output   = false;

		// create all video segments
		// TARGET SIZE NOT SETUP YET
		// if ( preset.target_size )
		// {
		// 	skip_output = !run_encode_inputs_target_size( output, segment_paths, segment_i, preset, preset_i, enc_output );
		// }
		// else
		{
			skip_output = !run_encode_inputs_standard( output, segment_paths, segment_i, preset, preset_i );
		}

		// if we want to skip this or we just have no video segments
		if ( !skip_output && segment_i > 0 )
		{
			// concat them together
			create_output_video( output, full_out_path, segment_paths, segment_i, !preset.target_size, preset_i );
		}

		// free data
		for ( u32 i = 0; i < segment_i; i++ )
			free( segment_paths[ i ] );

		free( segment_paths );
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

		// temp
		if ( preset.target_size )
			continue;

		strcat( out_dir, g_output_dir );

		if ( preset.out_folder_append )
		{
			strcat( out_dir, PATH_SEP_STR );
			strcat( out_dir, preset.out_folder_append );

			if ( fs_exists( out_dir ) )
			{
				if ( fs_is_file( out_dir ) )
				{
					printf( "Error: Output directory already exists as a file: \"%s\"\n", out_dir );
					continue;
				}
			}
			else if ( !fs_make_dir( out_dir ) )
			{
				printf( "Error: Failed to create output directory: \"%s\"\n", out_dir );
				continue;
			}
		}

		strcat( out_dir, PATH_SEP_STR );
		run_encode_preset( out_dir, preset, preset_i );
	}

	free( out_dir );
}

