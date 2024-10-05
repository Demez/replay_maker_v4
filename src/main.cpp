#include "main.h"

#include <locale.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3_loader.h"
#include "imgui_impl_opengl3.h"

#include "mpv/client.h"
#include "mpv/render_gl.h"

#include "clip/clip.h"

#include <vector>



bool           g_running                = true;

ImGuiContext** g_imgui_contexts         = nullptr;

ivec2          g_mpv_size               = { 0, 0 };
// ivec2          g_imgui_window_size[ 2 ] = { 0, 0 };
ivec2          g_window_size            = { 0, 0 };


//#define TEST_VIDEO L"H:\\videos\\av1_testing\\Replay 2024-07-21 23-10-44.mkv"
//#define TEST_VIDEO L"D:\\projects\\replay_maker_v4\\out\\test.mp4"

// #define TEST_VIDEO L"D:\\usr\\Downloads\\[twitter] Sigida_plushies—2024.08.09—1821928888315814020—hHiapz5PrN8XJ76v.mp4"
// #define TEST_VIDEO_ANSI "D:\\usr\\Downloads\\[twitter] Sigida_plushies—2024.08.09—1821928888315814020—hHiapz5PrN8XJ76v.mp4"

#define TEST_VIDEO L"H:\\videos\\av1_testing\\Replay 2024-06-25 21-11-40.mkv"
#define TEST_VIDEO_ANSI "H:\\videos\\av1_testing\\Replay 2024-06-25 21-11-40.mkv"


// --------------------------------------------------------------------------------------------------


void calc_imgui_window_size( int index, ivec2& size )
{
	switch ( index )
	{
		case 0:
		{
			return calc_playback_window_size( size );
		}
		case 1:
		{
			return calc_replay_window_size( size );
		}
		default:
		{
			size[ 0 ] = 0;
			size[ 1 ] = 0;
		}
	}
}


void calc_playback_window_size( ivec2& size )
{
	size[ 0 ] = MIN( 0, g_window_size[ 0 ] - ( g_mpv_size[ 0 ] + DIVIDER_SIZE ) );
	size[ 1 ] = g_window_size[ 1 ];
}


void calc_replay_window_size( ivec2& size )
{
	size[ 0 ] = MIN( 0, g_window_size[ 0 ] - DIVIDER_SIZE );
	size[ 1 ] = MIN( 0, g_window_size[ 1 ] - ( g_mpv_size[ 1 ] + DIVIDER_SIZE ) );
}


void run_logic()
{
	// TODO: clip trimming system
}


void draw_playback_controls( int size[ 2 ] )
{
	// is there a video playing?

	// time-pos
	double time_pos = 0;
	double duration = 0;
	p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );
	p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );

	// volume

	s32 paused = 0;
	p_mpv_get_property( g_mpv, "pause", MPV_FORMAT_FLAG, &paused );

	// seek bar

	ImGui::Text( "%.4f / %.4f", time_pos, duration );
	// ImGui::ProgressBar( time_pos / duration );

	// can offset the seek bar to the right depending on the current playback time
	// ImGui::SameLine();

	// what if we had a custom seek bar that was snapshots of the video, kind of like the vscode text preview on the scrollbar
	// or have a thumbnail of the frame as a popup when you hover over the seek bar

	float time_pos_f = (float)time_pos;
	if ( ImGui::SliderFloat( "Seek", &time_pos_f, 0.f, (float)duration ) )
	{
		// convert float to string in c
		char time_pos_str[ 16 ];
		gcvt( time_pos_f, 4, time_pos_str );

		const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	double volume = 0;
	p_mpv_get_property( g_mpv, "volume", MPV_FORMAT_DOUBLE, &volume );

	// ImGui::SameLine();

	float volume_f = volume;
	if ( ImGui::SliderFloat( "Volume", &volume_f, 0.f, 130.f ) )
	{
		// convert float to string in c
		char volume_str[ 16 ];
		gcvt( volume_f, 4, volume_str );

		const char* cmd[]   = { "set", "volume", volume_str, NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	if ( paused )
	{
		if ( ImGui::Button( "Play" ) )
		{
			const char* cmd[]   = { "set", "pause", "no", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}
	}
	else
	{
		if ( ImGui::Button( "Pause" ) )
		{
			const char* cmd[]   = { "set", "pause", "yes", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}
	}
}


clip_data_t*         g_clip_data           = nullptr;
clip_output_video_t* g_clip_current_output = nullptr;
u32                  g_clip_current_input  = 0;
char                 g_output_name_buf[ 512 ] = { 0 };

const char*          g_video_input_dir     = "";

static bool          g_view_search_paths   = false;
static float         g_time_range_start    = 0.f;


void draw_replay_info_menu_bar()
{
	ImGui::BeginMenuBar();

	if ( ImGui::BeginMenu( "File" ) )
	{
		if ( ImGui::MenuItem( "New" ) )
		{
			printf( "New\n" );
		}

		if ( ImGui::MenuItem( "Open Timestamps File" ) )
		{
			printf( "Open\n" );
		}

		if ( ImGui::MenuItem( "Open Directory" ) )
		{
			printf( "Open Dir\n" );
		}

		if ( ImGui::MenuItem( "Save" ) )
		{
			printf( "Save\n" );
		}

		if ( ImGui::MenuItem( "Save As" ) )
		{
			printf( "Save As\n" );
		}

		ImGui::EndMenu();
	}

	if ( ImGui::BeginMenu( "View" ) )
	{
		ImGui::MenuItem( "Search Paths", nullptr, &g_view_search_paths );

		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
}


void replay_editor_reset()
{
	g_clip_current_output = nullptr;
	g_clip_current_input  = 0;
	g_time_range_start    = 0.f;
	memset( g_output_name_buf, 0, 512 * sizeof( char ) );
}


bool replay_editor_set_video( clip_output_video_t* output, u32 input_i )
{
	if ( !output )
		return false;

	if ( input_i > output->input_count )
	{
		printf( "invalid input index\n" );
		return false;
	}

	g_clip_current_output     = output;
	g_clip_current_input      = input_i;
	g_time_range_start        = 0.f;

	memcpy( g_output_name_buf, output->name, strlen( output->name ) * sizeof( char ) );
}


void replay_editor_load( clip_output_video_t* output, u32 input_i )
{
	if ( !replay_editor_set_video( output, input_i ) )
		return;

	clip_input_video_t& input = output->input[ input_i ];
	mpv_cmd_loadfile( input.path );
}


void draw_replay_list( int size[ 2 ] )
{
	// display current data
	if ( !g_clip_data )
		return;

	// display search paths
	if ( g_view_search_paths )
	{
		ImGui::TextUnformatted( "Search Paths" );
		for ( u32 i = 0; i < g_clip_data->search_path_count; i++ )
		{
			ImGui::Text( "    %s", g_clip_data->search_path[ i ] );
		}
	}

	// display output videos
	u32 imgui_id = 1;

	for ( u32 i = 0; i < g_clip_data->output_count; i++ )
	{
		//ImGui::Indent( 16.f );

		clip_output_video_t* output = &g_clip_data->output[ i ];

		ImGui::Text( "Output %d:\n%s", i, output->name );

		//ImGui::SameLine();
		//
		//if ( ImGui::Button( "Load" ) )
		//{
		//
		//}

		ImGui::Separator();

		// display input videos
		ImGui::Text( "Input Videos: %d", output->input_count );

		for ( u32 j = 0; j < output->input_count; j++ )
		{
			clip_input_video_t* input = &output->input[ j ];

			ImGui::PushID( imgui_id );
			if ( ImGui::Button( "Load" ) )
			{
				replay_editor_load( output, j );
			}
			ImGui::PopID();

			imgui_id++;

			ImGui::Text( "%d - %s", j, input->path );

			ImGui::Separator();

			ImGui::Indent( 16.f );

			// display input video times
			for ( u32 time_range_i = 0; time_range_i < input->time_range_count; time_range_i++ )
			{
				ImGui::Text( "%d - Time \"%.4f\" - \"%.4f\"", time_range_i, input->time_range[ time_range_i ].start, input->time_range[ time_range_i ].end );
			}

			ImGui::Unindent();
		}

		//ImGui::Unindent();

		ImGui::Separator();
		ImGui::Separator();
	}
}


void draw_replay_info( int size[ 2 ] )
{
	if ( !g_clip_data )
	{
		ImGui::TextUnformatted( "No Clip Data Loaded or Created, use File/New or File/Open" );
		return;
	}

	if ( ImGui::Button( "New Output Video" ) )
	{
		// create a new output video based on the filename of the playing video
		char* current_video = mpv_get_current_video();

		if ( current_video )
		{
			replay_editor_reset();

			clip_output_video_t* output = clip_add_output( g_clip_data, current_video );
			g_clip_current_output       = output;

			if ( output )
			{
				clip_add_input( output, current_video );

				replay_editor_set_video( output, 0 );
			}
		}
	}

	if ( !g_clip_current_output )
		return;

	ImGui::SameLine();

	if ( ImGui::Button( "Add Input Video" ) )
	{

	}

	ImGui::Separator();

	// display output video data
	ImGui::TextUnformatted( "Output Video:" );

	if ( ImGui::InputText( "##output_video_name_edit", g_output_name_buf, 512 ) )
	{
		// lol
		size_t name_len = strlen( g_output_name_buf );

		char* new_data = ch_realloc( g_clip_current_output->name, name_len + 1 );

		if ( new_data )
		{
			g_clip_current_output->name = new_data;
			memcpy( g_clip_current_output->name, g_output_name_buf, name_len * sizeof( char ) );
			g_clip_current_output->name[ name_len ] = '\0';
		}
	}

	ImGui::Separator();

	if ( ImGui::BeginCombo( "Include/Exclude Presets", "" ) )
	{
		for ( u32 i = 0; i < g_clip_data->preset_count; i++ )
		{
			// lmao what the fuck
			bool skip = false;
			for ( u32 used_preset_i = 0; used_preset_i < g_clip_current_output->encode_overrides.presets_count; used_preset_i++ )
			{
				if ( i == used_preset_i )
				{
					skip = true;
					continue;
				}
			}

			if ( skip )
				continue;

			if ( ImGui::Selectable( g_clip_data->preset[ i ].name ) )
			{
				clip_add_preset_to_encode_override( g_clip_data, g_clip_current_output->encode_overrides, i );
			}
		}

		ImGui::EndCombo();
	}

	// display any overrides
	if ( ImGui::Button( g_clip_current_output->encode_overrides.preset_exclude ? "Mode: Exclude" : "Mode: Include" ) )
	{
		g_clip_current_output->encode_overrides.preset_exclude = !g_clip_current_output->encode_overrides.preset_exclude;
	}

	for ( u32 i = 0; i < g_clip_current_output->encode_overrides.presets_count; i++ )
	{
		ImGui::SameLine();

		char button_name[ MAX_LEN_PRESET_NAME + 4 ] = { "(X) " };

		clip_encode_preset_t& encode = g_clip_data->preset[ i ];

		memcpy( &button_name[ 4 ], encode.name, MAX_LEN_PRESET_NAME );

		if ( ImGui::Button( button_name ) )
		{
		}
	}
	
	ImGui::Separator();

	if ( ImGui::Button( "Enter Custom ffmpeg cmd" ) )
	{
	}

	ImGui::Separator();

	// display input videos
	for ( u32 j = 0; j < g_clip_current_output->input_count; j++ )
	{
		clip_input_video_t* input = &g_clip_current_output->input[ j ];

		ImGui::Text( "Input %d:\n%s", j, input->path );

		ImGui::Separator();

		ImGui::Indent( 16.f );

		// display input video times
		if ( input->time_range_count )
		{
			u32  delete_time_range = UINT32_MAX;
			u32  move_time_range   = UINT32_MAX;
			bool move_up           = false;

			for ( u32 time_range_i = 0; time_range_i < input->time_range_count; time_range_i++ )
			{
				if ( ImGui::Button( "X" ) )
				{
					delete_time_range = time_range_i;
				}

				ImGui::SameLine();

				if ( ImGui::Button( "/\\" ) )
				{
					move_time_range = time_range_i;
					move_up         = true;
				}

				ImGui::SameLine();

				if ( ImGui::Button( "\\/" ) )
				{
					move_time_range = time_range_i;
					move_up         = false;
				}

				ImGui::SameLine();
				ImGui::Text( "%d - Time \"%.4f\" - \"%.4f\"", time_range_i, input->time_range[ time_range_i ].start, input->time_range[ time_range_i ].end );
			}

			// does the user want to move a time range?
			if ( move_time_range != UINT32_MAX )
			{
				if ( move_up && move_time_range == 0 )
				{
					continue;
				}

				if ( !move_up && move_time_range == input->time_range_count )
				{
					continue;
				}

				move_time_range = UINT32_MAX;
			}

			// does the user want to delete a time range
			else if ( delete_time_range != UINT32_MAX )
			{
				clip_remove_time_range( g_clip_current_output, g_clip_current_input, delete_time_range );
				delete_time_range = UINT32_MAX;
			}
		}

		ImGui::Unindent();
	}

	// ideas for other stuff to put here:
	// - section for a list of all the videos in the current directory
	// - open the saved video list
	// - maybe something to run the encoder?
	// - section for the current output video
	// - buttons for making an output video

	// for now just show the current video name idfk
	// ImGui::TextUnformatted( TEST_VIDEO );

	ImGui::Separator();

	if ( ImGui::Button( "Set Start Time" ) )
	{
		// get position from mkv
		double time_pos     = 0;
		p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

		g_time_range_start = time_pos;
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Set End Time" ) )
	{
		double time_pos = 0;
		p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

		clip_add_time_range( g_clip_current_output, g_clip_current_input, g_time_range_start, time_pos );
		g_time_range_start = time_pos;
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Reset Start Time" ) )
	{
		g_time_range_start = 0;
	}

	// ImGui::Spacing();
	// ImGui::Spacing();
	// ImGui::Spacing();
	// ImGui::Spacing();
	// 
	// if ( ImGui::Button( "Save Video" ) )
	// {
	// }
	// 
	// ImGui::SameLine();
	// 
	// if ( ImGui::Button( "Delete Video" ) )
	// {
	// }
}


void update_folder_list( const char* new_dir )
{
}


void draw_folder_view( int size[ 2 ] )
{
	// TODO: open a folder and display all video files we find in it
}


// shows ffmpeg encode presets
void draw_preset_editor( int size[ 2 ] )
{
	if ( !g_clip_data )
		return;

	static char preset_name_buf[ MAX_LEN_PRESET_NAME ] = { 0 };

	if ( ImGui::InputText( "Add Encode Preset", preset_name_buf, MAX_LEN_PRESET_NAME ) )
	{
		//clip_encode_preset_t* what = clip_add_encode_preset( g_clip_data, "test", "testaa" );
	}

	// Edit encode preset

	// ImGui::InputText( "File Extension", ext_buf, MAX_LEN_EXT );

	// Display Encode Presets, and select one to edit above this list
	ImGui::Separator();
	ImGui::Text( "%d Encode Presets", g_clip_data->preset_count );
	ImGui::Separator();

	for ( u32 i = 0; i < g_clip_data->preset_count; i++ )
	{
		clip_encode_preset_t& preset = g_clip_data->preset[ i ];

		ImGui::Text( "Name:       %s", preset.name );
		ImGui::Text( "Extension:  %s", preset.ext );
		ImGui::Text( "ffmpeg cmd: %s", preset.ffmpeg_cmd );

		ImGui::Spacing();

		if ( preset.out_folder_append )
			ImGui::Text( "Output Folder Append: %s", preset.out_folder_append );

		if ( preset.out_prefix )
			ImGui::Text( "Output Prefix: %s", preset.out_prefix );

		ImGui::Separator();
	}
}


// change to prefix editor?
void draw_prefix_editor( int size[ 2 ] )
{
	if ( !g_clip_data )
		return;

	static bool in_prefix_adding = false;

	if ( ImGui::Button( "Add Video Prefix" ) || in_prefix_adding )
	{
		in_prefix_adding                                   = true;

		//static char prefix_name_buf[ MAX_LEN_PRESET_NAME ] = { 0 };
		//static char prefix_buf[ 256 ]                      = { 0 };

		static char prefix_name_buf[ MAX_LEN_PRESET_NAME ];
		static char prefix_buf[ 256 ];

		ImGui::Separator();

		ImGui::InputText( "Name", prefix_name_buf, MAX_LEN_PRESET_NAME );
		ImGui::InputText( "Prefix", prefix_buf, 256 );

		if ( ImGui::Button( "Done" ) )
		{
			u32 profile = clip_add_prefix( g_clip_data, prefix_name_buf, prefix_buf );

			memset( prefix_name_buf, 0, MAX_LEN_PRESET_NAME );
			memset( prefix_buf, 0, 256 );
			in_prefix_adding = false;
		}

		ImGui::SameLine();

		if ( ImGui::Button( "Cancel" ) )
		{
			memset( prefix_name_buf, 0, MAX_LEN_PRESET_NAME );
			memset( prefix_buf, 0, 256 );
			in_prefix_adding = false;
		}

		ImGui::Separator();
	}

	ImGui::Separator();

	// Display Prefixes
	ImGui::Text( "%d Prefixes", g_clip_data->prefix_count );

	for ( u32 i = 0; i < g_clip_data->prefix_count; i++ )
	{
		clip_prefix_t& prefix = g_clip_data->prefix[ i ];

		ImGui::Text( "Name:   %s", prefix.name );
		ImGui::Text( "Prefix: %s", prefix.prefix );
		ImGui::Separator();
	}
}


void draw_imgui_window( int window_size[ 2 ] )
{
	//ImGui::ShowDemoWindow();
	//
	//return;

	// draw sidebar
	{
		int element_size[ 2 ] = { 0, 0 };
		element_size[ 0 ]     = window_size[ 0 ] - g_mpv_size[ 0 ];
		element_size[ 1 ]     = window_size[ 1 ];

		ImGui::SetNextWindowSize( { (float)element_size[ 0 ], (float)element_size[ 1 ] } );
		ImGui::SetNextWindowPos( { (float)g_mpv_size[ 0 ], 0.f } );

		// ImGui::ShowDemoWindow();

		if ( !ImGui::Begin( "##Replay Info", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar ) )
		{
			ImGui::End();
			return;
		}
		
		draw_replay_info_menu_bar();
		
		if ( ImGui::BeginTabBar( "##replay_tabs" ) )
		{
			if ( ImGui::BeginTabItem( "Replay Editor" ) )
			{
				draw_replay_info( element_size );
				ImGui::EndTabItem();
			}
			
			if ( ImGui::BeginTabItem( "Clip Entries" ) )
			{
				draw_replay_list( element_size );
				ImGui::EndTabItem();
			}
			
			if ( ImGui::BeginTabItem( "Presets" ) )
			{
				draw_preset_editor( element_size );
				ImGui::EndTabItem();
			}
			
			if ( ImGui::BeginTabItem( "Prefix" ) )
			{
				draw_prefix_editor( element_size );
				ImGui::EndTabItem();
			}
			
			if ( ImGui::BeginTabItem( "Folder View" ) )
			{
				draw_folder_view( element_size );
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
		
		ImGui::End();
	}

	// draw playback controls
	{
		int element_size[ 2 ] = { 0, 0 };
		element_size[ 0 ]     = g_mpv_size[ 0 ] + 1;
		element_size[ 1 ]     = window_size[ 1 ] - g_mpv_size[ 1 ];

		ImGui::SetNextWindowSize( { (float)element_size[ 0 ], (float)element_size[ 1 ] } );
		ImGui::SetNextWindowPos( { 0.f, (float)g_mpv_size[ 1 ] } );

		if ( !ImGui::Begin( "##Playback Controls", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration ) )
		// if ( !ImGui::Begin( "##Playback Controls", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse ) )
		{
			ImGui::End();
			return;
		}

		draw_playback_controls( element_size );

		ImGui::End();
	}
}


auto main( int argc, char* argv[] ) -> int
{
	setlocale( LC_ALL, "en_US.UTF-8" );

	// ------------------------------------------
	// Load MPV First before anything else

	if ( !load_mpv_dll() )
	{
		printf( "load_mpv_dll failed!\n" );
		return 1;
	}

	// ------------------------------------------

	if ( !IMGUI_CHECKVERSION() )
	{
		printf( "Dear ImGui version mismatch!\n" );
		return 1;
	}

	if ( ImGui::CreateContext() == nullptr )
	{
		printf( "ImGui::CreateContext failed!\n" );
		return 1;
	}

	// ------------------------------------------

	SET_INT2( g_window_size, 1600, 900 );

	// calculate the size of the mpv window
	SET_INT2( g_mpv_size, g_window_size[ 0 ] - ( 600 + DIVIDER_SIZE ), g_window_size[ 1 ] - ( 100 + DIVIDER_SIZE ) );

	// calculate the size of the imgui windows
	// SET_INT2( g_imgui_window_size[ 0 ], 100, default_size[ 1 ] );  // playback controls
	// SET_INT2( g_imgui_window_size[ 1 ], 200, default_size[ 1 ] - (100 + DIVIDER_SIZE) );  // replay info

	// 2 imgui windows
	if ( !win32_create_windows( g_window_size[ 0 ], g_window_size[ 1 ] ) )
	{
		printf( "win32_create_windows failed!\n" );
		return 1;
	}

	// ------------------------------------------
	// init imgui, make an sdl window, make the sdl renderer, and hook up imgui to it

	const char* glsl_version    = nullptr;
	// const char* glsl_version    = "#version 130";

	ImGui_ImplWin32_InitForOpenGL( g_main_window );
	ImGui_ImplOpenGL3_Init();

	// load fonts
	// ImGui::GetIO().Fonts->AddFontFromFileTTF( "D:\\projects\\replay_maker_v4\\out\\SourceSans3-Regular.ttf", 16, nullptr );
	ImGui::GetIO().Fonts->AddFontFromFileTTF( "D:\\projects\\replay_maker_v4\\out\\CascadiaCode.ttf", 15, nullptr );

	ImGui_ImplOpenGL3_CreateFontsTexture();

	// ------------------------------------------
	// Startup and Load MPV

	if ( !start_mpv() )
	{
		printf( "start_mpv failed!\n" );
		return 1;
	}

	// ------------------------------------------

	printf( "loaded\n" );

	g_clip_data = clip_create();

	// add search paths
	clip_add_search_path( g_clip_data, "D:\\usr\\Downloads" );
	clip_add_search_path( g_clip_data, "D:\\videos" );

	// Play this file.
	mpv_cmd_loadfile( TEST_VIDEO_ANSI );

	win32_run();

	// ------------------------------------------
	// exit

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	win32_exit();

	// close mpv
	stop_mpv();

	return 0;
}

