#include "main.h"


static bool parse_ffprobe_int( bool& failed, char*& line_start, const char* name, size_t name_len, int& out_value )
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

	if ( !output.data || output.data[ 0 ] == '\0' )
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

	if ( !output.data || output.data[ 0 ] == '\0' )
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


// makes sure the time range desired is valid for this source video
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

