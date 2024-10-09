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


clip_data_t*                   g_clip_data              = nullptr;
clip_output_video_t*           g_clip_current_output    = nullptr;
u32                            g_clip_current_input     = 0;
u32                            g_clip_delete_input      = UINT32_MAX;
char                           g_output_name_buf[ 512 ] = { 0 };


static clip_encode_override_t* g_encode_override        = nullptr;
static clip_time_range_t*      g_edit_time_range        = nullptr;

// constexpr ImVec4               g_selected_btn_color( 1.f, 1.f, 1.f, 1.f );
constexpr ImVec4               g_selected_btn_color( 0.21f, 0.45f, 0.73f, 1.f );
static u32                     g_default_prefix   = 0;

const char*                    g_video_input_dir  = "";

static float                   g_time_range_start = 0.f;


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
			sys_pause_window_events( true );

			nfdu8char_t*          out_path = nullptr;
			nfdu8filteritem_t     filter   = { "timestamps file", "json,json5,rmk" };
			nfdopendialogu8args_t args     = { 0 };
			args.filterList                = &filter;
			args.filterCount               = 1;
			args.defaultPath               = sys_get_cwd();

			nfdresult_t result             = NFD_OpenDialogU8_With( &out_path, &args );

			sys_pause_window_events( false );

			if ( result == NFD_OKAY )
			{
				clip_parse_videos( g_clip_data, out_path );
			}
			else if ( result == NFD_ERROR )
			{
				printf( "NativeFileDialog Error: %s\n", NFD_GetError() );
			}

			printf( "Open\n" );
		}

		// if ( ImGui::MenuItem( "Open Directory" ) )
		// {
		// 	printf( "Open Dir\n" );
		// }

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
		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
}


void replay_editor_reset()
{
	g_clip_current_output = nullptr;
	g_encode_override     = nullptr;
	g_edit_time_range     = nullptr;
	g_clip_current_input  = 0;
	g_clip_delete_input   = UINT32_MAX;
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

	replay_editor_reset();

	g_clip_current_output     = output;
	g_clip_current_input      = input_i;

	memcpy( g_output_name_buf, output->name, strlen( output->name ) * sizeof( char ) );
}


void replay_editor_load_input( clip_output_video_t* output, u32 input_i )
{
	if ( !replay_editor_set_video( output, input_i ) )
		return;

	clip_input_video_t& input = output->input[ input_i ];

	if ( strcmp( mpv_get_current_video(), input.path ) == 0 )
		return;

	mpv_cmd_loadfile( input.path );
}


void draw_replay_list( int size[ 2 ] )
{
	// display current data
	if ( !g_clip_data )
		return;

	// display output videos
	u32 imgui_id = 1;

	for ( u32 i = 0; i < g_clip_data->output_count; i++ )
	{
		ImGui::Indent( 16.f );

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
				replay_editor_load_input( output, j );
			}
			ImGui::PopID();

			imgui_id++;

			ImGui::Text( "%d - %s", j, input->path );

			ImGui::Separator();

			ImGui::Indent( 16.f );

			// display input video times
			for ( u32 time_range_i = 0; time_range_i < input->time_range_count; time_range_i++ )
			{
				ImGui::Text( "%d - %.4f - %.4f", time_range_i, input->time_range[ time_range_i ].start, input->time_range[ time_range_i ].end );
			}

			ImGui::Unindent( 16.f );
		}

		ImGui::Unindent( 16.f );

		ImGui::Separator();
		ImGui::Separator();
	}
}


void draw_preset_override( clip_encode_override_t& override )
{
	if ( ImGui::BeginCombo( "Include/Exclude Presets", "" ) )
	{
		for ( u32 i = 0; i < g_clip_data->preset_count; i++ )
		{
			// lmao what the fuck
			bool skip = false;
			for ( u32 used_preset_i = 0; used_preset_i < override.presets_count; used_preset_i++ )
			{
				if ( i == override.presets[ used_preset_i ] )
				{
					skip = true;
					continue;
				}
			}

			if ( skip )
				continue;

			if ( ImGui::Selectable( g_clip_data->preset[ i ].name ) )
			{
				clip_add_preset_to_encode_override( g_clip_data, override, i );
			}
		}

		ImGui::EndCombo();
	}

	// display any overrides
	if ( ImGui::Button( override.preset_exclude ? "Mode: Exclude" : "Mode: Include" ) )
	{
		override.preset_exclude = !override.preset_exclude;
	}

	// index in the array to remove
	u32 preset_remove = UINT32_MAX;

	for ( u32 i = 0; i < override.presets_count; i++ )
	{
		ImGui::SameLine();

		char                  button_name[ MAX_LEN_PRESET_NAME + 4 ] = { "(X) " };

		clip_encode_preset_t& encode                                 = g_clip_data->preset[ override.presets[ i ] ];

		memcpy( &button_name[ 4 ], encode.name, MAX_LEN_PRESET_NAME );

		if ( ImGui::Button( button_name ) )
		{
			preset_remove = i;
		}
	}

	if ( preset_remove != UINT32_MAX )
	{
		util_array_remove_element( override.presets, override.presets_count, preset_remove );
	}

//	ImGui::Separator();
//
//	if ( ImGui::Button( "Enter Custom ffmpeg cmd" ) )
//	{
//	}
}


template< typename T >
void draw_edit_override_button( T*& selected, T* item, const char* name )
{
	if ( selected == item )
		ImGui::PushStyleColor( ImGuiCol_Button, g_selected_btn_color );

	if ( ImGui::Button( name ) )
	{
		if ( selected == item )
		{
			ImGui::PopStyleColor();
			selected = nullptr;
		}
		else
		{
			selected = item;
		}
	}
	else
	{
		if ( selected == item )
			ImGui::PopStyleColor();
	}
}


void draw_preset_override_button( clip_encode_override_t* override, const char* name )
{
	draw_edit_override_button( g_encode_override, override, name );
}


void draw_input_video_edit( u32 input_i, clip_input_video_t* input )
{
	ImGui::Text( "Input %d:\n%s", input_i, input->path );

	if ( ImGui::Button( "Set Current" ) )
	{
		replay_editor_load_input( g_clip_current_output, input_i );
	}

	ImGui::SameLine();
	if ( ImGui::Button( "Duplicate" ) )
	{
		u32 i = clip_duplicate_input( g_clip_current_output, input_i );

		if ( i != UINT32_MAX )
			replay_editor_load_input( g_clip_current_output, i );
	}

	ImGui::SameLine();
	if ( ImGui::Button( "Delete" ) )
	{
		g_clip_delete_input = input_i;
	}

	//ImGui::SameLine();
	//draw_preset_override_button( &input->encode_overrides, "Edit Input Presets" );

	ImGui::Separator();

	draw_preset_override( input->encode_overrides );

	// display input video times
	if ( input->time_range_count == 0 )
		return;

	ImGui::Separator();

	u32  delete_time_range = UINT32_MAX;
	u32  move_time_range   = UINT32_MAX;
	bool move_up           = false;

	ImGui::Indent( 16.f );

	for ( u32 time_range_i = 0; time_range_i < input->time_range_count; time_range_i++ )
	{
		// push id
		if ( ImGui::Button( "X" ) )
		{
			delete_time_range = time_range_i;
		}

		ImGui::SameLine();

		static char edit_btn[ 8 ] = { 0 };
		snprintf( edit_btn, 8, "Edit %d", time_range_i );

		ImGui::BeginDisabled( input_i != g_clip_current_input );

		draw_edit_override_button( g_edit_time_range, &input->time_range[ time_range_i ], edit_btn );

		ImGui::EndDisabled();

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
		ImGui::Text( "%.4f - %.4f", input->time_range[ time_range_i ].start, input->time_range[ time_range_i ].end );
	}

	ImGui::Unindent( 16.f );

	// does the user want to move a time range?
	if ( move_time_range != UINT32_MAX )
	{
		// move up
		if ( move_up && move_time_range == 0 )
		{
			return;
		}

		// move down
		if ( !move_up && move_time_range == input->time_range_count )
		{
			return;
		}

		move_time_range = UINT32_MAX;
	}

	// does the user want to delete a time range
	else if ( delete_time_range != UINT32_MAX )
	{
		clip_remove_time_range( g_clip_current_output, input_i, delete_time_range );
		delete_time_range = UINT32_MAX;
	}
}


void draw_replay_edit( int size[ 2 ] )
{
	if ( !g_clip_data )
	{
		ImGui::TextUnformatted( "No Clip Data Loaded or Created, use File/New or File/Open" );
		return;
	}

	// select default prefix
	if ( g_default_prefix >= g_clip_data->prefix_count )
		g_default_prefix = 0;

	if ( ImGui::BeginCombo( "Default Prefix", g_clip_data->prefix[ g_default_prefix ].name ) )
	{
		for ( u32 i = 0; i < g_clip_data->prefix_count; i++ )
		{
			if ( ImGui::Selectable( g_clip_data->prefix[ i ].name, i == g_default_prefix ) )
			{
				g_default_prefix = i;
			}
		}

		ImGui::EndCombo();
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

				output->prefix = g_default_prefix;
			}
		}
	}

	if ( !g_clip_current_output )
		return;

	ImGui::SameLine();

	if ( ImGui::Button( "Add Input Video" ) )
	{
		u32 i = clip_add_input( g_clip_current_output, mpv_get_current_video() );

		replay_editor_load_input( g_clip_current_output, i );
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Delete Output Video" ) )
	{
		clip_remove_output( g_clip_data, g_clip_current_output );
		replay_editor_reset();
		return;
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

	// combo box to select prefix
	char* current_prefix = nullptr;

	if ( g_clip_current_output->prefix >= g_clip_data->prefix_count )
	{
		// reset to general profile
		g_clip_current_output->prefix = 0;
	}

	current_prefix = g_clip_data->prefix[ g_clip_current_output->prefix ].name;

	if ( ImGui::BeginCombo( "Prefix", current_prefix ) )
	{
		for ( u32 i = 0; i < g_clip_data->prefix_count; i++ )
		{
			// if ( i == g_clip_current_output->prefix )
			// 	continue;

			if ( ImGui::Selectable( g_clip_data->prefix[ i ].name, i == g_clip_current_output->prefix ) )
			{
				g_clip_current_output->prefix = i;
			}
		}

		ImGui::EndCombo();
	}

	// draw_preset_override_button( &g_clip_current_output->encode_overrides, "Edit Presets" );

	ImGui::Separator();

	draw_preset_override( g_clip_current_output->encode_overrides );

	ImGui::Separator();

	// push_edit_button_color( &g_clip_current_output->encode_overrides );
	// 
	// if ( ImGui::Button( "Edit Presets" ) )
	// {
	// 	if ( g_encode_override == &g_clip_current_output->encode_overrides )
	// 		g_encode_override = nullptr;
	// 	else
	// 		g_encode_override = &g_clip_current_output->encode_overrides;
	// }

	//ImGui::Separator();

	ImGui::BeginDisabled( g_clip_current_input == UINT32_MAX );
	ImGui::Separator();

	if ( ImGui::Button( "Set Start Time" ) )
	{
		// get position from mkv
		double time_pos = 0;
		p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

		if ( g_edit_time_range )
		{
			g_time_range_start = time_pos;
		}
		else
		{
			g_time_range_start = time_pos;
		}
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Set End Time" ) )
	{
		double time_pos = 0;
		p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

		if ( g_edit_time_range )
		{
			// set end time
			// TODO: add undo system and have this be an action you can undo

			if ( time_pos < g_edit_time_range->start )
			{
				printf( "end time is earlier than start time! - %.4f > %.4f\n", g_edit_time_range->start, time_pos );
			}
			else
			{
				g_edit_time_range->end = time_pos;
			}
		}
		else
		{
			if ( time_pos < g_time_range_start )
			{
				printf( "end time is earlier than start time! - %.4f > %.4f\n", g_time_range_start, time_pos );
			}
			else
			{
				clip_add_time_range( g_clip_current_output, g_clip_current_input, g_time_range_start, time_pos );
				g_time_range_start = time_pos;
			}
		}
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Reset Start Time" ) )
	{
		g_time_range_start = 0;
	}

	// show current start time?
	ImGui::Separator();
	ImGui::Text( "Current Start Time: %.4f", g_time_range_start );
	ImGui::Separator();

	ImGui::EndDisabled();

	// display input videos
	for ( u32 j = 0; j < g_clip_current_output->input_count; j++ )
	{
		clip_input_video_t* input = &g_clip_current_output->input[ j ];

		// in case this is changed in the function
		u32                 current  = g_clip_current_input;

		ImVec4              frame_bg = ImGui::GetStyleColorVec4( ImGuiCol_ChildBg );
		ImGuiChildFlags     flags    = ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY;

		if ( current == j )
		{
			// flags |= ImGuiChildFlags_FrameStyle;
			// frame_bg.x = 0.5;
			// frame_bg.y = 0.5;
			// frame_bg.z = 0.5;

			frame_bg.x = 0.15;
			frame_bg.y = 0.15;
			frame_bg.z = 0.15;
			frame_bg.w = 1;
		}
		else
		{
			// frame_bg.x *= 0.4;
			// frame_bg.y *= 0.4;
			// frame_bg.z *= 0.4;
		}

		ImGui::PushStyleColor( ImGuiCol_ChildBg, frame_bg );

		if ( ImGui::BeginChild( (size_t)input, ImVec2( 0.f, 0.f ), flags ) )
		{
			draw_input_video_edit( j, input );
		}

		ImGui::EndChild();

		//if ( current == j )
		{
			ImGui::PopStyleColor();
		}
	}

	// ideas for other stuff to put here:
	// - section for a list of all the videos in the current directory
	// - open the saved video list
	// - maybe something to run the encoder?
	// - section for the current output video
	// - buttons for making an output video

	// for now just show the current video name idfk
	// ImGui::TextUnformatted( TEST_VIDEO );

#if 0
	if ( g_clip_current_input == UINT32_MAX )
		return;

	ImGui::Separator();

	if ( ImGui::Button( "Set Start Time" ) )
	{
		// get position from mkv
		double time_pos = 0;
		p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

		if ( g_edit_time_range )
		{
			g_time_range_start = time_pos;
		}
		else
		{
			g_time_range_start = time_pos;
		}
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Set End Time" ) )
	{
		double time_pos = 0;
		p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

		if ( g_edit_time_range )
		{
			// set end time
			// TODO: add undo system and have this be an action you can undo

			if ( time_pos < g_edit_time_range->start )
			{
				printf( "end time is earlier than start time! - %.4f > %.4f\n", g_edit_time_range->start, time_pos );
			}
			else
			{
				g_edit_time_range->end = time_pos;
			}
		}
		else
		{
			if ( time_pos < g_time_range_start )
			{
				printf( "end time is earlier than start time! - %.4f > %.4f\n", g_time_range_start, time_pos );
			}
			else
			{
				clip_add_time_range( g_clip_current_output, g_clip_current_input, g_time_range_start, time_pos );
				g_time_range_start = time_pos;
			}
		}
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Reset Start Time" ) )
	{
		g_time_range_start = 0;
	}

	// show current start time?
	ImGui::Separator();
	ImGui::Text( "Current Start Time: %.4f", g_time_range_start );
#endif

	if ( g_encode_override )
	{
		ImGui::Separator();
		draw_preset_override( *g_encode_override );
	}

	if ( g_clip_delete_input != UINT32_MAX )
	{
		clip_remove_input( g_clip_current_output, g_clip_delete_input );
		g_clip_delete_input = UINT32_MAX;

		g_clip_current_input = g_clip_current_output->input_count > 0 ? 0 : UINT32_MAX;
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
	//return;

	// draw sidebar
	{
		int element_size[ 2 ] = { 0, 0 };
		element_size[ 0 ]     = window_size[ 0 ] - g_mpv_size[ 0 ];
		element_size[ 1 ]     = window_size[ 1 ];

		ImGui::SetNextWindowSize( { (float)element_size[ 0 ], (float)element_size[ 1 ] } );
		ImGui::SetNextWindowPos( { (float)g_mpv_size[ 0 ], 0.f } );

		//ImGui::ShowDemoWindow();

		
		// if ( !ImGui::Begin( "##Replay Info", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar ) )
		if ( !ImGui::Begin( "##Replay Info", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar ) )
		{
			ImGui::End();
			return;
		}
		
		draw_replay_info_menu_bar();
		
		if ( ImGui::BeginTabBar( "##replay_tabs" ) )
		{
			if ( ImGui::BeginTabItem( "Replay Editor" ) )
			{
				draw_replay_edit( element_size );
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

