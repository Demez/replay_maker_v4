#include "main.h"

#include <locale.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3_loader.h"
#include "imgui_impl_opengl3.h"

#include "mpv/client.h"
#include "mpv/render_gl.h"

#include <vector>


constexpr int IMGUI_WINDOW_COUNT = 2;

bool           g_running          = true;

ImGuiContext** g_imgui_contexts   = nullptr;

int            g_mpv_width        = 800;
int            g_mpv_height       = 600;


//#define TEST_VIDEO L"H:\\videos\\av1_testing\\Replay 2024-07-21 23-10-44.mkv"
//#define TEST_VIDEO L"D:\\projects\\replay_maker_v4\\out\\test.mp4"

#define TEST_VIDEO L"D:\\usr\\Downloads\\[twitter] Sigida_plushies—2024.08.09—1821928888315814020—hHiapz5PrN8XJ76v.mp4"
#define TEST_VIDEO_ANSI "D:\\usr\\Downloads\\[twitter] Sigida_plushies—2024.08.09—1821928888315814020—hHiapz5PrN8XJ76v.mp4"


void run_logic()
{
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


void draw_replay_info( int size[ 2 ] )
{
	ImGui::BeginMenuBar();

	if ( ImGui::BeginMenu( "File" ) )
	{
		if ( ImGui::MenuItem( "New" ) )
		{
			printf( "New\n" );
		}
	
		if ( ImGui::MenuItem( "Open" ) )
		{
			printf( "Open\n" );
		}
	
		if ( ImGui::MenuItem( "Save" ) )
		{
			printf( "Save\n" );
		}
	
		if ( ImGui::MenuItem( "Save As" ) )
		{
			printf( "Save As\n" );
		}
	
		ImGui::Separator();
	
		if ( ImGui::MenuItem( "Exit" ) )
		{
			g_running = false;
		}
	
		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();

	// ideas for other stuff to put here:
	// - section for a list of all the videos in the current directory
	// - open the saved video list
	// - maybe something to run the encoder?
	// - section for the current output video
	// - buttons for making an output video

	// for now just show the current video name idfk
	// ImGui::TextUnformatted( TEST_VIDEO );
}


void draw_imgui_window( int index, int size[ 2 ] )
{
	// playback controls
	if ( index == 0 )
	{
		ImGui::SetNextWindowSize( { (float)size[ 0 ], (float)size[ 1 ] } );
		ImGui::SetNextWindowPos( { 0.f, 0.f } );

		if ( !ImGui::Begin( "##Playback Controls", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration ) )
		{
			ImGui::End();
			return;
		}
		
		draw_playback_controls( size );

		ImGui::End();
	}
	// replay info
	else if ( index == 1 )
	{
		ImGui::SetNextWindowSize( { (float)size[ 0 ], (float)size[ 1 ] } );
		ImGui::SetNextWindowPos( { 0.f, 0.f } );

		if ( !ImGui::Begin( "##Replay Info", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar ) )
		{
			ImGui::End();
			return;
		}

		draw_replay_info( size );

		ImGui::End();
	}
}


int main( int argc, char* argv[] )
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

	// 2 imgui windows
	if ( !win32_create_windows( 1600, 900, IMGUI_WINDOW_COUNT ) )
	{
		printf( "win32_create_windows failed!\n" );
		return 1;
	}

	// ------------------------------------------
	// init imgui, make an sdl window, make the sdl renderer, and hook up imgui to it

	const char* glsl_version    = nullptr;
	// const char* glsl_version    = "#version 130";

	if ( !IMGUI_CHECKVERSION() )
	{
		printf( "Dear ImGui version mismatch!\n" );
		return 1;
	}

	g_imgui_contexts = ( ImGuiContext** )calloc( sizeof( ImGuiContext* ), IMGUI_WINDOW_COUNT );

	for ( int i = 0; i < IMGUI_WINDOW_COUNT; i++ )
	{
		g_imgui_contexts[ i ] = ImGui::CreateContext();

		if ( g_imgui_contexts[i] == nullptr )
		{
			printf( "ImGui::CreateContext failed!\n" );
			return 1;
		}

		ImGui::SetCurrentContext( g_imgui_contexts[ i ] );

		ImGui_ImplWin32_InitForOpenGL( g_imgui_window[ i ] );
		ImGui_ImplOpenGL3_Init();
	}

	// ------------------------------------------
	// Startup and Load MPV

	if ( !start_mpv() )
	{
		printf( "start_mpv failed!\n" );
		return 1;
	}

	// ------------------------------------------

	printf( "loaded\n" );
	printf( "loading video\n" );

	// Play this file.
	mpv_cmd_loadfile( TEST_VIDEO_ANSI );

	win32_run();

	// ------------------------------------------
	// exit

	// ImGui_ImplOpenGL3_Shutdown();
	// ImGui_ImplSDL2_Shutdown();
	// ImGui::DestroyContext();

	// close mpv
	stop_mpv();

	return 0;
}

