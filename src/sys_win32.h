#pragma once


using module    = void*;


// library loading
module         sys_load_library( const wchar_t* path );
void           sys_close_library( module mod );
void*          sys_load_func( module mod, const char* path );
const char*    sys_get_error();
void           sys_print_last_error();

// also known as "sys_to_utf16"
wchar_t*       sys_to_wchar( const char* spStr, int sSize );
wchar_t*       sys_to_wchar( const char* spStr );

char*          sys_to_utf8( const wchar_t* spStr, int sSize );
char*          sys_to_utf8( const wchar_t* spStr );

// get folder exe is stored in
char*          sys_get_exe_folder( size_t* len = nullptr );

// get the full path of the exe
char*          sys_get_exe_path( size_t* len = nullptr );

// get current working directory
char*          sys_get_cwd();

