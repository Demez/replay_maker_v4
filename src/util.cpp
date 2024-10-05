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

