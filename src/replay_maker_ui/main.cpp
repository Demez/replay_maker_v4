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

#include <time.h>


bool                g_running      = true;

ivec2               g_mpv_size     = { 0, 0 };
ivec2               g_window_size  = { 0, 0 };
bool                g_show_sidebar = true;

extern clip_data_t* g_clip_data;

char*               g_videos_file_path;

video_media_info_t  g_video_media_info;

//#define TEST_VIDEO L"H:\\videos\\av1_testing\\Replay 2024-07-21 23-10-44.mkv"
//#define TEST_VIDEO L"D:\\projects\\replay_maker_v4\\out\\test.mp4"

// #define TEST_VIDEO L"D:\\usr\\Downloads\\[twitter] Sigida_plushies—2024.08.09—1821928888315814020—hHiapz5PrN8XJ76v.mp4"
// #define TEST_VIDEO_ANSI "D:\\usr\\Downloads\\[twitter] Sigida_plushies—2024.08.09—1821928888315814020—hHiapz5PrN8XJ76v.mp4"

#define TEST_VIDEO L"H:\\videos\\av1_testing\\Replay 2024-06-25 21-11-40.mkv"
#define TEST_VIDEO_ANSI "H:\\videos\\av1_testing\\Replay 2024-06-25 21-11-40.mkv"

//#define TEST_VIDEO L"D:\\projects\\replay_maker_v4_window_test\\build\\output4\\raw\\test_max_path\\testingtestingtestingtestingtestingtesting\\raw_gmod_d_test_ball_flip_video__raw_gmod_d_test_ball_flip_video__raw_gmod_d_test_ball_flip_video__raw_gmod_d_test_ball_flip_video____raw_gmod_d_test_ball_flip_video____ra.mkv"
//#define TEST_VIDEO_ANSI "D:\\projects\\replay_maker_v4_window_test\\build\\output4\\raw\\test_max_path\\testingtestingtestingtestingtestingtesting\\raw_gmod_d_test_ball_flip_video__raw_gmod_d_test_ball_flip_video__raw_gmod_d_test_ball_flip_video__raw_gmod_d_test_ball_flip_video____raw_gmod_d_test_ball_flip_video____ra.mkv"



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


void get_media_info()
{
	if ( !mpv_get_current_video() )
		return;

	memset( &g_video_media_info, 0, sizeof( video_media_info_t ) );
	
	mpv_error ret = (mpv_error)p_mpv_get_property( g_mpv, "track-list/count", MPV_FORMAT_INT64, &g_video_media_info.track_count );

	for ( s32 i = 0; i < g_video_media_info.track_count; i++ )
	{
		char cmd[ 64 ] = { 0 };
		snprintf( cmd, 64, "track-list/%d/type", i );

		char* type = nullptr;
		ret = (mpv_error)p_mpv_get_property( g_mpv, cmd, MPV_FORMAT_STRING, &type );

		if ( !type )
			continue;

		if ( strcmp( type, "video" ) == 0 )
			g_video_media_info.track_count_video++;

		else if ( strcmp( type, "audio" ) == 0 )
			g_video_media_info.track_count_audio++;
	}
}


void draw_playback_controls( int size[ 2 ], bool draw_volume )
{
	// is there a video playing?
	
	// HACK
	if ( g_video_media_info.track_count == 0 )
		get_media_info();

	// time-pos
	double time_pos = 0;
	double duration = 0;
	p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );
	p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );

	// volume

	s32 paused = 0;
	p_mpv_get_property( g_mpv, "pause", MPV_FORMAT_FLAG, &paused );

	// seek bar

	// https://stackoverflow.com/questions/3673226/how-to-print-time-in-format-2009-08-10-181754-811

	char str_time_pos[ TIME_BUFFER ]{ 0 };
	char str_duration[ TIME_BUFFER ]{ 0 };

	util_format_time( str_time_pos, time_pos );
	util_format_time( str_duration, duration );

	ImGui::Text( "%s / %s |", str_time_pos, str_duration );
	// ImGui::ProgressBar( time_pos / duration );

	ImGui::SameLine();
	if ( mpv_get_current_video() )
	{
		ImGui::TextUnformatted( mpv_get_current_video() );
	}
	else
	{
		ImGui::TextUnformatted( "[No Video Loaded]" );
	}

	// draw audio track name
	// audio track test
	//char* audio_track       = 0;
	char*        audio_track     = 0;
	// mpv_error audio_ret         = (mpv_error)p_mpv_get_property( g_mpv, "audio", MPV_FORMAT_NONE, &audio_track );
	mpv_error    audio_ret       = (mpv_error)p_mpv_get_property( g_mpv, "audio", MPV_FORMAT_STRING, &audio_track );
	// mpv_error audio_ret         = (mpv_error)p_mpv_get_property( g_mpv, "track-list/audio/id", MPV_FORMAT_STRING, &audio_track );

	char         track_name_buf[ 32 ] = { 0 };
	// snprintf( track_name_buf, 32, "track-list/%s/title", audio_track );

	char*        audio_track_name     = 0;
	audio_ret                         = (mpv_error)p_mpv_get_property( g_mpv, "current-tracks/audio/title", MPV_FORMAT_STRING, &audio_track_name );

	ImGui::SameLine();
	ImGui::Text( "| Audio: %s", audio_track_name );

	// can offset the seek bar to the right depending on the current playback time
	// ImGui::SameLine();

	// what if we had a custom seek bar that was snapshots of the video, kind of like the vscode text preview on the scrollbar
	// or have a thumbnail of the frame as a popup when you hover over the seek bar

	ImGuiStyle&  style           = ImGui::GetStyle();

	const ImVec2 seek_text_size  = ImGui::CalcTextSize( "Seek", NULL, true );
	const ImVec2 vol_text_size   = ImGui::CalcTextSize( "Volume", NULL, true );

	float        avaliable_width = size[ 0 ] - ( style.ItemSpacing.x * 2 );
	float        vol_bar_width   = vol_text_size.x * 3;
	float        seek_bar_width  = ( avaliable_width - seek_text_size.x );

	if ( draw_volume )
		seek_bar_width -= ( vol_bar_width + vol_text_size.x + ( style.ItemSpacing.x * 2 ) );

	ImGui::SetNextItemWidth( seek_bar_width );

	float time_pos_f = (float)time_pos;
	if ( ImGui::SliderFloat( "Seek", &time_pos_f, 0.f, (float)duration ) )
	{
		// convert float to string in c
		char time_pos_str[ 16 ];
		gcvt( time_pos_f, 4, time_pos_str );

		const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		printf( "seek - %d\n", cmd_ret );
	}

	if ( draw_volume )
	{
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
	}

	const ImVec2     label_size    = ImGui::CalcTextSize( "Pause", NULL, true );
	ImVec2           play_btn_size = ImGui::CalcItemSize( { 0, 0 }, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f );

	if ( paused )
	{
		if ( ImGui::Button( "Play", play_btn_size ) )
		{
			const char* cmd[]   = { "set", "pause", "no", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
			printf( "play- %d\n", cmd_ret );
		}
	}
	else
	{
		if ( ImGui::Button( "Pause", play_btn_size ) )
		{
			const char* cmd[]   = { "set", "pause", "yes", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
			printf( "pause- %d\n", cmd_ret );
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

	// TODO: add speed controls here

	if ( !draw_volume )
		return;

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	// audio track selection
	char audio_btn[ 16 ] = { 0 };

	if ( audio_track )
	{
		/*if ( strcmp( audio_track, "auto" ) == 0 )
		{
			snprintf( audio_btn, 16, "Audio: auto/%d", g_video_media_info.track_count_audio );
		}
		else*/ if ( strcmp( audio_track, "no" ) == 0 )
		{
			snprintf( audio_btn, 16, "Audio: -/%d", g_video_media_info.track_count_audio );
		}
		else
		{
			snprintf( audio_btn, 16, "Audio: %s/%d", audio_track, g_video_media_info.track_count_audio );
		}
	}

	char   temp_test[ 16 ] = { 0 };
	snprintf( temp_test, 16, "Audio: auto/%d", g_video_media_info.track_count_audio );
	ImVec2 audio_btn_text = ImGui::CalcTextSize( temp_test );

	audio_btn_text.x += style.ItemInnerSpacing.x * 2;
	audio_btn_text.y += style.ItemInnerSpacing.y * 2;

	if ( ImGui::Button( audio_btn, audio_btn_text ) )
	{
		const char* cmd[]   = { "cycle", "audio", NULL };
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		printf( "cycle audio ret - %d\n", cmd_ret );
	}

	ImGui::SameLine();

	// try to get the audio track (might be odd with muscle memory)

	if ( ImGui::Button( "Open Folder" ) )
	{
		sys_browse_to_file( mpv_get_current_video() );
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Toggle Sidebar" ) )
	{
		enable_sidebar( !g_show_sidebar );
	}

	ImGui::SameLine();

	ImGui::BeginDisabled( true );

	if ( ImGui::Button( "Take Screenshot" ) )
	{
	}

	ImGui::EndDisabled();
}


void draw_imgui_window( int window_size[ 2 ] )
{
	//ImGui::ShowDemoWindow();
	//return;

	// draw sidebar
	if ( g_show_sidebar )
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

		draw_playback_controls( element_size, true );

		ImGui::End();
	}
}


void save_settings()
{
	size_t exe_dir_len = 0;
	char*  exe_dir     = sys_get_exe_folder( &exe_dir_len );
	char settings_path[ 4096 ] = { 0 };

	memcpy( settings_path, exe_dir, exe_dir_len * sizeof( char ) );
	strcat( settings_path, PATH_SEP_STR "replay_maker_config.json5" );

	clip_save_settings( g_clip_data, settings_path );

	free( exe_dir );
}


void save_videos()
{
	clip_save_videos( g_clip_data, g_videos_file_path );
}


void write_recently_opened()
{
#if 1
	char recent[ 4096 ] = { 0 };

	for ( u8 i = 0; i < g_recently_opened_count; i++ )
	{
		strcat( recent, g_recently_opened[ i ] );
		strcat( recent, "\n");
	}

	fs_save_file( g_recently_opened_path, recent, strlen( recent ) );
#else
	FILE* fp = fopen( g_recently_opened_path, "w" );

	if ( !fp )
	{
		printf( "Failed to write recently opened file!\n" );
		return;
	}

	for ( u8 i = 0; i < g_recently_opened_count; i++ )
	{
		fwrite( g_recently_opened[ i ], strlen( g_recently_opened[ i ] ) * sizeof( char ), 1, fp );
		fwrite( "\n", sizeof( char ), 1, fp );
	}

	fclose( fp );
#endif
}


void load_recently_opened()
{
	if ( !fs_is_file( g_recently_opened_path ) )
		return;

	size_t len             = 0;
	char*  recently_opened = fs_read_file( g_recently_opened_path, &len );

	// find out how many entries are here
	char*  line_end        = strchr( recently_opened, '\n' );
	char*  line_start      = recently_opened;

	while ( line_end )
	{
		if ( *( line_end - 1 ) == '\r' )
			line_end--;

		size_t dist = line_end - line_start;
		char*  file = ch_malloc< char >( dist + 1 );
		memcpy( file, line_start, dist * sizeof( char ) );
		file[ dist ] = '\0';

		g_recently_opened[ g_recently_opened_count++ ] = file;

		if ( *line_end == '\r' )
			line_end++;

		line_start = ++line_end;
		line_end   = strchr( line_start, '\n' );
	}

	free( recently_opened );
}


void update_recently_opened( const char* clips_file )
{
	// this could have come from in the list itself
	char* temp_dup = strdup( clips_file );

	// shift everything back
	if ( g_recently_opened_count > 0 )
	{
		// check to see if this is the top most file
		if ( strcmp( clips_file, g_recently_opened[ 0 ] ) == 0 )
		{
			free( temp_dup );
			return;
		}

		// check to see if this file was opened before
		bool found = false;
		for ( u8 i = 0; i < g_recently_opened_count; i++ )
		{
			// TODO: already exists here, reuse that pointer
			if ( strcmp( clips_file, g_recently_opened[ i ] ) == 0 )
			{
				// g_recently_opened[ 0 ] = g_recently_opened[ i ];
				free( g_recently_opened[ i ] );
				memcpy( &g_recently_opened[ i ], &g_recently_opened[ i + 1 ], sizeof( char* ) * ( ( MAX_RECENT_OPEN - 1 ) - i ) );
				found = true;
				break;
			}
		}

		if ( !found )
		{
			if ( g_recently_opened_count == MAX_RECENT_OPEN )
				free( g_recently_opened[ MAX_RECENT_OPEN - 1 ] );
			else
				g_recently_opened_count++;
		}

		// shift everything down by one
		memcpy( g_recently_opened + 1, g_recently_opened, ( MAX_RECENT_OPEN - 1 ) * sizeof( char* ) );
		memset( &g_recently_opened[ 0 ], 0, sizeof( char* ) );
	}
	else
	{
		if ( g_recently_opened_count < MAX_RECENT_OPEN )
			g_recently_opened_count++;
	}

	g_recently_opened[ 0 ] = temp_dup;

	write_recently_opened();
}


void imgui_set_theme_steam_green()
{
	// Classic VGUI2 Style Color Scheme
	ImVec4* colors                           = ImGui::GetStyle().Colors;

	colors[ ImGuiCol_Text ]                  = ImVec4( 1.00f, 1.00f, 1.00f, 1.00f );
	colors[ ImGuiCol_TextDisabled ]          = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
	colors[ ImGuiCol_WindowBg ]              = ImVec4( 0.29f, 0.34f, 0.26f, 1.00f );
	colors[ ImGuiCol_ChildBg ]               = ImVec4( 0.29f, 0.34f, 0.26f, 1.00f );
	colors[ ImGuiCol_PopupBg ]               = ImVec4( 0.24f, 0.27f, 0.20f, 1.00f );
	colors[ ImGuiCol_Border ]                = ImVec4( 0.54f, 0.57f, 0.51f, 0.50f );
	colors[ ImGuiCol_BorderShadow ]          = ImVec4( 0.14f, 0.16f, 0.11f, 0.52f );
	colors[ ImGuiCol_FrameBg ]               = ImVec4( 0.24f, 0.27f, 0.20f, 1.00f );
	colors[ ImGuiCol_FrameBgHovered ]        = ImVec4( 0.27f, 0.30f, 0.23f, 1.00f );
	colors[ ImGuiCol_FrameBgActive ]         = ImVec4( 0.30f, 0.34f, 0.26f, 1.00f );
	colors[ ImGuiCol_TitleBg ]               = ImVec4( 0.24f, 0.27f, 0.20f, 1.00f );
	colors[ ImGuiCol_TitleBgActive ]         = ImVec4( 0.29f, 0.34f, 0.26f, 1.00f );
	colors[ ImGuiCol_TitleBgCollapsed ]      = ImVec4( 0.00f, 0.00f, 0.00f, 0.51f );
	colors[ ImGuiCol_MenuBarBg ]             = ImVec4( 0.24f, 0.27f, 0.20f, 1.00f );
	colors[ ImGuiCol_ScrollbarBg ]           = ImVec4( 0.35f, 0.42f, 0.31f, 1.00f );
	colors[ ImGuiCol_ScrollbarGrab ]         = ImVec4( 0.28f, 0.32f, 0.24f, 1.00f );
	colors[ ImGuiCol_ScrollbarGrabHovered ]  = ImVec4( 0.25f, 0.30f, 0.22f, 1.00f );
	colors[ ImGuiCol_ScrollbarGrabActive ]   = ImVec4( 0.23f, 0.27f, 0.21f, 1.00f );
	colors[ ImGuiCol_CheckMark ]             = ImVec4( 0.59f, 0.54f, 0.18f, 1.00f );
	colors[ ImGuiCol_SliderGrab ]            = ImVec4( 0.35f, 0.42f, 0.31f, 1.00f );
	colors[ ImGuiCol_SliderGrabActive ]      = ImVec4( 0.54f, 0.57f, 0.51f, 0.50f );
	colors[ ImGuiCol_Button ]                = ImVec4( 0.29f, 0.34f, 0.26f, 0.40f );
	colors[ ImGuiCol_ButtonHovered ]         = ImVec4( 0.35f, 0.42f, 0.31f, 1.00f );
	colors[ ImGuiCol_ButtonActive ]          = ImVec4( 0.54f, 0.57f, 0.51f, 0.50f );
	colors[ ImGuiCol_Header ]                = ImVec4( 0.35f, 0.42f, 0.31f, 1.00f );
	colors[ ImGuiCol_HeaderHovered ]         = ImVec4( 0.35f, 0.42f, 0.31f, 0.6f );
	colors[ ImGuiCol_HeaderActive ]          = ImVec4( 0.54f, 0.57f, 0.51f, 0.50f );
	colors[ ImGuiCol_Separator ]             = ImVec4( 0.14f, 0.16f, 0.11f, 1.00f );
	colors[ ImGuiCol_SeparatorHovered ]      = ImVec4( 0.54f, 0.57f, 0.51f, 1.00f );
	colors[ ImGuiCol_SeparatorActive ]       = ImVec4( 0.59f, 0.54f, 0.18f, 1.00f );
	colors[ ImGuiCol_ResizeGrip ]            = ImVec4( 0.19f, 0.23f, 0.18f, 0.00f );  // grip invis
	colors[ ImGuiCol_ResizeGripHovered ]     = ImVec4( 0.54f, 0.57f, 0.51f, 1.00f );
	colors[ ImGuiCol_ResizeGripActive ]      = ImVec4( 0.59f, 0.54f, 0.18f, 1.00f );
	colors[ ImGuiCol_Tab ]                   = ImVec4( 0.35f, 0.42f, 0.31f, 1.00f );
	colors[ ImGuiCol_TabHovered ]            = ImVec4( 0.54f, 0.57f, 0.51f, 0.78f );
	colors[ ImGuiCol_TabActive ]             = ImVec4( 0.59f, 0.54f, 0.18f, 1.00f );
	colors[ ImGuiCol_TabUnfocused ]          = ImVec4( 0.24f, 0.27f, 0.20f, 1.00f );
	colors[ ImGuiCol_TabUnfocusedActive ]    = ImVec4( 0.35f, 0.42f, 0.31f, 1.00f );
	//colors[ImGuiCol_DockingPreview]          = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
	//colors[ImGuiCol_DockingEmptyBg]          = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ ImGuiCol_PlotLines ]             = ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
	colors[ ImGuiCol_PlotLinesHovered ]      = ImVec4( 0.59f, 0.54f, 0.18f, 1.00f );
	colors[ ImGuiCol_PlotHistogram ]         = ImVec4( 1.00f, 0.78f, 0.28f, 1.00f );
	colors[ ImGuiCol_PlotHistogramHovered ]  = ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
	colors[ ImGuiCol_TextSelectedBg ]        = ImVec4( 0.59f, 0.54f, 0.18f, 1.00f );
	colors[ ImGuiCol_DragDropTarget ]        = ImVec4( 0.73f, 0.67f, 0.24f, 1.00f );
	colors[ ImGuiCol_NavHighlight ]          = ImVec4( 0.59f, 0.54f, 0.18f, 1.00f );
	colors[ ImGuiCol_NavWindowingHighlight ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
	colors[ ImGuiCol_NavWindowingDimBg ]     = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
	colors[ ImGuiCol_ModalWindowDimBg ]      = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );

	ImGuiStyle& style                        = ImGui::GetStyle();
	style.FrameBorderSize                    = 1.0f;
	style.WindowRounding                     = 0.0f;
	style.ChildRounding                      = 0.0f;
	style.FrameRounding                      = 0.0f;
	style.PopupRounding                      = 0.0f;
	style.ScrollbarRounding                  = 0.0f;
	style.GrabRounding                       = 0.0f;
	style.TabRounding                        = 0.0f;
}


auto main( int argc, char* argv[] ) -> int
{
	// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/setlocale-wsetlocale?view=msvc-170#utf-8-support
	// allows c ansi functions to use utf-8
	// only works on Windows 10 version 1803 (10.0.17134.0) and above
	setlocale( LC_ALL, ".UTF-8" );

	sys_init();

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

	size_t exe_dir_len = 0;
	char*  exe_dir     = sys_get_exe_folder( &exe_dir_len );

	{
		char font_path[ 260 ] = { 0 };

		memcpy( font_path, exe_dir, exe_dir_len * sizeof( char ) );
		strcat( font_path, PATH_SEP_STR "CascadiaCode.ttf" );

		ImGui::GetIO().Fonts->AddFontFromFileTTF( font_path, 15, nullptr );
	}

	// load fonts
	// ImGui::GetIO().Fonts->AddFontFromFileTTF( "D:\\projects\\replay_maker_v4\\out\\SourceSans3-Regular.ttf", 16, nullptr );
	// ImGui::GetIO().Fonts->AddFontFromFileTTF( "D:\\projects\\replay_maker_v4\\out\\CascadiaCode.ttf", 15, nullptr );

	ImGui_ImplOpenGL3_CreateFontsTexture();

	//imgui_set_theme_steam_green();

	// ------------------------------------------
	// Startup and Load MPV

	if ( !start_mpv() )
	{
		printf( "start_mpv failed!\n" );
		return 1;
	}

	// ------------------------------------------

	g_clip_data        = clip_create();

	if ( !g_clip_data )
	{
		printf( "failed to create clip data\n" );
		return 1;
	}

	{
		char settings_path[ 4096 ] = { 0 };

		memcpy( settings_path, exe_dir, exe_dir_len * sizeof( char ) );
		strcat( settings_path, PATH_SEP_STR "replay_maker_config.json5" );

		clip_parse_settings( g_clip_data, settings_path );
	}

	{
		char recent_path[ 256 ] = { 0 };

		memcpy( recent_path, exe_dir, exe_dir_len * sizeof( char ) );
		strcat( recent_path, PATH_SEP_STR RECENTLY_OPENED_FILE );

		g_recently_opened_path = strdup( recent_path );
	}

//	{
//		char videos_path[ 4096 ] = { 0 };
//
//		memcpy( videos_path, exe_dir, exe_dir_len * sizeof( char ) );
//		strcat( videos_path, PATH_SEP_STR "test_video.json5" );
//
//		clip_parse_videos( g_clip_data, videos_path );
//
//		g_videos_file_path = strdup( videos_path );
//	}

	free( exe_dir );

	g_recently_opened = ch_calloc< char* >( MAX_RECENT_OPEN );

	load_recently_opened();

	if ( argc > 1 )
	{
		if ( fs_is_file( argv[ 1 ] ) )
		{
			// assume these are clips
			if ( clip_parse_videos( g_clip_data, argv[ 1 ] ) )
			{
				update_recently_opened( argv[ 1 ] );
				g_videos_file_path = strdup( argv[ 1 ] );
			}
		}
	}

	printf( "loaded\n" );
	
	// Play this file.
	//mpv_cmd_loadfile( TEST_VIDEO_ANSI );

	win32_run();

	// ------------------------------------------
	// exit

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	win32_exit();

	// close mpv
	stop_mpv();

	for ( u8 i = 0; i < g_recently_opened_count; i++ )
		free( g_recently_opened[ i ] );

	free( g_recently_opened );
	free( g_recently_opened_path );

	return 0;
}

