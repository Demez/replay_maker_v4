#include "main.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <ole2.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()
#include <GL/GL.h>
#include <direct.h>


// ----------------------------------------------------


module sys_load_library( const wchar_t* path )
{
	return (module)LoadLibrary( path );
}


void sys_close_library( module mod )
{
	FreeLibrary( (HMODULE)mod );
}


void* sys_load_func( module mod, const char* name )
{
	return GetProcAddress( (HMODULE)mod, name );
}


const wchar_t* sys_get_error_w()
{
	DWORD errorID = GetLastError();

	if ( errorID == 0 )
		return L"";  // No error message

	LPTSTR strErrorMessage = NULL;

	FormatMessage(
	  FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_ALLOCATE_BUFFER,
	  NULL,
	  errorID,
	  0,
	  strErrorMessage,
	  0,
	  NULL );

	static wchar_t message[ 512 ];
	memset( message, 512, 0 );
	_snwprintf( message, 512, L"Win32 API Error %ud: %s", errorID, strErrorMessage );

	// Free the Win32 string buffer.
	LocalFree( strErrorMessage );

	return message;
}


const char* sys_get_error()
{
	const wchar_t* error = sys_get_error_w();

	if ( !error )
		return "";

	return sys_to_utf8( error );
}


void sys_print_last_error()
{
	fwprintf( stderr, L"Error: %s\n", sys_get_error_w() );
}


wchar_t* sys_to_wchar( const char* spStr )
{
	int      sSize = MultiByteToWideChar( CP_UTF8, 0, spStr, -1, NULL, 0 );

	wchar_t* spDst = (wchar_t*)malloc( ( sSize + 1 ) * sizeof( wchar_t ) );
	memset( spDst, 0, ( sSize + 1 ) * sizeof( wchar_t ) );

	MultiByteToWideChar( CP_UTF8, 0, spStr, -1, spDst, sSize );

	return spDst;
}


wchar_t* sys_to_wchar( const char* spStr, int sSize )
{
	wchar_t* spDst = (wchar_t*)malloc( ( sSize + 1 ) * sizeof( wchar_t ) );
	memset( spDst, 0, ( sSize + 1 ) * sizeof( wchar_t ) );

	MultiByteToWideChar( CP_UTF8, 0, spStr, -1, spDst, sSize );

	return spDst;
}


char* sys_to_utf8( const wchar_t* spStr )
{
	int   sSize = WideCharToMultiByte( CP_UTF8, 0, spStr, -1, NULL, 0, NULL, NULL );

	char* spDst = (char*)malloc( ( sSize + 1 ) * sizeof( char ) );
	memset( spDst, 0, ( sSize + 1 ) * sizeof( char ) );

	int ret = WideCharToMultiByte( CP_UTF8, 0, spStr, -1, spDst, sSize, NULL, NULL );

	return spDst;
}


char* sys_to_utf8( const wchar_t* spStr, int sSize )
{
	char* spDst = (char*)malloc( ( sSize + 1 ) * sizeof( char ) );
	memset( spDst, 0, ( sSize + 1 ) * sizeof( char ) );

	WideCharToMultiByte( CP_UTF8, 0, spStr, -1, spDst, sSize, NULL, NULL );

	return spDst;
}


char* sys_get_exe_path( size_t* len )
{
	wchar_t output_w[ 4096 ];
	GetModuleFileName( NULL, output_w, 4096 );

	size_t len_w = wcslen( output_w );

	char* output = sys_to_utf8( output_w, len_w );

	if ( len )
		*len = len_w;

	return output;
}


char* sys_get_exe_folder( size_t* len )
{
	wchar_t output_w[ 4096 ];
	GetModuleFileName( NULL, output_w, 4096 );

	size_t len_w = wcslen( output_w );

	// find index of last path separator
	wchar_t* sep    = wcsrchr( output_w, '\\' );
	size_t   path_i = sep - output_w;

	char*    output = sys_to_utf8( output_w, path_i );

	if ( len )
		*len = path_i;

	return output;
}


char* sys_get_cwd()
{
	return _getcwd( 0, 0 );
}

