#pragma once

#include <cstdio>
#include <cstring>
#include <stdlib.h>


// --------------------------------------------------------------------------------------------------------


using u8        = unsigned char;
using u16       = unsigned short;
using u32       = unsigned int;
using u64       = unsigned long long;

using s8        = char;
using s16       = short;
using s32       = int;
using s64       = long long;

using f32       = float;
using f64       = double;

using module    = void*;
using window_id = void*;


// --------------------------------------------------------------------------------------------------------


// struct ivec2
// {
// 	int x, y;
// };


using ivec2 = int[ 2 ];
using vec2  = float[ 2 ];


// struct vec2
// {
// 	float x, y;
// };


// --------------------------------------------------------------------------------------------------------

#define ARR_SIZE( arr ) ( sizeof( arr ) / sizeof( arr[ 0 ] ) )
#define MIN( a, b )     ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define MAX( a, b )     ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

#define SET_INT2( var, x, y ) \
	( var )[ 0 ] = x;         \
	( var )[ 1 ] = y;


template< typename T >
T CLAMP( T value, T low, T high )
{
	return ( value < low ) ? low : ( ( value > high ) ? high : value );
}


template< typename T >
T* ch_malloc( size_t count )
{
	T* data = (T*)malloc( count * sizeof( T ) );

	if ( data == nullptr )
	{
		printf( "malloc failed\n" );
		return nullptr;
	}

	memset( data, 0, count * sizeof( T ) );
	return data;

	// return (T*)malloc( count * sizeof( T ) );
}


template< typename T >
T* ch_calloc( size_t count )
{
	return (T*)calloc( count, sizeof( T ) );
}


template< typename T >
T* ch_realloc( T* data, size_t count )
{
	return (T*)realloc( data, count * sizeof( T ) );
}


// --------------------------------------------------------------------------------------------------------


char* util_strdup( const char* string );
char* util_strndup( const char* string, size_t len );

// takes in a pointer to realloc to
char* util_strdup_r( char* data, const char* string );
char* util_strndup_r( char* data, const char* string, size_t len );

char* fs_get_filename( const char* path );
char* fs_get_filename_no_ext( const char* path );

char* fs_get_filename( const char* path, size_t pathLen );
char* fs_get_filename_no_ext( const char* path, size_t pathLen );

// bool  fs_scandir();
