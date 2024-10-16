#include "util.h"

#include <sys/stat.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
  #include <direct.h>
  #include <io.h>

  // get rid of the dumb windows posix depreciation warnings
  #define mkdir      _mkdir
  #define chdir      _chdir
  #define access     _access
  #define getcwd     _getcwd

  #define stat       _stat
#else
  #include <unistd.h>
  #include <dirent.h>
  #include <string.h>

// windows-specific mkdir() is used
  #define mkdir( f ) mkdir( f, 666 )
#endif


char* util_strdup( const char* string )
{
	return util_strdup_r( nullptr, string );
}


char* util_strndup( const char* string, size_t len )
{
	return util_strndup_r( nullptr, string, len );
}


char* util_strdup_r( char* data, const char* string )
{
	if ( !string )
		return nullptr;

	size_t len = strlen( string );

	if ( len == 0 )
		return nullptr;

	char* new_data = ch_realloc( data, len + 1 );

	if ( !new_data )
		return nullptr;

	memcpy( new_data, string, len * sizeof( char ) );
	new_data[ len ] = '\0';
	return new_data;
}


char* util_strndup_r( char* data, const char* string, size_t len )
{
	if ( !string )
		return nullptr;

	if ( len == 0 )
		return nullptr;

	char* new_data = ch_realloc( data, len + 1 );

	if ( !new_data )
		return nullptr;

	memcpy( new_data, string, len * sizeof( char ) );
	new_data[ len ] = '\0';
	return new_data;
}


bool util_strncmp( const char* left, const char* right, size_t len )
{
	const char*       cur1 = left;
	const char*       cur2 = right;
	const char* const end  = len + left;

	for ( ; cur1 < end; ++cur1, ++cur2 )
	{
		if ( *cur1 != *cur2 )
			return false;
	}

	return true;
}


bool util_strncmp( const char* left, size_t left_len, const char* right, size_t right_len )
{
	if ( left_len != right_len )
		return false;

	return util_strncmp( left, right, left_len );
}


void util_append_str( str_buf_t& buffer, const char* str, size_t len, size_t buffer_size )
{
	if ( ( len + buffer.size ) > buffer.capacity )
	{
		size_t increase = MAX( len, buffer_size );
		char*  new_data = ch_realloc( buffer.data, buffer.capacity + increase );

		if ( !new_data )
		{
			printf( "util_append_str: failed to increase string buffer size!\n" );
			return;
		}

		buffer.capacity += increase;
		buffer.data = new_data;
	}

	memcpy( &buffer.data[ buffer.size ], str, len * sizeof( char ) );
	buffer.size += len;
}


void util_append_str( str_buf_t& buffer, const char* str, size_t len )
{
	util_append_str( buffer, str, len, STR_BUF_SIZE );
}


void util_format_time( char* buffer, size_t buffer_size, double time )
{
	if ( buffer_size < 9 )
		return;

	time_t     time_time_pos = (time_t)time;

	struct tm* tm_info;

	tm_info = gmtime( &time_time_pos );
	strftime( buffer, 9, "%H:%M:%S", tm_info );

	if ( buffer_size == 9 )
		return;

	// add miliseconds
	snprintf( buffer + 8, buffer_size - 8, "%.8f", fmod( time, 1 ) );

	// move it back to get rid of the 0 lol
	memcpy( buffer + 8, buffer + 9, buffer_size - 9 );
	buffer[ buffer_size - 1 ] = '0';
}


void util_format_time( char* buffer, double time )
{
	return util_format_time( buffer, TIME_BUFFER, time );
}


// ============================================================================================


char* fs_get_filename( const char* path, size_t path_len )
{
	if ( !path || path_len == 0 )
		return nullptr;

	size_t i = path_len - 1;
	for ( ; i > 0; i-- )
	{
		if ( path[ i ] == '/' || path[ i ] == '\\' )
			break;
	}

	// No File Extension Found
	if ( i == path_len )
		return {};

	size_t start_index = i + 1;

	if ( start_index == path_len )
		return {};

	return util_strndup( &path[ start_index ], path_len - start_index );
}


char* fs_get_filename_no_ext( const char* path, size_t path_len )
{
	if ( !path || path_len == 0 )
		return nullptr;

	char* name = fs_get_filename( path, path_len );

	if ( !name )
		return nullptr;

	char* dot = strrchr( name, '.' );

	if ( !dot || dot == name )
		return name;

	char* output = util_strndup( name, dot - name );
	free( name );
	return output;
}


char* fs_get_filename( const char* path )
{
	if ( !path )
		return nullptr;

	return fs_get_filename( path, strlen( path ) );
}


char* fs_get_filename_no_ext( const char* path )
{
	if ( !path )
		return nullptr;

	return fs_get_filename_no_ext( path, strlen( path ) );
}


bool fs_exists( const char* path )
{
	return access( path, 0 ) != -1;
}


bool fs_make_dir( const char* path )
{
	return mkdir( path ) == 0;
}

bool fs_is_dir( const char* path )
{
	struct stat s;

	if ( stat( path, &s ) == 0 )
		return ( s.st_mode & S_IFDIR );

	return false;
}


bool fs_is_file( const char* path )
{
	struct stat s;

	if ( stat( path, &s ) == 0 )
		return ( s.st_mode & S_IFREG );

	return false;
}


u64 fs_file_size( const char* path )
{
	struct stat s;

	if ( stat( path, &s ) == 0 )
		return s.st_size;

	return 0;
}


// returns the file length in the len argument
char* fs_read_file( const char* path, size_t* len )
{
	FILE* fp = fopen( path, "r" );

	if ( !fp )
	{
		return nullptr;
	}

	fseek( fp, 0, SEEK_END );
	long size = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	char* output = (char*)malloc( ( size + 1 ) * sizeof( char ) );

	if ( !output )
	{
		return nullptr;
	}

	fread( output, size, 1, fp );
	fclose( fp );

	output[ size ] = 0;

	if ( len )
		*len = size;

	return output;
}


static bool handle_rename( const char* path, const char* new_path )
{
	int code = rename( path, new_path );

	if ( code == 0 )
		return true;

	printf( "failed to rename old saved file \"%s\" - ", path );

	switch ( code )
	{
		case EACCES:
			printf( "Permission denied\n" );
			break;
		case ENOENT:
			printf( "Source file does not exist\n" );
			break;
		case EEXIST:
			printf( "A file with the new filename already exists\n" );
			break;
		case EINVAL:
			printf( "The names specified are invalid\n" );
			break;
	}

	return false;
}


bool fs_save_file( const char* path, const char* data, size_t size )
{
	// write to a temp file,
	// then rename to old saved file to name.bak,
	// then remove .temp from new file, and remove .bak file (or keep it until next save)
	// also check if a .temp file already exists just in case if a crash happened midway through this
	// basically this is all so if there is a crashe at any point during this, we dont lose any data

	char temp_path[ 4096 ] = { 0 };
	strcat( temp_path, path );
	strcat( temp_path, ".temp" );

	char bak_path[ 4096 ] = { 0 };
	strcat( bak_path, path );
	strcat( bak_path, ".bak" );

	// check if a .temp file exists already
	if ( access( temp_path, 0 ) != -1 )
	{
		if ( !sys_recycle_file( temp_path ) )
		{
			printf( "failed to delete old temp file for saving! - \"%s\"\n", temp_path );
			return false;
		}
	}

	FILE* fp = fopen( temp_path, "wb" );

	if ( fp == nullptr )
	{
		printf( "failed to open file handle to write file to\n - \"%s\"", temp_path );
		return false;
	}

	size_t amount_wrote = fwrite( data, size, 1, fp );

	fclose( fp );

	// check if a saved file exists already
	bool old_save_exists = access( path, 0 ) != -1;

	if ( old_save_exists )
	{
		// check if a .bak file exists already
		if ( access( bak_path, 0 ) != -1 )
		{
			if ( !sys_recycle_file( bak_path ) )
			{
				printf( "failed to delete old backup file for saving! - \"%s\"\n", bak_path );
				return false;
			}
		}

		if ( !handle_rename( path, bak_path ) )
			return false;
	}

	if ( !handle_rename( temp_path, path ) )
		return false;

	// copy file creation date
	u64 create_date = 0;

	if ( old_save_exists && sys_get_file_times( bak_path, &create_date, nullptr, nullptr ) )
	{
		sys_set_file_times( path, &create_date, nullptr, nullptr );
	}

	return true;
}


bool fs_write_file( const char* path, const char* data, size_t size )
{
	FILE* fp = fopen( path, "wb" );

	if ( fp == nullptr )
	{
		printf( "failed to open file handle to write file to\n - \"%s\"", path );
		return false;
	}

	size_t amount_wrote = fwrite( data, size, 1, fp );

	fclose( fp );

	return true;
}

