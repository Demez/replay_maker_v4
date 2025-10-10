#pragma once

#include <cstdio>
#include <cstring>
#include <stdlib.h>


// --------------------------------------------------------------------------------------------------------


using s8    = char;
using s16   = short;
using s32   = int;
using s64   = long long;

using u8    = unsigned char;
using u16   = unsigned short;
using u32   = unsigned int;
using u64   = unsigned long long;

using f32   = float;
using f64   = double;

using module_t = void*;


// --------------------------------------------------------------------------------------------------------


#ifdef _WIN32
  #define SEP_S "\\"
  #define SEP '\\'

  #define PATH_SEP_STR "\\"
  #define PATH_SEP     '\\'

  #define strncasecmp _strnicmp
  #define strcasecmp  _stricmp
#else
  #define SEP_S        "/"
  #define SEP          '/'

  #define PATH_SEP_STR "/"
  #define PATH_SEP     '/'
#endif


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


constexpr size_t STR_BUF_SIZE = 512;
constexpr size_t TIME_BUFFER  = 14;


struct str_buf_t
{
	char*  data;
	size_t capacity;
	size_t size;
};


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


template< typename T >
T* ch_recalloc( T* data, size_t count, size_t add_count )
{
	T* new_data = (T*)realloc( data, (count + add_count) * sizeof( T ) );

	if ( new_data )
		memset( &new_data[ count ], 0, add_count * sizeof( T ) );

	return new_data;
}


// --------------------------------------------------------------------------------------------------------


// removes the element and shifts everything back, and memsets the last item with 0
template< typename T, typename COUNT_TYPE >
void util_array_remove_element( T* data, COUNT_TYPE& count, COUNT_TYPE index )
{
	if ( index >= count )
		return;

	memcpy( &data[ index ], &data[ index + 1 ], sizeof( T ) * ( count - index ) );
	count--;

	if ( count == 0 )
		return;

	memset( &data[ count ], 0, sizeof( T ) );
}


template< typename T >
bool array_append( T*& data, u32 count )
{
#if 1
	T* new_data = ch_recalloc< T >( data, count, 1 );

	if ( !new_data )
		return true;

	data = new_data;
#else
	T* new_data = ch_realloc< T >( data, count + 1 );

	if ( !new_data )
		return true;

	data = new_data;
	memset( &data[ count ], 0, sizeof( T ) );
#endif

	return false;
}


template< typename T >
bool array_append_err( T*& data, u32 count, const char* msg )
{
#if 1
	T* new_data = ch_recalloc< T >( data, count, 1 );

	if ( !new_data )
	{
		fputs( msg, stdout );
		return true;
	}

	data = new_data;
#else
	T* new_data = ch_realloc< T >( data, count + 1 );

	if ( !new_data )
		return true;

	data = new_data;
	memset( &data[ count ], 0, sizeof( T ) );
#endif

	return false;
}


// --------------------------------------------------------------------------------------------------------
// system functions


// library loading
module_t    sys_load_library( const wchar_t* path );
void        sys_close_library( module_t mod );
void*       sys_load_func( module_t mod, const char* path );
const char* sys_get_error();
const wchar_t* sys_get_error_w();
void        sys_print_last_error();

int         sys_init();
void        sys_shutdown();

// also known as "sys_to_utf16"
wchar_t*    sys_to_wchar( const char* spStr, int sSize );
wchar_t*    sys_to_wchar( const char* spStr );

// prepends "\\?\" on the string for windows
// https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
wchar_t*    sys_to_wchar_extended( const char* spStr, int sSize );
wchar_t*    sys_to_wchar_extended( const char* spStr );

wchar_t*    sys_to_wchar_short( const char* spStr, int sSize );
wchar_t*    sys_to_wchar_short( const char* spStr );

char*       sys_to_utf8( const wchar_t* spStr, int sSize );
char*       sys_to_utf8( const wchar_t* spStr );

// get folder exe is stored in
char*       sys_get_exe_folder( size_t* len = nullptr );

// get the full path of the exe
char*       sys_get_exe_path( size_t* len = nullptr );

// get current working directory
char*       sys_get_cwd();

// on windows, this sends the file to the recycle bin
// it does the equivalent on other platforms
bool        sys_recycle_file( const char* path );

// hack for above
void        sys_set_main_hwnd( void* hwnd );

bool        sys_get_file_times( const char* path, u64* creation, u64* access, u64* write );
bool        sys_set_file_times( const char* path, u64* creation, u64* access, u64* write );
bool        sys_copy_file_times( const char* src_path, const char* out_path, bool creation, bool access, bool write );

// execute a command and read it's output
bool        sys_execute_read( const char* command, str_buf_t& output );

// execute a command and read it's output, with a callback function everytime more output is read from the file
using       f_exec_callback = void( char* buf, size_t len );
bool        sys_execute_read_callback( const char* command, str_buf_t& output, f_exec_callback* p_exec_callback );

// execute a command and return the commands return value
int         sys_execute( const char* command );

// NOTE: path cannot be over MAX_PATH (260 characters), thanks windows shell
void        sys_browse_to_file( const char* path );

// print color with \aFFF escape codes for color values
//void        sys_print_color( const char* string );


// --------------------------------------------------------------------------------------------------------
// utility functions

#if _WIN32
char*       strcasestr( const char* s, const char* find );
#endif

char*       util_strdup( const char* string );
char*       util_strndup( const char* string, size_t len );

// takes in a pointer to realloc to
char*       util_strdup_r( char* data, const char* string );
char*       util_strndup_r( char* data, const char* string, size_t len );

bool        util_strncmp( const char* left, const char* right, size_t len );
bool        util_strncmp( const char* left, size_t left_len, const char* right, size_t right_len );

void        util_append_str( str_buf_t& buffer, const char* str, size_t len );
void        util_append_str( str_buf_t& buffer, const char* str, size_t len, size_t buffer_size );

// kinda lame lol
void        util_format_time( char* buffer, double time );  // expects at least TIME_BUFFER characters in buffer
void        util_format_time( char* buffer, size_t buffer_size, double time );


// --------------------------------------------------------------------------------------------------------
// file system functions


char*       fs_get_filename( const char* path );
char*       fs_get_filename_no_ext( const char* path );

char*       fs_get_filename( const char* path, size_t pathLen );
char*       fs_get_filename_no_ext( const char* path, size_t pathLen );

bool        fs_exists( const char* path );
bool        fs_make_dir( const char* path );
bool        fs_is_dir( const char* path );
bool        fs_is_file( const char* path );

// replace all backslash path separators with forward slashes
char*       fs_replace_path_seps_unix( const char* path );

// checks if it exists and if it's a file and not a directory
bool        fs_make_dir_check( const char* path );

// returns file size in bytes
u64         fs_file_size( const char* path );

// returns the file length in the len argument, optional
char*       fs_read_file( const char* path, size_t* len = nullptr );

// ensures no data loss happens and backs up the old file
bool        fs_save_file( const char* path, const char* data, size_t size );

// overrwites any existing file
bool        fs_write_file( const char* path, const char* data, size_t size );



