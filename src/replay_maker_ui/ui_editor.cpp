#include "main.h"
#include "clip/clip.h"

#include "imgui.h"

// native file dialog
// https://github.com/btzy/nativefiledialog-extended
#include "nfd.h"

char*                          g_recently_opened_path   = nullptr;
char**                         g_recently_opened        = nullptr;
u8                             g_recently_opened_count  = 0;

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

static float                   g_time_range_start   = 0.f;

static bool                    g_focus_replay_maker = false;

static int                     g_draw_built_in_menu = 0;

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
				update_recently_opened( out_path );
				NFD_FreePathU8( out_path );
			}
			else if ( result == NFD_ERROR )
			{
				printf( "NativeFileDialog Error: %s\n", NFD_GetError() );
			}

			free( cwd );

			printf( "Open\n" );
		}

		// if ( ImGui::MenuItem( "Open Directory" ) )
		// {
		// 	printf( "Open Dir\n" );
		// }

		if ( ImGui::BeginMenu( "Open Recent" ) )
		{
			for ( u8 i = 0; i < g_recently_opened_count; i++ )
			{
				char* file_name = fs_get_filename_no_ext( g_recently_opened[ i ] );

				ImGui::PushID( i + 1 );

				if ( ImGui::MenuItem( file_name ) )
				{
					g_videos_file_path = util_strdup_r( g_videos_file_path, g_recently_opened[ i ] );
					clip_parse_videos( g_clip_data, g_recently_opened[ i ] );
					update_recently_opened( g_recently_opened[ i ] );
				}

				ImGui::PopID();

				free( file_name );
			}

			ImGui::EndMenu();
		}

		ImGui::Separator();

		ImGui::BeginDisabled( g_videos_file_path == nullptr );

		char* videos_filename = fs_get_filename( g_videos_file_path );

		char save_btn[ 256 ] = { "Save " };

		if ( videos_filename )
			strcat( save_btn, videos_filename );

		if ( ImGui::MenuItem( save_btn ) )
		{
			save_videos();
		}

		ImGui::EndDisabled();

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
				NFD_FreePathU8( out_path );
			}
			else if ( result == NFD_ERROR )
			{
				printf( "NativeFileDialog Error: %s\n", NFD_GetError() );
			}

			on_file_dialog_exit();

			free( cwd );
		}

		free( videos_filename );

		ImGui::Separator();

		if ( ImGui::MenuItem( "Save Config" ) )
		{
			save_settings();
		}

		ImGui::EndMenu();
	}

	if ( ImGui::BeginMenu( "View" ) )
	{
		if ( ImGui::MenuItem( "Style Editor", nullptr, g_draw_built_in_menu == 1 ) )
		{
			if ( g_draw_built_in_menu != 1)
				g_draw_built_in_menu = 1;
			else
				g_draw_built_in_menu = 0;
		}

		if ( ImGui::MenuItem( "Demo Window", nullptr, g_draw_built_in_menu == 2 ) )
		{
			if ( g_draw_built_in_menu != 2 )
				g_draw_built_in_menu = 2;
			else
				g_draw_built_in_menu = 0;
		}

		ImGui::EndMenu();
	}

	ImGui::BeginDisabled( g_videos_file_path == nullptr );

	// if ( ImGui::MenuItem( "Save" ) )
	if ( ImGui::Button( "Save" ) )
	{
		save_videos();
	}

	ImGui::EndDisabled();

	ImGui::EndMenuBar();

	if ( g_draw_built_in_menu == 1 )
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
	g_focus_replay_maker = true;
	return true;
}


void replay_editor_load_input( clip_output_video_t* output, u32 input_i )
{
	float cur_start_time = g_time_range_start;

	if ( !replay_editor_set_video( output, input_i ) )
		return;

	clip_input_video_t& input = output->input[ input_i ];
	g_focus_replay_maker      = true;

	if ( mpv_get_current_video() )
	{
		if ( strcmp( mpv_get_current_video(), input.path ) == 0 )
		{
			// hack to keep it lol
			g_time_range_start = cur_start_time;
			return;
		}
	}

	mpv_cmd_loadfile( input.path );
}


void replay_editor_load( clip_output_video_t* output )
{
	replay_editor_reset();

	g_clip_current_output = output;
	g_clip_current_input  = 0;

	memcpy( g_output_name_buf, output->name, strlen( output->name ) * sizeof( char ) );
	g_focus_replay_maker = true;
}


void draw_replay_list_entry( u64& imgui_id, u32 out_i, char* search_box, u32 prefix_search, bool collapse_all )
{
	clip_output_video_t& output = g_clip_data->output[ out_i ];
	clip_prefix_t&       prefix = g_clip_data->prefix[ output.prefix ];

	if ( prefix_search != UINT32_MAX )
		if ( prefix_search != output.prefix )
			return;

	if ( search_box[ 0 ] != '\0' )
		if ( !strcasestr( output.name, search_box ) )
			return;

	ImGui::PushID( imgui_id++ );
	if ( ImGui::Button( "Load" ) )
	{
		replay_editor_load_input( &output, 0 );
	}
	ImGui::PopID();

	ImGui::SameLine();

	//ImVec2 load_text_size = ImGui::CalcTextSize( "Load" );
	//load_text_size.x += style.ItemInnerSpacing.x * 2;
	//load_text_size.y += style.ItemInnerSpacing.y * 2;

	ImVec2 region_avail = ImGui::GetContentRegionAvail();

	char   header_name[ 512 ] = { 0 };
	//memset( header_name, 0, sizeof( char ) * 512 );

	snprintf( header_name, 512, "%s - %s - %d Inputs", prefix.name, output.name, output.input_count );

	if ( collapse_all )
		ImGui::SetNextItemOpen( false );

	if ( !ImGui::CollapsingHeader( header_name ) )
		return;

	//ImGui::PushID( imgui_id++ );
	//
	//if ( ImGui::Button( "Load" ) )
	//{
	//	replay_editor_load_input( &output, 0 );
	//}
	//
	//ImGui::PopID();

	for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
	{
		clip_input_video_t& input = output.input[ in_i ];

		ImGui::PushID( in_i + 1 );

		if ( ImGui::TreeNode( input.path ) )
		{
			// display encode presets
			//for ( u32 preset_i = 0; preset_i < input.encode_overrides.presets_count; preset_i++ )
			//{
			//
			//}

			// display input video times
			for ( u32 time_range_i = 0; time_range_i < input.time_range_count; time_range_i++ )
			{
				char start_str[ TIME_BUFFER ] = { 0 };
				char end_str[ TIME_BUFFER ]   = { 0 };

				util_format_time( start_str, input.time_range[ time_range_i ].start );
				util_format_time( end_str, input.time_range[ time_range_i ].end );

				ImGui::Text( "%d - %s - %s", time_range_i, start_str, end_str );
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}
}


void draw_replay_list( int size[ 2 ] )
{
	// display current data
	if ( !g_clip_data )
		return;

	static char search_box[ 128 ] = { 0 };

	// Search Box
	if ( ImGui::InputText( "Search Names", search_box, 128 ) )
	{
	}

	// Search by Prefixes
	static u32 prefix_search = UINT32_MAX;

	if ( prefix_search < UINT32_MAX )
		prefix_search = MIN( g_clip_data->prefix_count, prefix_search );

	if ( ImGui::BeginCombo( "Prefix Filter", prefix_search == UINT32_MAX ? "" : g_clip_data->prefix[ prefix_search ].name ) )
	{
		if ( ImGui::Selectable( "None", prefix_search == UINT32_MAX ) )
			prefix_search = UINT32_MAX;

		for ( u32 i = 0; i < g_clip_data->prefix_count; i++ )
		{
			clip_prefix_t& prefix                                     = g_clip_data->prefix[ i ];
			char           prefix_display[ MAX_LEN_PRESET_NAME + 16 ] = { 0 };
			u32            result_count                               = 0;

			for ( u32 out_i = 0; out_i < g_clip_data->output_count; out_i++ )
			{
				if ( i == g_clip_data->output[ out_i ].prefix )
					result_count++;
			}

			snprintf( prefix_display, MAX_LEN_PRESET_NAME + 16, "%d - %s", result_count, prefix.name );

			if ( ImGui::Selectable( prefix_display, i == prefix_search ) )
			{
				prefix_search = i;
			}
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine();

	// do the search again for this text lol
	u32 result_count = 0;
	for ( u32 out_i = 0; out_i < g_clip_data->output_count; out_i++ )
	{
		clip_output_video_t& output = g_clip_data->output[ out_i ];

		if ( prefix_search != UINT32_MAX )
			if ( prefix_search != output.prefix )
				continue;

		if ( search_box[ 0 ] != '\0' )
			if ( !strcasestr( output.name, search_box ) )
				continue;

		result_count++;
	}

	ImGui::Text( "%d Entries", result_count );

	ImGui::Separator();

	bool collapse_all = ImGui::Button( "Collapse All" );

	ImGui::SameLine();

	static bool sort_newest_top = true;

	if ( ImGui::Button( sort_newest_top ? "Sort: Newest First" : "Sort: Oldest First" ) )
		sort_newest_top = !sort_newest_top;

	ImGui::Separator();

	//if ( !ImGui::BeginChild( "##video_list", {}, ImGuiChildFlags_Border ) )
	if ( !ImGui::BeginChild( "##video_list" ) )
	{
		ImGui::EndChild();
		return;
	}

	//ImGuiStyle& style = ImGui::GetStyle();

	u64  imgui_id           = 1;

#if 1
	// draw_replay_list_entry

	if ( sort_newest_top )
	{
		for ( u32 out_i = g_clip_data->output_count; out_i > 0; --out_i )
		{
			draw_replay_list_entry( imgui_id, out_i - 1, search_box, prefix_search, collapse_all );
		}
	}
	else
	{
		for ( u32 out_i = 0; out_i < g_clip_data->output_count; out_i++ )
		{
			draw_replay_list_entry( imgui_id, out_i, search_box, prefix_search, collapse_all );
		}
	}

#else
	char header_name[ 512 ] = { 0 };
	for ( u32 out_i = 0; out_i < g_clip_data->output_count; out_i++ )
	{
		clip_output_video_t& output = g_clip_data->output[ out_i ];
		clip_prefix_t&       prefix = g_clip_data->prefix[ output.prefix ];

		if ( prefix_search != UINT32_MAX )
			if ( prefix_search != output.prefix )
				continue;

		if ( search_box[ 0 ] != '\0' )
			if ( !strcasestr( output.name, search_box ) )
				continue;

		ImGui::PushID( imgui_id++ );
		if ( ImGui::Button( "Load" ) )
		{
			replay_editor_load_input( &output, 0 );
		}
		ImGui::PopID();

		ImGui::SameLine();

		//ImVec2 load_text_size = ImGui::CalcTextSize( "Load" );
		//load_text_size.x += style.ItemInnerSpacing.x * 2;
		//load_text_size.y += style.ItemInnerSpacing.y * 2;

		ImVec2 region_avail = ImGui::GetContentRegionAvail();

		memset( header_name, 0, sizeof( char ) * 512 );
		
		snprintf( header_name, 512, "%s - %s - %d Inputs", prefix.name, output.name, output.input_count );

		if ( collapse_all )
			ImGui::SetNextItemOpen( false );

		if ( !ImGui::CollapsingHeader( header_name ) )
			continue;

		//ImGui::PushID( imgui_id++ );
		//
		//if ( ImGui::Button( "Load" ) )
		//{
		//	replay_editor_load_input( &output, 0 );
		//}
		//
		//ImGui::PopID();

		for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
		{
			clip_input_video_t& input = output.input[ in_i ];

			ImGui::PushID( in_i + 1 );

			if ( ImGui::TreeNode( input.path ) )
			{
				// display encode presets
				//for ( u32 preset_i = 0; preset_i < input.encode_overrides.presets_count; preset_i++ )
				//{
				//	
				//}

				// display input video times
				for ( u32 time_range_i = 0; time_range_i < input.time_range_count; time_range_i++ )
				{
					char start_str[ TIME_BUFFER ] = { 0 };
					char end_str[ TIME_BUFFER ]   = { 0 };

					util_format_time( start_str, input.time_range[ time_range_i ].start );
					util_format_time( end_str, input.time_range[ time_range_i ].end );

					ImGui::Text( "%d - %s - %s", time_range_i, start_str, end_str );
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}
#endif

	ImGui::EndChild();
}


void draw_preset_override_single( clip_encode_override_t& override )
{
	u32 selected = override.presets_count ? override.presets[ 0 ] : UINT32_MAX;

	if ( ImGui::BeginCombo( "Preset", override.presets_count ? g_clip_data->preset[ selected ].name : "" ) )
	{
		for ( u32 i = 0; i < g_clip_data->preset_count; i++ )
		{
			if ( ImGui::Selectable( g_clip_data->preset[ i ].name, i == selected ) )
			{
			}
		}
	}
}


void draw_preset_override( clip_encode_override_t& override )
{
	ImGuiStyle& style = ImGui::GetStyle();
	
	// display any overrides
	if ( ImGui::Button( override.preset_exclude ? "EXCLUDE" : "INCLUDE" ) )
		override.preset_exclude = !override.preset_exclude;

	ImGui::SameLine();

	ImVec2 text_size  = ImGui::CalcTextSize( "Presets" );
	float  combo_size = text_size.x + ( ( style.FramePadding.x + style.FramePadding.y + style.ItemSpacing.x ) * 2 );
	ImGui::SetNextItemWidth( combo_size );

	if ( ImGui::BeginCombo( "##presets", "Presets" ) )
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

#if 0

	ImGui::SameLine();

	ImVec2 region_avail = ImGui::GetContentRegionAvail();
	ImVec2 text_size    = ImGui::CalcTextSize( "Presets" );
	ImVec2 inc_size    = ImGui::CalcTextSize( "INCLUDE" );
	ImVec2 exc_size    = ImGui::CalcTextSize( "EXCLUDE" );

	float  include_size = MAX( inc_size.x, exc_size.x );
	include_size += style.ItemSpacing.x * 2;

	// add Y frame padding for drop down arrow
	float  combo_size    = text_size.x + ( ( style.FramePadding.x + style.FramePadding.y + style.ItemSpacing.x ) * 2 );

	float  spacing_width = region_avail.x;
	spacing_width -= combo_size;
	spacing_width -= style.ItemSpacing.x;
	spacing_width -= include_size;

	spacing_width = MAX( -style.ItemSpacing.x, spacing_width );

	ImGui::Dummy( { spacing_width, 0.f } );
	ImGui::SameLine();

	ImGui::SetNextItemWidth( combo_size );
#endif


//	ImGui::SameLine();
//
//	if ( ImGui::Button( override.preset_exclude ? "EXCLUDE" : "INCLUDE" ) )
//	{
//		override.preset_exclude = !override.preset_exclude;
//	}


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
	ImGuiStyle& style = ImGui::GetStyle();

	// ImGui::Text( "Input %d:\n%s", input_i, input->path );
	ImGui::TextUnformatted( input->path );

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

	// calcuate total duration
	float total_duration = 0.f;
	for ( u32 time_range_i = 0; time_range_i < input->time_range_count; time_range_i++ )
		total_duration += input->time_range[ time_range_i ].end - input->time_range[ time_range_i ].start;

	char total_duration_str[ TIME_BUFFER ] = { 0 };
	util_format_time( total_duration_str, total_duration );
	ImGui::Text( "Duration: %s", total_duration_str );

	ImGui::SameLine();

	ImVec2 line_remain = ImGui::GetContentRegionAvail();

	float  spacing_width = line_remain.x;
	spacing_width -= ImGui::CalcTextSize( "Delete" ).x;
	spacing_width -= style.FramePadding.x * 2;
	spacing_width -= style.ItemSpacing.x;

	spacing_width = MAX( -style.ItemSpacing.x, spacing_width );

	ImGui::Dummy( { spacing_width, 0.f } );
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

	u32  copy_time_range   = UINT32_MAX;
	u32  delete_time_range = UINT32_MAX;
	u32  move_time_range   = UINT32_MAX;
	bool move_up           = false;

//	ImGui::Indent( 16.f );

	size_t imgui_id          = 10000;

	bool   disabled          = mpv_get_current_video() ? strcmp( input->path, mpv_get_current_video() ) != 0 : true;

	for ( u32 time_range_i = 0; time_range_i < input->time_range_count; time_range_i++ )
	{
		if ( time_range_i > 0 )
			ImGui::Separator();

		// push id
		ImGui::PushID( imgui_id++ );

		if ( ImGui::Button( "X" ) )
		{
			delete_time_range = time_range_i;
		}

		ImGui::PopID();

		ImGui::SameLine();

		ImGui::PushID( imgui_id++ );

		if ( ImGui::Button( "Dup" ) )
		{
			copy_time_range = time_range_i;
		}

		ImGui::PopID();

		ImGui::SameLine();

		//static char edit_btn[ 8 ] = { 0 };
		//snprintf( edit_btn, 8, "Edit %d", time_range_i );

		// ImGui::BeginDisabled( input_i != g_clip_current_input );
		ImGui::BeginDisabled( disabled );

		ImGui::PushID( imgui_id++ );
		draw_edit_override_button( g_edit_time_range, &input->time_range[ time_range_i ], "Edit" );
		ImGui::PopID();

		ImGui::EndDisabled();

		ImGui::SameLine();

		// these buttons aren't implemented yet
		ImGui::BeginDisabled( true );

		ImGui::PushID( imgui_id++ );

		if ( ImGui::Button( "/\\" ) )
		{
			move_time_range = time_range_i;
			move_up         = true;
		}

		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );
		ImGui::SameLine();

		if ( ImGui::Button( "\\/" ) )
		{
			move_time_range = time_range_i;
			move_up         = false;
		}

		ImGui::PopStyleVar();

		ImGui::PopID();

		ImGui::EndDisabled();

		ImGui::SameLine();
	//	ImGui::Spacing();
	//	ImGui::SameLine();

		char duration_str[ TIME_BUFFER ] = { 0 };
		util_format_time( duration_str, input->time_range[ time_range_i ].end - input->time_range[ time_range_i ].start );

		ImVec2 region_avail       = ImGui::GetContentRegionAvail();
		ImVec2 size_duration      = ImGui::CalcTextSize( duration_str );
		float  duration_area      = ( size_duration.x / 2 ) + style.ItemSpacing.x;
		float  single_button_area = ( region_avail.x / 2.f ) - duration_area;

		ImGui::BeginDisabled( disabled );

		char start_str[ TIME_BUFFER ] = { 0 };
		char end_str[ TIME_BUFFER ]   = { 0 };

		util_format_time( start_str, input->time_range[ time_range_i ].start );
		util_format_time( end_str, input->time_range[ time_range_i ].end );

		// snprintf( start_str, 32, "%.4f", input->time_range[ time_range_i ].start );
		// snprintf( end_str, 32, "%.4f", input->time_range[ time_range_i ].end );

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

		ImGui::SameLine();
		ImGui::PopItemWidth();

		ImGui::PushItemWidth( duration_area );
		ImGui::TextUnformatted( duration_str );
		ImGui::PopItemWidth();

		ImGui::EndDisabled();

		ImGui::PushID( imgui_id++ );
		draw_preset_override( input->time_range[ time_range_i ].encode_overrides );
		ImGui::PopID();
		// ImGui::Text( "%.4f - %.4f", input->time_range[ time_range_i ].start, input->time_range[ time_range_i ].end );
	}

//	ImGui::Unindent( 16.f );

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

	// does the user want to copy a time range
	else if ( copy_time_range != UINT32_MAX )
	{
		clip_duplicate_time_range( g_clip_current_output, input_i, copy_time_range );
		copy_time_range = UINT32_MAX;
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

	ImGui::TextUnformatted( "TODO: Default Encode Presets Here" );

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

	ImGuiStyle& style         = ImGui::GetStyle();

	ImVec2 line_remain   = ImGui::GetContentRegionAvail();

	float  spacing_width = line_remain.x;
	spacing_width -= ImGui::CalcTextSize( "Delete" ).x;
	spacing_width -= style.FramePadding.x * 2;
	spacing_width -= style.ItemSpacing.x;

	spacing_width = MAX( -style.ItemSpacing.x, spacing_width );

	ImGui::Dummy( { spacing_width, 0.f } );
	ImGui::SameLine();

	if ( ImGui::Button( "Delete" ) )
	{
		clip_remove_output( g_clip_data, g_clip_current_output );
		replay_editor_reset();
		return;
	}

	ImGui::Separator();

	const ImVec2 name_text_size   = ImGui::CalcTextSize( "Name" );
	const ImVec2 prefix_text_size = ImGui::CalcTextSize( "Prefix" );

	float        avaliable_width  = size[ 0 ] - ( style.ItemSpacing.x * 2 );
	float        name_bar_width   = ( avaliable_width - prefix_text_size.x );

	name_bar_width -= style.ItemSpacing.x * 2;

	ImGui::SetNextItemWidth( name_bar_width );

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

	ImGui::SetNextItemWidth( name_bar_width );

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

	//ImGui::Spacing();
	//ImGui::Spacing();
	//ImGui::Spacing();
	//ImGui::Spacing();
	ImGui::Separator();

	// probably not needed, we can just use the presets stored on output videos to determine what encode presets to run this video on
	//draw_preset_override( g_clip_current_output->encode_overrides );
	//ImGui::Separator();

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

	// lmao
	draw_playback_controls( size, false );
	ImGui::Separator(); 

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
	
	ImGui::SameLine();

	if ( ImGui::Button( "Add Full Video Time" ) )
	{
		double duration = 0;
		p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );
		clip_add_time_range( g_clip_current_output, g_clip_current_input, 0, duration );
	}

	// show current start time?
	char start_time_str[ TIME_BUFFER ] = { 0 };
	util_format_time( start_time_str, g_time_range_start );
	ImGui::Text( "Current Start Time: %s", start_time_str );
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
		ImGui::TextUnformatted( "ffmpeg cmd:\n" );

		ImGui::PushTextWrapPos( 0.f );
		ImGui::TextUnformatted( preset.ffmpeg_cmd );
		ImGui::PopTextWrapPos();

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

	if ( g_draw_built_in_menu == 2 )
	{
		ImGui::ShowDemoWindow();
		ImGui::End();
		return;
	}

	if ( ImGui::BeginTabBar( "##replay_tabs" ) )
	{
		if ( ImGui::BeginTabItem( "Replay Editor", nullptr, g_focus_replay_maker ? ImGuiTabItemFlags_SetSelected : 0 ) )
		{
			draw_replay_edit( element_size );
			ImGui::EndTabItem();

			g_focus_replay_maker = false;
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

		if ( ImGui::BeginTabItem( "Style Editor" ) )
		{
			// TODO: maybe have this be a separate window so you can preview another tab easily?
			// TODO: pass in a ref pointer to allow saving to and loading from a file
			// TODO: when you implement that, implement a very basic theme loader/saver
			ImGui::ShowStyleEditor();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

