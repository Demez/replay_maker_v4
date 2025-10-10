#include "main.h"
#include "util.h"

#include <stdarg.h>
#include <time.h>


extern const char*    g_log_dir;
extern const char*    g_video_files;

char                  g_date_str[ 32 ]{};
char*                 g_file_name        = nullptr;

FILE*                 g_log_file         = nullptr;
FILE*                 g_log_file_results = nullptr;


constexpr const char* g_log_color_ansi[] = {
	"\033[0m",     // Default
	"\033[0;30m",  // Black
	"\033[1;37m",  // White

	"\033[0;34m",  // Dark Blue
	"\033[0;32m",  // Dark Green
	"\033[0;36m",  // Dark Cyan
	"\033[0;31m",  // Dark Red
	"\033[0;35m",  // Dark Purple
	"\033[0;33m",  // Dark Yellow
	"\033[0;30m",  // Dark Gray

	"\033[1;34m",  // Blue
	"\033[1;32m",  // Green
	"\033[1;36m",  // Cyan
	"\033[1;31m",  // Red
	"\033[1;35m",  // Purple
	"\033[1;33m",  // Yellow
	"\033[1;30m",  // Gray
};


const char* log_get_color_ansi( e_log_color color )
{
	return g_log_color_ansi[ static_cast< int >( color ) ];
}


void log_set_color_ansi( e_log_color color )
{
	fputs( log_get_color_ansi( color ), stdout );
}


#if _WIN32
#include <consoleapi2.h>
#include <debugapi.h>

extern HANDLE g_con_out;

int g_log_color_win32[] = {
	FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,                         // Default
	0,                                                                           // Black
	FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,  // White

	FOREGROUND_BLUE,                                      // Dark Blue
	FOREGROUND_GREEN,                                     // Dark Green
	FOREGROUND_GREEN | FOREGROUND_BLUE,                   // Dark Cyan
	FOREGROUND_RED,                                       // Dark Red
	FOREGROUND_RED | FOREGROUND_BLUE,                     // Dark Purple
	FOREGROUND_RED | FOREGROUND_GREEN,                    // Dark Yellow
	FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,  // Dark Gray

	FOREGROUND_INTENSITY | FOREGROUND_BLUE,                     // Blue
	FOREGROUND_INTENSITY | FOREGROUND_GREEN,                    // Green
	FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE,  // Cyan
	FOREGROUND_INTENSITY | FOREGROUND_RED,                      // Red
	FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,    // Purple
	FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,   // Yellow
	FOREGROUND_INTENSITY,                                       // Gray
};


constexpr int win32_get_color( e_log_color color )
{
	return g_log_color_win32[ static_cast< int >( color ) ];
}


void win32_set_color( e_log_color color )
{
	BOOL result = SetConsoleTextAttribute( g_con_out, win32_get_color( color ) );

	if ( !result )
	{
		printf( "*** Failed to set console color: %s\n", sys_get_error() );
	}
}
#endif


void log_set_con_color( e_log_color color )
{
#ifdef _WIN32
	win32_set_color( color );
#elif __unix__
	// technically works in windows 10, might try later and see if it's cheaper, idk
	log_set_color_ansi( color );
#endif
}


bool log_init()
{
	time_t     time_raw;
	struct tm* time_info;

	time( &time_raw );
	time_info       = localtime( &time_raw );

	strftime( g_date_str, 32, "%F_%H-%M-%S", time_info );

	g_file_name = fs_get_filename_no_ext( g_video_files );

	// Format filename:
	// Example - replay_encoder__2025-10-09_20-54-08__filename.log
	char file_path[ 2048 ]{};
	snprintf( file_path, 2048, "%s" SEP_S "replay_encoder__%s__%s__results.log", g_log_dir, g_date_str, g_file_name );

	g_log_file_results = fopen( file_path, "w" );

	if ( !g_log_file_results )
	{
		printf( "FAILED TO OPEN RESULTS LOG FILE FOR WRITING\n" );
		return false;
	}

	if ( !log_set_file( "init" ) )
		return false;

	return true;
}


char* log_build_name( const char* name )
{
	// Format filename:
	// Example - replay_encoder__2025-10-09_20-54-08__filename__custom_name.log
	size_t len = snprintf( nullptr, 0, "%s" SEP_S "replay_encoder__%s__%s__%s.log", g_log_dir, g_date_str, g_file_name, name );
	char* file_name = ch_calloc< char >( len );
	snprintf( file_name, len, "%s" SEP_S "replay_encoder__%s__%s__%s.log", g_log_dir, g_date_str, g_file_name, name );

	return file_name;
}


bool log_set_file( const char* name )
{
	if ( g_log_file )
	{
		fflush( g_log_file );
		fclose( g_log_file );
		g_log_file = nullptr;
	}
	
	// Format filename:
	// Example - replay_encoder__2025-10-09_20-54-08__filename__custom_name.log
	char  file_path[ 2048 ]{};
	snprintf( file_path, 2048, "%s" SEP_S "replay_encoder__%s__%s__%s.log", g_log_dir, g_date_str, g_file_name, name );

	g_log_file = fopen( file_path, "w" );

	if ( !g_log_file )
	{
		printf( "FAILED TO OPEN LOG FILE FOR WRITING\n" );
		return false;
	}

	return true;
}


void log_shutdown()
{
	free( g_file_name );

	if ( g_log_file )
	{
		fflush( g_log_file );
		fclose( g_log_file );
	}

	if ( g_log_file_results )
	{
		fflush( g_log_file_results );
		fclose( g_log_file_results );
	}

	g_log_file         = nullptr;
	g_log_file_results = nullptr;
}


void log_write_quick( const char* buffer, size_t len )
{
	fwrite( buffer, sizeof( char ), len, g_log_file );
}


void log_write( const char* buffer, size_t len )
{
	fwrite( buffer, sizeof( char ), len, g_log_file );
	fflush( g_log_file );
}


// return new length after removing carriage return characters
size_t log_remove_carriage_return( char* buffer, size_t len, char*& output )
{
	char*  pos      = strchr( buffer, '\r' );
	char*  last_pos = buffer;
	size_t remain   = len;

	output         = ch_malloc< char >( len + 1 );

	while ( pos )
	{
		// memmove( pos, pos + 1, sizeof( char ) * ( remain - 1 ) );
		strncat( output, last_pos, sizeof( char ) * ( pos - last_pos ) );

		last_pos = ++pos;

		// needed for ffmpeg frame encoding lines, where it only uses carriage returns
		if ( *pos != '\n' )
			strncat( output, "\n", sizeof( char ) * 1 );

		pos = strchr( pos, '\r' );
		remain = pos - buffer;
	}

	// output[ new_len ] = '\0';
	size_t new_len = strlen( output );

	return new_len;
}


void log_print_v( log_channel channel, const char* format, va_list args )
{
	va_list copy;
	va_copy( copy, args );
	int len = std::vsnprintf( nullptr, 0, format, copy );
	va_end( copy );

	if ( len < 0 )
	{
		printf( "\n *** logging: vsnprintf failed?\n\n" );
		return;
	}

	char* result = ch_malloc< char >( len + 1 );
	std::vsnprintf( result, len + 1, format, args );
	result[ len ] = '\0';

	switch ( channel )
	{
		default:
		case log_general:
		{
			log_set_con_color( e_log_color_cyan );

			printf( result );
			log_write( result, len );

			log_set_con_color( e_log_color_default );
			break;
		}
		case log_ffmpeg:
		{
			printf( result );

			// still buggy
#if 0
			char*  temp_output;
			size_t new_len = log_remove_carriage_return( result, len, temp_output );
			log_write( temp_output, new_len );
			free( temp_output );
#else
			log_write( result, len );
#endif

			break;
		}
		case log_warning:
		{
			log_set_con_color( e_log_color_yellow );

			printf( "warn: %s", result );
			log_write_quick( "warn: ", 6 );
			log_write( result, len );

			log_set_con_color( e_log_color_default );
			break;
		}
		case log_error:
		{
			log_set_con_color( e_log_color_red );

			printf( "err: %s", result );
			log_write_quick( "err: ", 5 );
			log_write( result, len );

			log_set_con_color( e_log_color_default );
			break; 
		}
		case log_result:
		{
			printf( result );
			fwrite( result, sizeof( char ), len, g_log_file_results );
			fflush( g_log_file_results );
			break; 
		}
	}

	free( result );
}


void log_printf( const char* format, ... )
{
	va_list args;
	va_start( args, format );
	log_print_v( log_general, format, args );
	va_end( args );
}


void log_printf( log_channel channel, const char* format, ... )
{
	va_list args;
	va_start( args, format );
	log_print_v( channel, format, args );
	va_end( args );
}

