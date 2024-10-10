#include "main.h"

#include <locale.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3_loader.h"
#include "imgui_impl_opengl3.h"

#include "mpv/client.h"
#include "mpv/render_gl.h"

#include "clip/clip.h"

// native file dialog
#include "nfd.h"

#include <vector>


bool                g_running     = true;

ivec2               g_mpv_size    = { 0, 0 };
ivec2               g_window_size = { 0, 0 };

extern clip_data_t* g_clip_data;

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

	ImGuiStyle&  style           = ImGui::GetStyle();

	const ImVec2 seek_text_size  = ImGui::CalcTextSize( "Seek", NULL, true );
	const ImVec2 vol_text_size   = ImGui::CalcTextSize( "Volume", NULL, true );

	float        avaliable_width = size[ 0 ] - ( style.ItemSpacing.x * 2 );
	float        vol_bar_width   = vol_text_size.x * 3;
	float        seek_bar_width  = ( avaliable_width - seek_text_size.x ) - ( vol_bar_width + vol_text_size.x + ( style.ItemSpacing.x * 2 ) );

	ImGui::SetNextItemWidth( seek_bar_width );

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

	ImGui::SameLine();
	ImGui::SetNextItemWidth( vol_bar_width );

	float volume_f = volume;
	if ( ImGui::SliderFloat( "Volume", &volume_f, 0.f, 130.f ) )
	{
		// convert float to string in c
		char volume_str[ 16 ];
		gcvt( volume_f, 4, volume_str );

		const char* cmd[]   = { "set", "volume", volume_str, NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	const ImVec2     label_size    = ImGui::CalcTextSize( "Pause", NULL, true );
	ImVec2           play_btn_size = ImGui::CalcItemSize( { 0, 0 }, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f );

	if ( paused )
	{
		if ( ImGui::Button( "Play", play_btn_size ) )
		{
			const char* cmd[]   = { "set", "pause", "no", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}
	}
	else
	{
		if ( ImGui::Button( "Pause", play_btn_size ) )
		{
			const char* cmd[]   = { "set", "pause", "yes", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}
	}

	ImGui::SameLine();
	ImGui::Spacing();

	ImGui::SameLine();
	if ( ImGui::Button( "<|" ) )
	{
		const char* cmd[]   = { "seek", "0", "absolute", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	ImGui::SameLine();
	if ( ImGui::Button( "|>" ) )
	{
		char duration_str[ 16 ];
		gcvt( duration, 4, duration_str );

		// const char* cmd[]   = { "seek", duration_str, "absolute", NULL };
		const char* cmd[]   = { "seek", "100", "absolute-percent+exact", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	ImGui::SameLine();
	ImGui::Spacing();

	ImGui::SameLine();
	if ( ImGui::Button( "<" ) )
	{
		const char* cmd[]   = { "frame-back-step", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}

	ImGui::SameLine();
	if ( ImGui::Button( ">" ) )
	{
		const char* cmd[]   = { "frame-step", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
	}
}


void draw_imgui_window( int window_size[ 2 ] )
{
	//ImGui::ShowDemoWindow();
	//return;

	// draw sidebar
	{
		draw_replay_editor_window( window_size );
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
	// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/setlocale-wsetlocale?view=msvc-170#utf-8-support
	// allows c ansi functions to use utf-8
	// only works on Windows 10 version 1803 (10.0.17134.0) and above
	setlocale( LC_ALL, ".UTF-8" );

	if ( NFD_Init() != NFD_OKAY )
	{
		printf( "Failed to Init NativeFileDialog\n" );
		return 1;
	}

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

	// calculate the size of the mpv window (what about DPI Scale here later?)
	g_mpv_size[ 0 ] = g_window_size[ 0 ] - 600;  // replay editor/sidebar
	g_mpv_size[ 1 ] = g_window_size[ 1 ] - 82;   // playback controls

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

	{
		size_t exe_dir_len           = 0;
		char*  exe_dir               = sys_get_exe_folder( &exe_dir_len );
		char   settings_path[ 4096 ] = { 0 };

		memcpy( settings_path, exe_dir, exe_dir_len * sizeof( char ) );
		strcat( settings_path, PATH_SEP_STR "replay_maker_config.json5" );

		clip_parse_settings( g_clip_data, settings_path );
	}

	{
		size_t exe_dir_len           = 0;
		char*  exe_dir               = sys_get_exe_folder( &exe_dir_len );
		char   settings_path[ 4096 ] = { 0 };

		memcpy( settings_path, exe_dir, exe_dir_len * sizeof( char ) );
		strcat( settings_path, PATH_SEP_STR "test_video.json5" );

		clip_parse_videos( g_clip_data, settings_path );
	}

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

