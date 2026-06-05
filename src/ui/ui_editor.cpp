#include "main.h"
#include "clip/clip.h"

#include "imgui.h"

// native file dialog
// https://github.com/btzy/nativefiledialog-extended
#include "imgui_internal.h"
#include "nfd.h"

char*                          g_recently_opened_path       = nullptr;
char**                         g_recently_opened            = nullptr;
u8                             g_recently_opened_count      = 0;

char                           g_output_name_buf[ 512 ]     = { 0 };

// constexpr ImVec4               g_selected_btn_color( 1.f, 1.f, 1.f, 1.f );
// constexpr ImVec4               g_selected_btn_color( 0.21f, 0.45f, 0.73f, 1.f );
constexpr ImVec4               g_selected_btn_color( 0.31f, 0.55f, 0.86f, 1.f );

// UNUSED
static bool                    g_focus_replay_maker = false;

static int                     g_draw_built_in_menu = 0;

void                           draw_replay_list( int size[ 2 ] );


// ===============================================================================================
// Replay Editor Management


void replay_editor_reset()
{
	clip_data::current_output = nullptr;
	clip_data::current_source  = 0;
	memset( g_output_name_buf, 0, 512 * sizeof( char ) );

	timeline_reset();
}


bool replay_editor_set_video( u32 output_i, u32 input_i )
{
	if ( output_i >= clip_data::output_count )
		return false;

	clip_output_video_t& output = clip_data::output[ output_i ];

	if ( input_i >= output.source_count )
	{
		input_i = 0;
		printf( "invalid source index\n" );
		// return false;
	}

	// replay_editor_reset();

	clip_data::current_output       = &output;
	clip_data::current_output_index = output_i;
	clip_data::current_source        = input_i;

	memcpy( g_output_name_buf, output.name, strlen( output.name ) * sizeof( char ) );
	g_focus_replay_maker = true;

	set_mpv_index( input_i );
	
	return true;
}


void replay_editor_close_loose_video()
{
	if ( g_mpv_extra_vid_on )
	{
		mpv_cmd_close_video( EXTRA_VID_ID );

		if ( get_mpv_index() == EXTRA_VID_ID )
			set_mpv_index( 0 );
	}
}


void replay_editor_load_loose_video( const char* path )
{
	replay_editor_close_loose_video();

	mpv_cmd_loadfile( path, EXTRA_VID_ID );
	set_mpv_index( EXTRA_VID_ID );
}


void replay_editor_set_group( u32 output_i, u32 group_i, u32 group_src_i )
{
	if ( group_src_i == EXTRA_VID_ID )
	{
		set_mpv_index( EXTRA_VID_ID );
		return;
	}

	if ( output_i >= clip_data::output_count )
		return;

	clip_output_video_t& output       = clip_data::output[ output_i ];

	if ( group_i >= output.groups.size() )
		return;

	clip_output_group_t& group = output.groups[ group_i ];

	// No sources in current group
	if ( group.sources.empty() )
	{
		mpv_cmd_close_video();
		clip_data::current_group        = group_i;
		clip_data::current_group_source = group_src_i;
		replay_editor_set_video( output_i, 0 );
		return;
	}

	// if ( group_src_i >= group.sources.size() )
	// {
	// 	mpv_cmd_close_video();
	// 	clip_data::current_group        = group_i;
	// 	clip_data::current_group_source = group_src_i;
	// 	replay_editor_set_video( output_i, 0 );
	// 	return;
	// }

	clip_source_usage_t& source_use = group.sources[ group_src_i ];

	replay_editor_set_video( output_i, source_use.source_index );
	///if ( !replay_editor_set_video( output_i, input_i ) )
	//	return;

	if ( output.source_count <= source_use.source_index )
	{
		mpv_cmd_close_video();
		set_mpv_index( 0 );
		g_focus_replay_maker = true;
		return;
	}

	clip_source_t& source                   = output.source[ source_use.source_index ];

	bool           swapping_group_or_output = clip_data::current_output_index != output_i || clip_data::current_group != group_i;
	// bool pause_video              = swapping_group_or_output;
	bool           pause_video              = clip_data::current_output_index != output_i;

	pause_video |= mpv_get_current_video() && strcmp( mpv_get_current_video(), source.path ) != 0;

	if ( g_mpv_extra_vid_on && clip_data::current_output_index != output_i )
	{
		// replay_editor_close_loose_video();
	}

	clip_data::current_group        = group_i;
	clip_data::current_group_source = group_src_i;

	g_focus_replay_maker  = true;

	// check current mpv instance for playback state
	s32 paused                  = 0;
	p_mpv_get_property( get_mpv(), "pause", MPV_FORMAT_FLAG, &paused );

	// load all mpv instances
	set_mpv_count( output.source_count );

	for ( u32 i = 0; i < output.source_count; i++ )
	{
		mpv_cmd_loadfile( output.source[ i ].path, i );

		mpv_data_t* mpv = get_mpv_data( i );

		if ( mpv && mpv->mpv )
		{
			// if the video was playing, pause the other mpv clients and play the one we swapped to
			if ( i != source_use.source_index || pause_video )
			{
				const char* cmd[]   = { "set", "pause", "yes", NULL };
				int         cmd_ret = p_mpv_command_async( mpv->mpv, 0, cmd );
			}
			else if ( !paused )
			{
				const char* cmd[]   = { "set", "pause", "no", NULL };
				int         cmd_ret = p_mpv_command_async( mpv->mpv, 0, cmd );
			}
		}
	}

	// mpv has an extra video if it's loose
	if ( g_mpv_extra_vid_on && swapping_group_or_output )
	{
		mpv_data_t* mpv = get_mpv_data( EXTRA_VID_ID );

		if ( mpv )
		{
			const char* cmd[]   = { "set", "pause", "yes", NULL };
			int         cmd_ret = p_mpv_command_async( mpv->mpv, 0, cmd );
		}
	}

	set_mpv_index( source_use.source_index );
}


void replay_editor_load( clip_output_video_t* output )
{
	replay_editor_reset();

	clip_data::current_output = output;
	clip_data::current_source  = 0;

	memcpy( g_output_name_buf, output->name, strlen( output->name ) * sizeof( char ) );
	g_focus_replay_maker = true;
}


// ===============================================================================================
// Sidebar UI


void on_file_dialog_open()
{
	// pause mpv
	int pause                = 1;
	int cmd_ret              = p_mpv_set_property( get_mpv(), "pause", MPV_FORMAT_FLAG, &pause );

	app::pause_window_events = true;
}


void on_file_dialog_exit()
{
	app::pause_window_events = false;
}


void draw_replay_info_menu_bar()
{
	ImGui::BeginDisabled( clip_thread_loading() );

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
			nfdu8filteritem_t     filter   = { "replay maker videos", "json5" };
			nfdopendialogu8args_t args     = { 0 };

			args.filterList                = &filter;
			args.filterCount               = 1;
			args.defaultPath               = cwd;

			nfdresult_t result             = NFD_OpenDialogU8_With( &out_path, &args );

			if ( result == NFD_OKAY )
			{
				g_videos_file_path = util_strdup_r( g_videos_file_path, out_path );
				clip_thread_open_file( out_path );
				update_recently_opened( out_path );
				NFD_FreePathU8( out_path );
			}
			else if ( result == NFD_ERROR )
			{
				printf( "NativeFileDialog Error: %s\n", NFD_GetError() );
			}

			free( cwd );

			on_file_dialog_exit();

			printf( "Open\n" );
		}

		if ( ImGui::BeginMenu( "Open Recent" ) )
		{
			for ( u8 i = 0; i < g_recently_opened_count; i++ )
			{
				char* file_name = fs_get_filename_no_ext( g_recently_opened[ i ] );

				ImGui::PushID( i + 1 );

				if ( ImGui::MenuItem( file_name ) )
				{
					g_videos_file_path = util_strdup_r( g_videos_file_path, g_recently_opened[ i ] );
					clip_thread_open_file( g_recently_opened[ i ] );
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

		char  save_btn[ 256 ] = { "Save " };

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
			// nfdu8filteritem_t     filter   = { "video timestamps", "json,json5" };
			nfdu8filteritem_t     filters[ 1 ] = { { "replay maker videos json5", "json5" } };
			nfdsavedialogu8args_t args{};

			args.defaultPath   = cwd;
			args.defaultName   = videos_filename;
			args.filterList    = filters;
			args.filterCount   = 1;

			nfdresult_t result = NFD_SaveDialogU8_With( &out_path, &args );

			if ( result == NFD_OKAY )
			{
				g_videos_file_path = util_strdup_r( g_videos_file_path, out_path );
				save_videos();

				// add this to recently opened
				update_recently_opened( out_path );

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
			if ( g_draw_built_in_menu != 1 )
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

	{
		static float             time_since_last_update = 0;
		static ChVector< float > frame_time_history;
		static float             frame_time_average = 0.f;
		static float             frame_time_total   = 0.f;
		static u32               frame_count        = 0;

		if ( time_since_last_update > 0.15 )
		{
			frame_time_total       = time_since_last_update;
			time_since_last_update = 0.f;
			frame_time_average     = 0.f;

			for ( u32 i = 0; i < frame_time_history.size(); i++ )
				frame_time_average += frame_time_history[ i ];

			frame_time_average /= frame_time_history.size();
			frame_count = frame_time_history.size();
			frame_time_history.clear();
		}
		else
		{
			frame_time_history.push_back( app::frame_time );
			time_since_last_update += app::frame_time;
		}

		float frameRate = ImGui::GetIO().Framerate;

		char  buf[ 512 ]{};
		snprintf( buf, 512, "Real %.1f FPS (%.3f ms/frame) | Avg  %.1f FPS (%.3f ms/frame)",
		          frameRate, 1000.0f / frameRate,
		          frame_count / frame_time_total, 1000.f * frame_time_average );

		ImGui::TextUnformatted( buf );
	}
	// ImGui::Text( "%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate );

	ImGui::EndMenuBar();

	if ( g_draw_built_in_menu == 1 )
		ImGui::ShowStyleEditor();

	ImGui::EndDisabled();
}


void draw_preset_dropdown( clip_output_video_t& output, clip_output_group_t& group, bool edit )
{
	ImGuiStyle& style = ImGui::GetStyle();

	if ( edit )
	{
		if ( ImGui::BeginCombo( "##presets", "Presets", ImGuiComboFlags_HeightLargest | ImGuiComboFlags_WidthFitPreview ) )
		{
			for ( u32 i = 0; i < clip_data::preset_count; i++ )
			{
				// lmao what the fuck
				bool skip = false;
				for ( u32 g = 0; g < output.groups.size(); g++ )
				{
					clip_output_group_t& group_ = output.groups[ g ];

					for ( u32 used_preset_i = 0; used_preset_i < group_.presets.size(); used_preset_i++ )
					{
						if ( group_.presets[ used_preset_i ] == i )
						{
							skip = true;
							break;
						}
					}

					if ( skip )
						break;
				}

				if ( skip )
					continue;

				if ( ImGui::Selectable( clip_data::preset[ i ].name ) )
				{
					clip_group_add_preset( output, group, i );
				}
			}

			ImGui::EndCombo();
		}
	}

	// index in the array to remove
	u32 preset_remove = UINT32_MAX;

	for ( u32 i = 0; i < group.presets.size(); i++ )
	{
		if ( edit || i > 0 )
			ImGui::SameLine();

		clip_encode_preset_t& encode = clip_data::preset[ group.presets[ i ] ];

		ImGui::PushStyleColor( ImGuiCol_ButtonActive, COLOR_BTN_RED_ACTIVE );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, COLOR_BTN_RED_HOVER );
		ImGui::PushStyleColor( ImGuiCol_Button, COLOR_BTN_RED );

		if ( ImGui::Button( encode.name ) )
		{
			preset_remove = i;
		}

		ImGui::PopStyleColor( 3 );
	}

	if ( preset_remove != UINT32_MAX )
	{
		clip_group_remove_preset( output, group, preset_remove );
	}
}


// shows ffmpeg encode presets
void draw_preset_editor( int size[ 2 ] )
{
	static char preset_name_buf[ MAX_LEN_PRESET_NAME ] = { 0 };

	if ( ImGui::InputText( "Add Encode Preset", preset_name_buf, MAX_LEN_PRESET_NAME ) )
	{
		//clip_encode_preset_t* what = clip_add_encode_preset( "test", "testaa" );
	}

	// Edit encode preset

	// ImGui::InputText( "File Extension", ext_buf, MAX_LEN_EXT );

	// Display Encode Presets, and select one to edit above this list
	ImGui::Separator();
	ImGui::Text( "%d Encode Presets", clip_data::preset_count );
	ImGui::Separator();

	for ( u32 i = 0; i < clip_data::preset_count; i++ )
	{
		clip_encode_preset_t& preset = clip_data::preset[ i ];

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
			u32 profile = clip_add_prefix( prefix_name_buf, prefix_buf );

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
	ImGui::Text( "%d Prefixes", clip_data::prefix_count );

	for ( u32 i = 0; i < clip_data::prefix_count; i++ )
	{
		clip_prefix_t& prefix = clip_data::prefix[ i ];

		ImGui::Text( "Name:   %s", prefix.name );
		ImGui::Text( "Prefix: %s", prefix.prefix );
		ImGui::Separator();
	}
}


void draw_replay_editor_window( int window_size[ 2 ] )
{
	int element_size[ 2 ] = { 0, 0 };
	element_size[ 0 ]     = window_size[ 0 ] - app::mpv_size[ 0 ];
	element_size[ 1 ]     = window_size[ 1 ];

	ImGui::SetNextWindowSize( { (float)element_size[ 0 ], (float)element_size[ 1 ] } );
	ImGui::SetNextWindowPos( { 0.f, 0.f } );

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
		if ( ImGui::BeginTabItem( "Clip Entries" ) )
		{
			draw_replay_list( element_size );
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Encode Presets" ) )
		{
			draw_preset_editor( element_size );
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Prefixes" ) )
		{
			draw_prefix_editor( element_size );
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Settings" ) )
		{
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

