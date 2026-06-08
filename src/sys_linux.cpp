#include "main.h"
#include "util.h"

#include <SDL3/SDL_video.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <linux/stat.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <ftw.h>


// ----------------------------------------------------------------------------------------


bool fs_exists( const char* path )
{
	return access( path, 0 ) != -1;
}


bool fs_make_dir( const char* path )
{
	return mkdir( path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) == 0;
}


bool fs_is_dir( const char* path )
{
	// TODO: USE STATX
	struct stat s;

	if ( stat( path, &s ) == 0 )
		return ( s.st_mode & S_IFDIR );

	return false;
}


bool fs_is_file( const char* path )
{
	// TODO: USE STATX
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


// ----------------------------------------------------------------------------------------


int sys_init()
{
	return 0;
}


void sys_shutdown()
{
}


void sys_update()
{
}


void sys_set_window( SDL_Window* window )
{
}


void sys_set_main_hwnd( void* hwnd )
{
}


// ----------------------------------------------------------------------------------------
// Library Loading


module_t sys_load_library( const char* path )
{
	return (module_t)dlopen( path, RTLD_NOW | RTLD_GLOBAL );
}


void sys_close_library( module_t mod )
{
	dlclose( mod );
}


void* sys_load_func( module_t mod, const char* name )
{
	return dlsym( mod, name );
}


// ----------------------------------------------------------------------------------------
// System Errors


char* sys_get_error_internal()
{
	static char output[ 256 ];
	memset( output, 0,256 ) ;
	snprintf( output, 256, "Error: %d - %s", errno, strerror( errno ) );

	return output;
}


char* sys_get_error()
{
	return util_strdup( sys_get_error_internal() );
}


void sys_print_last_error()
{
	fprintf( stderr, "Error: %s\n", sys_get_error_internal() );
}


// --------------------------------------------------------------------------------------------------------
// Filesystem


char* sys_get_exe_folder( size_t* len )
{
	char output[ 4096 ];
	size_t ret = readlink( "/proc/self/exe", output, 4096 );

	if ( ret <= 0 )
	{
		printf("Failed to get exe folder!\n" );
		sys_print_last_error();
		return util_strdup( "" );
	}

	// find index of last path separator
	char*  sep    = strrchr( output, '/' );
	size_t path_i = sep - output;

	if ( len )
		*len = path_i;

	return util_strndup( output, path_i );
}


char* sys_get_exe_path( size_t* len )
{
	char output[ 4096 ];
	size_t ret = readlink( "/proc/self/exe", output, 4096 );

	if ( ret <= 0 )
	{
		printf("Failed to get exe folder!\n" );
		sys_print_last_error();
		return util_strdup( "" );
	}

	if ( len )
		*len = strlen( output );

	return util_strdup( output );
}


char* sys_get_cwd()
{
	return getcwd( nullptr, 0 );
}

bool sys_get_file_times_and_size( const char* path, u64* creation, u64* access, u64* write, u64* size )
{
	if ( !path )
		return false;

	// TODO: USE STATX
	struct stat s{};
	if ( stat( path, &s ) != 0 )
		return false;

	if ( creation )
		*creation = s.st_ctime;

	if ( write )
		*write = s.st_mtime;

	if ( access )
		*access = s.st_atime;

	if ( size )
		*size = s.st_size;

	return true;
}


bool sys_set_file_times( const char* path, u64* creation, u64* access, u64* write )
{
	return false;
}


bool sys_get_drives( std::vector< std::string >& drives )
{
	return true;
}

#if 0
static void set_file_attributes( file_t &file, const dirent64 &ent, const struct statx &s )
{
	if ( S_ISDIR( s.stx_mode ) )
	{
		file.type |= e_file_type_directory;
		file.size = 0;
	}
	else if ( S_ISREG( s.stx_mode ) )
	{
		file.type |= e_file_type_file;
		file.size = s.stx_size;
	}

	file.type |= ( ent.d_type == DT_LNK ) ? e_file_type_system_link : 0;

	file.date_created = s.stx_btime.tv_sec;
	file.date_mod     = s.stx_mtime.tv_sec;
}


// TODO: look at getdents64()?
bool sys_scandir( const char* root, const char* path, std::vector< file_t >& files, e_scandir_flags flags )
{
	std::string scan_dir = root;

	if ( path )
	{
		scan_dir += SEP_S;
		scan_dir += path;
	}

	scan_dir += SEP_S;

	DIR* dir = opendir( scan_dir.c_str() );

	if ( !dir )
	{
		printf( "Failed to open directory: \"%s\"\n", scan_dir.c_str() );
		return false;
	}

	dirent64* ent;
	while ( ( ent = readdir64( dir ) ) != nullptr )
	{
		if ( ent->d_type == DT_DIR )
		{
			if ( strcmp( ent->d_name, "." ) == 0 || strcmp( ent->d_name, ".." ) == 0 )
				continue;
		}

		std::string relative_path;

		if ( path )
		{
			relative_path += path;
			relative_path += SEP_S;
		}

		relative_path += ent->d_name;

		if ( ent->d_type == DT_DIR )
		{
			if ( flags & e_scandir_recursive )
				sys_scandir( root, relative_path.data(), files, flags );

			if ( flags & e_scandir_no_dirs )
				continue;
		}

		if ( ( ent->d_type != DT_DIR ) && flags & e_scandir_no_files )
		{
			continue;
		}

		file_t file{};

		std::string abs_path = scan_dir + ent->d_name;

		if ( flags & e_scandir_abs_paths )
			file.path = abs_path;
		else
			file.path = relative_path;

		struct statx s{};

		const int statx_mask = STATX_TYPE | STATX_BTIME | STATX_MTIME | STATX_SIZE;
		if ( statx( 0, abs_path.c_str(), 0, statx_mask, &s ) != 0 )
		{
			printf( "Call to statx() failed on %s\n", abs_path.c_str() );
			continue;
		}

		set_file_attributes(file, *ent, s);

		files.push_back( file );
	}

	closedir( dir );
	return true;
}
#endif


// --------------------------------------------------------------------------------------------------------
// Shell Functions


bool sys_recycle_file( const char* path )
{
	// TODO: move file to ~/.local/share/trash

	// Check if we have gio
	int gio_check = sys_execute( "gio" );

	if ( gio_check == 0 )
	{
		char buffer[ 1024 ]{};
		snprintf( buffer, 1024, "gio trash %s", path );
		sys_execute( buffer );
		return true;
	}

	return false;
}


#if 0
void sys_open_file_properties( const std::vector< fs::path >& files )
{
	// not possible to implement on linux
	// dolphin doesn't expose a way to open file properties
}


// --------------------------------------------------------------------------------------------------------

const void* sdl_clipboard_callback( void *userdata, const char *mime_type, size_t *size )
{
	char* data = (char*)userdata;
	*size      = strlen( data );
	return SDL_strdup( data );
}


bool sys_copy_to_clipboard( const std::vector< fs::path >& files )
{
	if ( files.empty() )
		return false;

	// TODO: Copy all files, not just the first one
	std::string path      = files[ 0 ].string();

	const char* mime_type = "text/uri-list";

	static char buffer[ 1024 ]{};
	memset( buffer, 0, 1024 );
	snprintf( buffer, 1024, "file://%s", path.c_str() );

	if ( SDL_SetClipboardData( sdl_clipboard_callback, nullptr, buffer, &mime_type, 1 ) )
		return true;

	printf( "Failed to copy to clipboard!\n" );
	return false;
}


// simpiler version of sys_browse_to_files, one file or folder
void sys_browse_to_path( const fs::path& path )
{
	if ( path.empty() )
		return;

	// Check if we have xdg-open
	int xdg_check = sys_execute( "xdg-open" );

	if ( xdg_check == 0 )
	{
		char buffer[ 1024 ]{};
		snprintf( buffer, 1024, "xdg-open %s", path.c_str() );
		sys_execute( buffer );
		return;
	}

	// Check if we have gio
	int gio_check = sys_execute( "gio" );

	if ( gio_check == 0 )
	{
		char buffer[ 1024 ]{};
		snprintf( buffer, 1024, "gio open %s", path.c_str() );
		sys_execute( buffer );
		return;
	}

	printf( "Failed to browse to file, could not find terminal tools xdg-open or gio!\n" );
}


void sys_browse_to_files( const fs::path& root, const std::vector< fs::path > paths )
{
}
#endif


void sys_browse_to_file( const char* path )
{
	if ( !path )
		return;

	// Check if we have xdg-open
	int xdg_check = sys_execute( "xdg-open" );

	if ( xdg_check == 0 )
	{
		char buffer[ 1024 ]{};
		snprintf( buffer, 1024, "xdg-open %s", path );
		sys_execute( buffer );
		return;
	}

	// Check if we have gio
	int gio_check = sys_execute( "gio" );

	if ( gio_check == 0 )
	{
		char buffer[ 1024 ]{};
		snprintf( buffer, 1024, "gio open %s", path );
		sys_execute( buffer );
		return;
	}

	printf( "Failed to browse to file, could not find terminal tools xdg-open or gio!\n" );
}


void sys_open_folder( const char* path )
{
}


// --------------------------------------------------------------------------------------------------------
// Terminal


// https://stackoverflow.com/a/646254
bool sys_execute_read( const char* command, str_buf_t& output )
{
	FILE* fp = popen( command, "r" );

	if ( fp == nullptr )
	{
		printf("Failed to run command: %s\n", command );
		return false;
	}

	char buffer[ 4096 ]{};
	while (fgets( buffer, sizeof( buffer ), fp ) != nullptr )
	{
		// output.append( buffer );
		util_append_str( output, buffer, strlen( buffer ), 2048 );
	}

	pclose( fp );
	return true;
}


bool sys_execute_read_callback( const char* command, str_buf_t& output, f_exec_callback* p_exec_callback )
{
	if ( !p_exec_callback )
		return false;

	return false;
}


int sys_execute( const char* command )
{
	return std::system( command );
}


// --------------------------------------------------------------------------------------------------------
// Folder Monitor

// Return true if something in the folder changed, indicating we need a refresh
bool sys_folder_mon_changed()
{
	return false;
}


void sys_folder_mon_shutdown()
{
}


// --------------------------------------------------------------------------------------------------------
// Other

#if 0
sys_font_data_t sys_get_font()
{
	sys_font_data_t font_data{};

	// TODO: use fontconfig.h instead
	// Check if we have fc-match
	int ret = sys_execute( "fc-match" );

	if ( ret != 0 )
	{
		printf("Failed to find fc-match for default font!\n" );
		return font_data;
	}

	std::string output;
	sys_execute_read( "fc-match -b", output );

	const char* file      = strstr( output.c_str(), "file: " );
	//const char* pixelsize = strstr( output.c_str(), "pixelsize: " );

	if ( file )
	{
		const char* file_end = strchr( file, '\n' );
		std::string file_string( file + 7, ( ( file_end - 4 ) - ( file + 7 ) ) );
		font_data.font_path = util_strndup( file_string.c_str(), file_string.size() );
	}

	//if ( pixelsize )
	//{
	//	const char* line_end = strchr( pixelsize, '\n' );
	//	std::string pixelsize_str( pixelsize + 11, ( ( line_end - 6 ) - ( pixelsize + 11 ) ) );
	//	// font_data.font_path = util_strndup( pixelsize.c_str(), pixelsize.size() );
	//	font_data.height = atof( pixelsize_str.c_str() );
	//}
	//else
	{
		font_data.height = 17;
	}

	return font_data;
}


proc_mem_info_t sys_get_mem_info()
{
	proc_mem_info_t mem_info{};

	rusage usage{};
	int ret = getrusage( RUSAGE_SELF, &usage );

	if ( ret == 0 )
	{
		mem_info.working_set = usage.ru_maxrss * (long)MEM_SCALE;
	}
	else
	{
		printf("Failed to get memory usage - %d!\n", ret );
		mem_info.working_set = 0;
	}

	mem_info.page_file = 0;

	return mem_info;
}
#endif

u64 sys_get_time_ms()
{
	struct timespec ts{};
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (uint64_t)( ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL );
}

#if 0
// Start drag and drop of multiple files in the system shell, like dragging to another folder to copy, into discord, etc.
void sys_do_drag_drop_files( const std::vector< fs::path >& files, u32 sdl_mouse_btn )
{
}


std::string sys_path_to_string( const fs::path& path )
{
	return path.native();
}


fs::path sys_string_to_path( const std::string& path_str )
{
	fs::path path = path_str;
	return path;
}
#endif
