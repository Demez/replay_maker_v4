#include "util.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <ole2.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()
#include <GL/GL.h>
#include <direct.h>
#include <shellapi.h>
#include <shlwapi.h> 
#include <shlobj_core.h> 
#include <time.h>


// ----------------------------------------------------


HANDLE g_con_out = INVALID_HANDLE_VALUE;


module_t sys_load_library( const wchar_t* path )
{
	return (module_t)LoadLibrary( path );
}


void sys_close_library( module_t mod )
{
	FreeLibrary( (HMODULE)mod );
}


void* sys_load_func( module_t mod, const char* name )
{
	return GetProcAddress( (HMODULE)mod, name );
}


const wchar_t* sys_get_error_w()
{
	DWORD errorID = GetLastError();

	if ( errorID == 0 )
		return L"";  // No error message

	// LPTSTR strErrorMessage = NULL;
	WCHAR strErrorMessage[ 1024 ];

	DWORD ret = FormatMessageW(
	  FORMAT_MESSAGE_FROM_SYSTEM,
	  NULL,
	  errorID,
	  0,
	  strErrorMessage,
	  1024,
	  NULL );

	static wchar_t message[ 1100 ];
	memset( message, 1100, 0 );

	if ( ret == 0 )
	{
		printf( "smh FormatMessageW failed with %d\n", GetLastError() );
		_snwprintf( message, 1100, L"Win32 API Error %ud", errorID );
		return message;
	}

	_snwprintf( message, 1100, L"Win32 API Error %u: %s", errorID, strErrorMessage );

	// Free the Win32 string buffer.
	// LocalFree( strErrorMessage );

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


wchar_t* sys_to_wchar_extended( const char* spStr )
{
	int      sSize = MultiByteToWideChar( CP_UTF8, 0, spStr, -1, NULL, 0 );

	wchar_t* spDst = (wchar_t*)malloc( ( sSize + 5 ) * sizeof( wchar_t ) );
	memset( spDst, 0, ( sSize + 1 ) * sizeof( wchar_t ) );
	wcscat( spDst, L"\\\\?\\" );

	MultiByteToWideChar( CP_UTF8, 0, spStr, -1, spDst + 4, sSize );

	return spDst;
}


wchar_t* sys_to_wchar_extended( const char* spStr, int sSize )
{
	wchar_t* spDst = (wchar_t*)malloc( ( sSize + 5 ) * sizeof( wchar_t ) );
	memset( spDst, 0, ( sSize + 1 ) * sizeof( wchar_t ) );
	wcscat( spDst, L"\\\\?\\" );

	MultiByteToWideChar( CP_UTF8, 0, spStr, -1, spDst + 4, sSize );

	return spDst;
}


// this doesn't actually work for some reason
#if 0
// wchar_t* sys_to_wchar_short( const char* spStr, int sSize )
// {
// 	wchar_t* spDst = (wchar_t*)malloc( ( sSize + 1 ) * sizeof( wchar_t ) );
// 	memset( spDst, 0, ( sSize + 1 ) * sizeof( wchar_t ) );
// 
// 	MultiByteToWideChar( CP_UTF8, 0, spStr, -1, spDst, sSize );
// 
// 	return spDst;
// }


wchar_t* sys_to_wchar_short( const char* spStr )
{
	int      sSize = MultiByteToWideChar( CP_UTF8, 0, spStr, -1, NULL, 0 );

#if 1
	wchar_t* spDst = (wchar_t*)malloc( ( sSize + 5 ) * sizeof( wchar_t ) );
	memset( spDst, 0, ( sSize + 1 ) * sizeof( wchar_t ) );
	wcscat( spDst, L"\\\\?\\" );

	MultiByteToWideChar( CP_UTF8, 0, spStr, -1, spDst + 4, sSize );
#else
	wchar_t* spDst = (wchar_t*)malloc( ( sSize + 1 ) * sizeof( wchar_t ) );
	memset( spDst, 0, ( sSize + 1 ) * sizeof( wchar_t ) );

	MultiByteToWideChar( CP_UTF8, 0, spStr, -1, spDst, sSize );
#endif

	wchar_t short_path[ MAX_PATH * 2 ] = { 0 };
	DWORD   ret                    = GetShortPathNameW( spDst, short_path, MAX_PATH * 2 );

	free( spDst );

	return wcsdup( short_path );
}
#endif


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


bool sys_recycle_file( const char* path )
{
	//if ( !hwnd )
	//{
	//	printf( "No HWND specified" );
	//	return false;
	//}

	TCHAR Buffer[ 2048 + 4 ];

	wchar_t* path_w = sys_to_wchar( path );

	wcsncpy_s( Buffer, 2048 + 4, path_w, 2048 );
	Buffer[ wcslen( Buffer ) + 1 ] = 0;  //Double-Null-Termination

	SHFILEOPSTRUCT s;
	s.hwnd                  = NULL;
	s.wFunc                 = FO_DELETE;
	s.pFrom                 = Buffer;
	s.pTo                   = NULL;
	s.fFlags                = FOF_ALLOWUNDO;
	s.fAnyOperationsAborted = false;
	s.hNameMappings         = NULL;
	s.lpszProgressTitle     = NULL;

	//if ( !showConfirm )
		s.fFlags |= FOF_SILENT;

	int rc = SHFileOperation( &s );

	free( path_w );

	if ( rc != 0 )
	{
		printf( "Failed To Delete File: %s\n", path );
		return false;
	}

	printf( "Deleted File: %s\n", path );
	return true;
}


// hack for above
void sys_set_main_hwnd( void* hwnd )
{
}


// static u64 file_time_to_u64( FILETIME& time )
// {
// 	return static_cast< u64 >( time.dwHighDateTime ) << 32 | time.dwLowDateTime;
// }


// this is weird
// https://stackoverflow.com/a/26416380
u64 file_time_to_unix( const FILETIME& filetime )
{
	FILETIME localFileTime;
	FileTimeToLocalFileTime( &filetime, &localFileTime );
	SYSTEMTIME sysTime;
	FileTimeToSystemTime( &localFileTime, &sysTime );
	struct tm tmtime = { 0 };
	tmtime.tm_year   = sysTime.wYear - 1900;
	tmtime.tm_mon    = sysTime.wMonth - 1;
	tmtime.tm_mday   = sysTime.wDay;
	tmtime.tm_hour   = sysTime.wHour;
	tmtime.tm_min    = sysTime.wMinute;
	tmtime.tm_sec    = sysTime.wSecond;
	tmtime.tm_wday   = 0;
	tmtime.tm_yday   = 0;
	tmtime.tm_isdst  = -1;
	time_t ret       = mktime( &tmtime );
	return ret;
}  


// ????
// https://support.microsoft.com/en-us/topic/bf03df72-96e4-59f3-1d02-b6781002dc7f
static FILETIME file_time_from_unix( u64 time )
{
	// Note that LONGLONG is a 64-bit value
	LONGLONG ll;
	FILETIME filetime;

	ll                      = Int32x32To64( time, 10000000 ) + 116444736000000000;
	filetime.dwLowDateTime  = (DWORD)ll;
	filetime.dwHighDateTime = ll >> 32;

	return filetime;
}


bool sys_get_file_times( const char* path, u64* creation, u64* access, u64* write )
{
	wchar_t* path_w = sys_to_wchar_extended( path );

	HANDLE file     = CreateFile( path_w, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );

	if ( file == INVALID_HANDLE_VALUE )
	{
		printf( "failed to open file handle for \"%s\"\n", path );
		sys_print_last_error();
		return false;
	}

	FILETIME file_create{}, file_access{}, file_write{};
	BOOL     ret = GetFileTime( file, &file_create, &file_access, &file_write );

	CloseHandle( file );

	if ( ret == FALSE )
	{
		sys_print_last_error();
		printf( "failed to get file time - \"%s\"\n", path );

		if ( creation )
			*creation = 0;

		if ( access )
			*access = 0;

		if ( write )
			*write = 0;

		return false;
	}

	free( path_w );

	if ( creation )
		*creation = file_time_to_unix( file_create );

	if ( access )
		*access = file_time_to_unix( file_access );

	if ( write )
		*write = file_time_to_unix( file_write );

	return true;
}


// unreliable as hell wtf
bool sys_set_file_times( const char* path, u64* creation, u64* access, u64* write )
{
	wchar_t* path_w = sys_to_wchar_extended( path );

	// FILE_WRITE_ATTRIBUTES

	// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew#caching-behavior
	// FILE_FLAG_RANDOM_ACCESS

	SECURITY_ATTRIBUTES attrib{};

	HANDLE   file   = CreateFile( path_w, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );

	if ( file == INVALID_HANDLE_VALUE )
	{
		printf( "failed to open file handle for \"%s\"\n", path );
		sys_print_last_error();
		return false;
	}

	FILETIME file_create{}, file_access{}, file_write{};
	BOOL     ret = GetFileTime( file, &file_create, &file_access, &file_write );

	if ( ret == FALSE )
	{
		CloseHandle( file );
		sys_print_last_error();
		printf( "failed to get file time - \"%s\"\n", path );
		return false;
	}

	if ( creation )
		file_create = file_time_from_unix( *creation );

	if ( access )
		file_access = file_time_from_unix( *access );
	 
	if ( write )
		file_write = file_time_from_unix( *write );

	ret = SetFileTime( file, &file_create, &file_access, &file_write );

	CloseHandle( file );

	if ( ret == FALSE )
	{
		sys_print_last_error();
		printf( "failed to set file time - \"%s\"\n", path );
		return false;
	}

	free( path_w );

	return true;
}


bool sys_copy_file_times( const char* src_path, const char* out_path, bool creation, bool access, bool write )
{
	return false;
}


// https://stackoverflow.com/a/35658917
bool sys_execute_read_callback( const char* command, str_buf_t& output, f_exec_callback* p_exec_callback )
{
	if ( !p_exec_callback )
		return false;

	HANDLE              hPipeRead, hPipeWrite;

	SECURITY_ATTRIBUTES saAttr  = { sizeof( SECURITY_ATTRIBUTES ) };
	saAttr.bInheritHandle       = TRUE;  // Pipe handles are inherited by child process.
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe to get results from child's stdout.
	if ( !CreatePipe( &hPipeRead, &hPipeWrite, &saAttr, 0 ) )
		return false;

	STARTUPINFOW si              = { sizeof( STARTUPINFOW ) };
	si.dwFlags                   = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.hStdOutput                = hPipeWrite;
	si.hStdError                 = hPipeWrite;
	si.wShowWindow               = SW_HIDE;  // Prevents cmd window from flashing.
											 // Requires STARTF_USESHOWWINDOW in dwFlags.

	PROCESS_INFORMATION pi       = { 0 };

	wchar_t*            command_w = sys_to_wchar( command );

	BOOL                fSuccess  = CreateProcessW( NULL, command_w, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi );

	free( command_w );

	if ( !fSuccess )
	{
		CloseHandle( hPipeWrite );
		CloseHandle( hPipeRead );
		return false;
	}

	memset( &output, 0, sizeof( str_buf_t ) );

	bool bProcessEnded = false;
	while ( !bProcessEnded )
	{
		// Give some timeslice (50 ms), so we won't waste 100% CPU.
		bProcessEnded = WaitForSingleObject( pi.hProcess, 50 ) == WAIT_OBJECT_0;

		// Even if process exited - we continue reading, if
		// there is some data available over pipe.
		for ( ;; )
		{
			char  buf[ 1024 ];
			DWORD dwRead  = 0;
			DWORD dwAvail = 0;

			if ( !::PeekNamedPipe( hPipeRead, NULL, 0, NULL, &dwAvail, NULL ) )
				break;

			if ( !dwAvail )  // No data available, return
				break;

			if ( !::ReadFile( hPipeRead, buf, MIN( sizeof( buf ) - 1, dwAvail ), &dwRead, NULL ) || !dwRead )
				// Error, the child process might ended
				break;

			buf[ dwRead ] = 0;
			size_t buf_len = strlen( buf );
			p_exec_callback( buf, buf_len );
			util_append_str( output, buf, buf_len, 2048 );
		}
	}

	util_append_str( output, "\0", 1, 1 );

	CloseHandle( hPipeWrite );
	CloseHandle( hPipeRead );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
	return true;
}


// https://stackoverflow.com/a/35658917
bool sys_execute_read( const char* command, str_buf_t& output )
{
	HANDLE              hPipeRead, hPipeWrite;

	SECURITY_ATTRIBUTES saAttr  = { sizeof( SECURITY_ATTRIBUTES ) };
	saAttr.bInheritHandle       = TRUE;  // Pipe handles are inherited by child process.
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe to get results from child's stdout.
	if ( !CreatePipe( &hPipeRead, &hPipeWrite, &saAttr, 0 ) )
		return false;

	STARTUPINFOW si              = { sizeof( STARTUPINFOW ) };
	si.dwFlags                   = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.hStdOutput                = hPipeWrite;
	si.hStdError                 = hPipeWrite;
	si.wShowWindow               = SW_HIDE;  // Prevents cmd window from flashing.
											 // Requires STARTF_USESHOWWINDOW in dwFlags.

	PROCESS_INFORMATION pi       = { 0 };

	wchar_t*            command_w = sys_to_wchar( command );

	BOOL                fSuccess  = CreateProcessW( NULL, command_w, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi );

	free( command_w );

	if ( !fSuccess )
	{
		CloseHandle( hPipeWrite );
		CloseHandle( hPipeRead );
		return false;
	}

	memset( &output, 0, sizeof( str_buf_t ) );

	bool bProcessEnded = false;
	while ( !bProcessEnded )
	{
		// Give some timeslice (50 ms), so we won't waste 100% CPU.
		bProcessEnded = WaitForSingleObject( pi.hProcess, 50 ) == WAIT_OBJECT_0;

		// Even if process exited - we continue reading, if
		// there is some data available over pipe.
		for ( ;; )
		{
			char  buf[ 1024 ];
			DWORD dwRead  = 0;
			DWORD dwAvail = 0;

			if ( !::PeekNamedPipe( hPipeRead, NULL, 0, NULL, &dwAvail, NULL ) )
				break;

			if ( !dwAvail )  // No data available, return
				break;

			if ( !::ReadFile( hPipeRead, buf, MIN( sizeof( buf ) - 1, dwAvail ), &dwRead, NULL ) || !dwRead )
				// Error, the child process might ended
				break;

			buf[ dwRead ] = 0;
			util_append_str( output, buf, strlen( buf ), 2048 );
		}
	}

	util_append_str( output, "\0", 1, 1 );

	CloseHandle( hPipeWrite );
	CloseHandle( hPipeRead );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
	return true;
}


int sys_execute( const char* command )
{
	PROCESS_INFORMATION pi        = { 0 };
	wchar_t*            command_w = sys_to_wchar( command );
	STARTUPINFOW        si        = { sizeof( STARTUPINFOW ) };

	BOOL                success   = CreateProcessW( NULL, command_w, NULL, NULL, TRUE, BELOW_NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi );

	free( command_w );

	if ( !success )
	{
		sys_print_last_error();
		return false;
	}

	WaitForSingleObject( pi.hProcess, INFINITE );

	DWORD exit_code = 0;
	BOOL  ret       = GetExitCodeProcess( pi.hProcess, &exit_code );

	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	return (int)exit_code;
}


void sys_browse_to_file( const char* path )
{
	wchar_t*    path_w = sys_to_wchar( path );

	ITEMIDLIST* pidl   = ILCreateFromPath( path_w );
	if ( pidl )
	{
		SHOpenFolderAndSelectItems( pidl, 0, 0, 0 );
		ILFree( pidl );
	}

	free( path_w );
}


int sys_init()
{
	g_con_out = GetStdHandle( STD_OUTPUT_HANDLE );

	if ( g_con_out == INVALID_HANDLE_VALUE )
	{
		sys_print_last_error();
		return 1;
	}

	return 0;
}


void sys_shutdown()
{
}

