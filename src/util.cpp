#include "util.h"


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


bool util_strncmp_base( const char* left, const char* right, size_t len )
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

	return util_strncmp_base( left, right, left_len );
}


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


// returns the file length in the len argument
char* fs_readfile( const char* path, size_t* len )
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

