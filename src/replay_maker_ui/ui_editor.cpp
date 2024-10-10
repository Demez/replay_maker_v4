#include "main.h"
#include "clip/clip.h"

#include "imgui.h"

// native file dialog
// https://github.com/btzy/nativefiledialog-extended
#include "nfd.h"


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

char*                          g_video_input_dir;

static float                   g_time_range_start = 0.f;


void on_file_dialog_open()
{
	// pause mpv
	int pause   = 1;
	int cmd_ret = p_mpv_set_property( g_mpv, "pause", MPV_FORMAT_FLAG, &pause );

	sys_pause_window_events( true );
}


void on_file_dialog_exit()
{
	sys_pause_window_events( false );
}


void draw_replay_info_menu_bar()
{
	ImGui::BeginMenuBar();

	if ( ImGui::BeginMenu( "File" ) )
	{
		if ( ImGui::MenuItem( "New" ) )
		{
			printf( "New\n" );
		}

		if ( ImGui::MenuItem( "Open Videos" ) )
		{
			on_file_dialog_open();

			char*                 cwd      = sys_get_cwd();
			nfdu8char_t*          out_path = nullptr;
			nfdu8filteritem_t     filter   = { "replay maker videos", "json,json5,rmv" };
			nfdopendialogu8args_t args     = { 0 };

			args.filterList                = &filter;
			args.filterCount               = 1;
			args.defaultPath               = cwd;

			nfdresult_t result             = NFD_OpenDialogU8_With( &out_path, &args );

			on_file_dialog_exit();

			if ( result == NFD_OKAY )
			{
				g_videos_file_path = util_strdup_r( g_videos_file_path, out_path );
				clip_parse_videos( g_clip_data, out_path );
			}
			else if ( result == NFD_ERROR )
			{
				printf( "NativeFileDialog Error: %s\n", NFD_GetError() );
			}

			free( cwd );
			NFD_FreePathU8( out_path );

			printf( "Open\n" );
		}

		// if ( ImGui::MenuItem( "Open Directory" ) )
		// {
		// 	printf( "Open Dir\n" );
		// }

		ImGui::Separator();
		char* videos_filename = fs_get_filename( g_videos_file_path );

		char save_btn[ 256 ] = { "Save " };
		strcat( save_btn, videos_filename );

		if ( ImGui::MenuItem( save_btn ) )
		{
			save_videos();
		}

		if ( ImGui::MenuItem( "Save As" ) )
		{
			on_file_dialog_open();

			char*                 cwd          = sys_get_cwd();
			nfdu8char_t*          out_path     = nullptr;
			// nfdu8filteritem_t     filter   = { "video timestamps", "json,json5,rmv" };
			nfdu8filteritem_t     filters[ 2 ] = { { "replay maker videos", "rmv" }, { "replay maker videos json5", "json5" } };
			nfdsavedialogu8args_t args{};

			args.defaultPath      = cwd;
			args.defaultName      = videos_filename;
			args.filterList       = filters;
			args.filterCount      = 2;

			nfdresult_t  result   = NFD_SaveDialogU8_With( &out_path, &args );

			if ( result == NFD_OKAY )
			{
				g_videos_file_path = util_strdup_r( g_videos_file_path, out_path );
				save_videos();
			}
			else if ( result == NFD_ERROR )
			{
				printf( "NativeFileDialog Error: %s\n", NFD_GetError() );
			}

			on_file_dialog_exit();

			free( cwd );
			NFD_FreePathU8( out_path );
		}

		free( videos_filename );

		ImGui::Separator();

		if ( ImGui::MenuItem( "Save Config" ) )
		{
			save_settings();
		}

		ImGui::EndMenu();
	}

	static bool g_draw_style_editor = false;

	if ( ImGui::BeginMenu( "View" ) )
	{
		if ( ImGui::MenuItem( "Style Editor", nullptr, g_draw_style_editor ) )
		{
			g_draw_style_editor = !g_draw_style_editor;
		}

		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();

	if ( g_draw_style_editor )
		ImGui::ShowStyleEditor();
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

	g_clip_current_output = output;
	g_clip_current_input  = input_i;

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

	ImGuiStyle& style    = ImGui::GetStyle();

	size_t      imgui_id = 10000;

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

		// these buttons aren't implemented yet
		ImGui::BeginDisabled( true );

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

		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();

		ImVec2 region       = ImGui::GetWindowContentRegionMax();

		ImVec2 size_x       = ImGui::CalcTextSize( "X" );
		ImVec2 size_edit    = ImGui::CalcTextSize( edit_btn );
		ImVec2 size_up      = ImGui::CalcTextSize( "/\\" );
		ImVec2 size_down    = ImGui::CalcTextSize( "\\/" );

		//float  button_space = region.x - ( ( style.FramePadding.x * 2 ) + ( style.ItemSpacing.x * 4 ) + ( style.WindowPadding.x * 4 ) + 16.f );
		//float  button_space = region.x - ( ( style.WindowPadding.x * 2 ) + 16.f );
		float  button_space = region.x;

		button_space -= style.WindowPadding.x * 1;
		button_space -= 16.f;                                                  // indent
		button_space -= ( size_x.x + size_edit.x + size_up.x + size_down.x );  // text size
		button_space -= ( style.FramePadding.x * 8 );                          // padding applies to both sides of the frame, so 4 buttons * 2 times per button
		button_space -= ( style.ItemSpacing.x * 4 );                           // 4 buttons + 1 spacers

		float single_button_area = ( button_space / 2.f ) - style.ItemSpacing.x;

		ImGui::BeginDisabled( input_i != g_clip_current_input );

		char start_str[ 32 ] = { 0 };
		char end_str[ 32 ]   = { 0 };

		snprintf( start_str, 32, "%.4f", input->time_range[ time_range_i ].start );
		snprintf( end_str, 32, "%.4f", input->time_range[ time_range_i ].end );

		ImGui::PushID( imgui_id++ );
		ImGui::PushItemWidth( single_button_area );

		// TODO: if we're editing this time range, maybe when we push the start or end time button, it changes to the current seek time instead?
		if ( ImGui::Button( start_str, ImVec2( single_button_area, 0 ) ) )
		{
			const char* cmd[]   = { "seek", start_str, "absolute", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}

		ImGui::PopID();

		ImGui::SameLine();

		if ( ImGui::Button( end_str, ImVec2( single_button_area, 0 ) ) )
		{
			const char* cmd[]   = { "seek", end_str, "absolute", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}

		ImGui::PopItemWidth();

		ImGui::EndDisabled();
		// ImGui::Text( "%.4f - %.4f", input->time_range[ time_range_i ].start, input->time_range[ time_range_i ].end );
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
	if ( ImGui::InputText( "Name", g_output_name_buf, 512 ) )
	{
		// lol
		size_t name_len = strlen( g_output_name_buf );

		char*  new_data = ch_realloc( g_clip_current_output->name, name_len + 1 );

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

	if ( ImGui::Button( "Set Start Time" ) )
	{
		// get position from mkv
		double time_pos = 0;
		p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

		if ( g_edit_time_range )
		{
			// NOTE: maybe allow the user to set a start time later than the end time
			// but color the time range as red so the user knows it's invalid
			g_edit_time_range->start = time_pos;
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

			// NOTE: maybe allow the user to set a start time later than the end time
			// but color the time range as red so the user knows it's invalid
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
	ImGui::Text( "Current Start Time: %.4f", g_time_range_start );
	ImGui::Separator();

	ImGui::EndDisabled();

	// display input videos
	for ( u32 j = 0; j < g_clip_current_output->input_count; j++ )
	{
		clip_input_video_t* input    = &g_clip_current_output->input[ j ];

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

	if ( g_encode_override )
	{
		ImGui::Separator();
		draw_preset_override( *g_encode_override );
	}

	if ( g_clip_delete_input != UINT32_MAX )
	{
		clip_remove_input( g_clip_current_output, g_clip_delete_input );
		g_clip_delete_input  = UINT32_MAX;

		g_clip_current_input = g_clip_current_output->input_count > 0 ? 0 : UINT32_MAX;
	}
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
		in_prefix_adding = true;

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


void draw_replay_editor_window( int window_size[ 2 ] )
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

