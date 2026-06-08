#include "main.h"

#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#include <SDL3/SDL.h>

int           g_grabbed_divider_idx = -1;
bool          g_hovered_divider     = false;

constexpr int DIVIDER_SIZE = 3;  // multiplied by 2

// ===============================================================================================
// Dividers

void move_divider( u32 index )
{
	// check if we released the key, we could release it outside the window mid drag, as it lags behind
	if ( !ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
	{
		g_grabbed_divider_idx = -1;
		return;
	}

	g_grabbed_divider_idx = index;

	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	if ( index == 0 )
	{
		// vertical divider
		//cursor_main.y -= g_grab_cursor_offset[ 1 ];
		app::mpv_size[ 1 ] = CLAMP( app::mouse_pos[ 1 ], 0, height );
		ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeNS );
	}
	else if ( index == 1 )
	{
		// horizontal divider
		//cursor_main.x -= g_grab_cursor_offset[ 0 ];
		app::mpv_size[ 0 ] = width - CLAMP( app::mouse_pos[ 0 ], 0, width );
		ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeEW );
	}

	// update mpv window size
	mpv_window_resize();

	//	window_render_all();
}


void update_dividers()
{
	//if ( !g_mouse_in_window )
	//	return;

	// calc divider rectangle
	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	// rectangle 0 - playback controls divider
	ImVec2   div_0_min{ float( width - app::mpv_size[ 0 ] ), (float)app::mpv_size[ 1 ] - DIVIDER_SIZE };
	ImVec2   div_0_max{ float( width ), (float)app::mpv_size[ 1 ] + DIVIDER_SIZE };

	// rectangle 1 - replay info
	ImVec2   div_1_min{ (float)( width - app::mpv_size[ 0 ] ) - DIVIDER_SIZE, 0.f };
	ImVec2   div_1_max{ (float)( width - app::mpv_size[ 0 ] ) + DIVIDER_SIZE, (float)height };

	ImGuiIO& io                       = ImGui::GetIO();

	g_hovered_divider                 = false;
	static bool last_frame_left_click = false;
	// bool        left_click            = ImGui::IsMouseClicked( ImGuiMouseButton_Left, false ) || ImGui::IsMouseDown( ImGuiMouseButton_Left );
	bool        left_click            = ImGui::IsKeyPressed( ImGuiKey_MouseLeft, false );

	// Already moving a divider, continue updating
	if ( g_grabbed_divider_idx != -1 )
	{
		g_hovered_divider = true;
		move_divider( g_grabbed_divider_idx );
		last_frame_left_click = left_click;
		return;
	}

	bool hover_divider_0 = mouse_in_rect( div_0_min, div_0_max );
	bool hover_divider_1 = mouse_in_rect( div_1_min, div_1_max );
	bool popup_hovered   = false;

	// check any popups first
	if ( hover_divider_0 || hover_divider_1 )
	{
		if ( ImGui::IsPopupOpen( "", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel ) )
		{
			bool          hovered_popup = false;
			ImGuiContext* context       = ImGui::GetCurrentContext();

			if ( context )
			{
				// Check popups to see if mouse is hovering over any of them
				for ( int i = 0; i < context->OpenPopupStack.Size; i++ )
				{
					ImGuiPopupData& data   = context->OpenPopupStack[ i ];
					ImGuiWindow*    window = data.Window;

					if ( mouse_in_rect( window->Pos, { window->Pos.x + window->Size.x, window->Pos.y + window->Size.y } ) )
					{
						last_frame_left_click = left_click;
						return;
					}
				}
			}
		}
	}

	// NOTE: not perfect, windows keeps setting the cursor back to default even though im not using a cursor in a window class, hmm
	if ( g_grabbed_divider_idx == 0 || hover_divider_0 )
	{
		g_hovered_divider = true;

		//io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse;
		ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeNS );

		if ( !last_frame_left_click && left_click )
			move_divider( 0 );
	}
	else if ( app::sidebar && ( g_grabbed_divider_idx == 1 || hover_divider_1 ) )
	{
		g_hovered_divider = true;

		//io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse;
		ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeEW );
		//SetCursor( g_cursor_resize_h );

		if ( !last_frame_left_click && left_click )
			move_divider( 1 );
	}
	else if ( io.ConfigFlags & ( ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse ) )
	{
		//io.ConfigFlags &= ~( ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoMouse );
		ImGui::SetMouseCursor( ImGuiMouseCursor_Arrow );
		//SetCursor( g_cursor_default );
		g_grabbed_divider_idx = -1;
	}

	last_frame_left_click = left_click;
}


// ===============================================================================================
// Clip Filtering


namespace clip_filter
{
	//bool               sort_newest_top;
	char               search[ 128 ] = { 0 };
	u32                prefix        = UINT32_MAX;
	std::vector< u32 > preset;
}


void clip_filtering_reset()
{
	//clip_filter::sort_newest_top = true;
	clip_filter::search[ 128 ] = { 0 };
	clip_filter::prefix        = UINT32_MAX;
	clip_filter::preset.clear();
}


bool clip_filtering_visible( clip_t& clip )
{
	if ( clip_filter::prefix != UINT32_MAX )
		if ( clip_filter::prefix != clip.prefix )
			return false;

	if ( clip_filter::search[ 0 ] != '\0' )
		if ( !strcasestr( clip.name, clip_filter::search ) )
			return false;

	if ( clip_filter::preset.size() )
	{
		for ( u32 preset_i : clip_filter::preset )
		{
			for ( size_t preset_use_i = 0; preset_use_i < clip.groups.size(); preset_use_i++ )
			{
				clip_group_t& preset_use = clip.groups[ preset_use_i ];

				if ( preset_use.presets.index( preset_i ) != UINT32_MAX )
					return true;
			}
		}

		return false;
	}

	return true;
}


void clip_filtering_draw()
{
	ImGuiStyle& style = ImGui::GetStyle();

	//if ( ImGui::BeginChild( "##clip_filtering", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
	{
		ImGui::BeginDisabled( clip_thread_loading() );

		//ImGui::TextUnformatted( "Clips" );
		//ImGui::Separator();

		ImVec2 prefix_filter_size = ImGui::CalcTextSize( "Prefix Filter" );
		ImVec2 search_size        = ImGui::CalcTextSize( "Search" );

		ImGui::TextUnformatted( "Search" );

		ImGui::SameLine();
		ImGui::Dummy( { prefix_filter_size.x - search_size.x - style.ItemSpacing.x, ImGui::GetTextLineHeight() } );
		ImGui::SameLine();

		ImGui::SetNextItemWidth( -FLT_MIN );

		// Search Box
		ImGui::InputText( "##search", clip_filter::search, 128 );

		// Search by Prefixes
		if ( clip_filter::prefix < UINT32_MAX )
			clip_filter::prefix = MIN( clip_data::prefix_count, clip_filter::prefix );

		ImGui::TextUnformatted( "Prefix Filter" );

		ImGui::SameLine();
		ImGui::SetNextItemWidth( -FLT_MIN );

		if ( ImGui::BeginCombo( "##prefix_filter", clip_filter::prefix == UINT32_MAX ? "" : clip_data::prefix[ clip_filter::prefix ].name ) )
		{
			if ( ImGui::Selectable( "None", clip_filter::prefix == UINT32_MAX ) )
				clip_filter::prefix = UINT32_MAX;

			for ( u32 i = 0; i < clip_data::prefix_count; i++ )
			{
				clip_prefix_t& prefix                                     = clip_data::prefix[ i ];
				char           prefix_display[ MAX_LEN_PRESET_NAME + 16 ] = { 0 };
				u32            result_count                               = 0;

				for ( u32 out_i = 0; out_i < clip_data::clip_count; out_i++ )
				{
					if ( i == clip_data::clip[ out_i ].prefix )
						result_count++;
				}

				snprintf( prefix_display, MAX_LEN_PRESET_NAME + 16, "%d - %s", result_count, prefix.name );

				if ( ImGui::Selectable( prefix_display, i == clip_filter::prefix ) )
				{
					clip_filter::prefix = i;
				}
			}

			ImGui::EndCombo();
		}

		//ImGui::SameLine();

		//ImGui::TextUnformatted( "Preset Filter" );
		//
		//ImGui::SameLine();
		//ImGui::SetNextItemWidth( -FLT_MIN );

		// Advanced filters, filter by encode presets
		if ( ImGui::BeginCombo( "##presets", "Preset Filter", ImGuiComboFlags_HeightLargest | ImGuiComboFlags_WidthFitPreview ) )
		{
			for ( u32 i = 0; i < clip_data::preset_count; i++ )
			{
				// check if it's already in the search list
				bool skip = false;
				for ( size_t used_preset_i = 0; used_preset_i < clip_filter::preset.size(); used_preset_i++ )
				{
					if ( i == clip_filter::preset[ used_preset_i ] )
					{
						skip = true;
						break;
					}
				}

				if ( skip )
					continue;

				if ( ImGui::Selectable( clip_data::preset[ i ].name ) )
				{
					clip_filter::preset.emplace_back( i );
				}
			}

			ImGui::EndCombo();
		}

		// index in the array to remove
		size_t preset_remove = SIZE_MAX;

		for ( size_t i = 0; i < clip_filter::preset.size(); i++ )
		{
			ImGui::SameLine();

			clip_encode_preset_t& encode = clip_data::preset[ clip_filter::preset[ i ] ];

			ImGui::PushStyleColor( ImGuiCol_ButtonActive, COLOR_BTN_RED_ACTIVE );
			ImGui::PushStyleColor( ImGuiCol_ButtonHovered, COLOR_BTN_RED_HOVER );
			ImGui::PushStyleColor( ImGuiCol_Button, COLOR_BTN_RED );

			if ( ImGui::Button( encode.name ) )
			{
				preset_remove = i;
			}

			ImGui::PopStyleColor( 3 );
		}

		if ( preset_remove != SIZE_MAX )
		{
			clip_filter::preset.erase( clip_filter::preset.begin() + preset_remove );
			preset_remove = SIZE_MAX;
		}

		//ImGui::Separator();

		// collapse_all = ImGui::Button( "Collapse All" );
		// ImGui::SameLine();

		//if ( ImGui::Button( sort_newest_top ? "Sort: Newest First" : "Sort: Oldest First" ) )
		//	sort_newest_top = !sort_newest_top;

		//ImGui::SameLine();

		//u32 result_count = 0;
		//for ( u32 out_i = 0; out_i < clip_data::clip_count; out_i++ )
		//{
		//	if ( clip_filtering_visible( clip_data::clip[ out_i ] ) )
		//		result_count++;
		//}
		//
		//ImGui::Text( "%d Entries", result_count );

		ImGui::EndDisabled();
	}
	//ImGui::EndChild();
}


// ===============================================================================================
// Base UI


void draw_replay_edit_creation_info()
{
	static u32 default_prefix        = 0;
	static u32 default_encode_preset = 0;

	// select default prefix and encode preset
	if ( default_prefix >= clip_data::prefix_count )
		default_prefix = 0;

	if ( default_encode_preset >= clip_data::preset_count )
		default_encode_preset = 0;

	mpv_data_t* mpv = get_mpv_data();

	ImGui::PushStyleColor( ImGuiCol_ButtonActive, COLOR_BTN_RED_ACTIVE );
	ImGui::PushStyleColor( ImGuiCol_ButtonHovered, COLOR_BTN_RED_HOVER );
	ImGui::PushStyleColor( ImGuiCol_Button, COLOR_BTN_RED );

	if ( ImGui::Button( "Close Video" ) )
	{
		mpv_cmd_close_video( EXTRA_VID_ID );
	}

	ImGui::SameLine();

	ImGui::PopStyleColor( 3 );

	if ( ImGui::Button( "New Video" ) )
	{
		// create a new output video based on the filename of the playing video
		if ( mpv && mpv->current_video )
		{
			replay_editor_reset();

			clip_t* clip = clip_add_entry( mpv->current_video );
			clip_data::current_clip   = clip;

			if ( clip )
			{
				u32                  new_group = clip_data::current_clip->groups.size();
				clip_group_t& group     = clip_data::current_clip->groups.emplace_back();

				group.presets.push_back( default_encode_preset );

				// Copy seek time and pause
				double time_pos = mpv->time_pos;
				s32    paused   = mpv->pause;

				clip_group_add_source( clip, new_group, mpv->current_video );
				replay_editor_set_group( clip_data::clip_count - 1, new_group, 0 );
				clip->prefix = default_prefix;

				p_mpv_set_property( get_mpv(), "time-pos", MPV_FORMAT_DOUBLE, &time_pos );
				// p_mpv_set_property( get_mpv(), "pause", MPV_FORMAT_FLAG, &paused );

				const char* cmd[]   = { "set", "pause", paused ? "yes" : "no", NULL };
				int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );

				// close loose video
				mpv_cmd_close_video( EXTRA_VID_ID );
			}
		}
	}

	ImGui::SameLine();

	// ImGui::TextUnformatted( "Default Prefix" );
	// ImGui::SameLine();
	// ImGui::SetNextItemWidth( -FLT_MIN );

	if ( clip_data::prefix_count )
	{
		if ( ImGui::BeginCombo( "Default Prefix", clip_data::prefix[ default_prefix ].name, ImGuiComboFlags_WidthFitPreview ) )
		{
			for ( u32 i = 0; i < clip_data::prefix_count; i++ )
			{
				if ( ImGui::Selectable( clip_data::prefix[ i ].name, i == default_prefix ) )
				{
					default_prefix = i;
				}
			}

			ImGui::EndCombo();
		}
	}

	ImGui::SameLine();

	if ( clip_data::preset_count )
	{
		if ( ImGui::BeginCombo( "Default Encode Preset", clip_data::preset[ default_encode_preset ].name, ImGuiComboFlags_WidthFitPreview ) )
		{
			for ( u32 i = 0; i < clip_data::preset_count; i++ )
			{
				if ( ImGui::Selectable( clip_data::preset[ i ].name, i == default_encode_preset ) )
				{
					default_encode_preset = i;
				}
			}

			ImGui::EndCombo();
		}
	}

	ImGui::Separator();

	if ( clip_data::current_clip && mpv && mpv->current_video )
	{
		ImGui::TextUnformatted( "Add Video to Group" );

		for ( u32 group_i = 0; group_i < clip_data::current_clip->groups.size(); group_i++ )
		{
			clip_group_t& group = clip_data::current_clip->groups[ group_i ];
			std::string group_name = clip_group_get_name( clip_data::current_clip, group, false );

			ImGui::SameLine();
			if ( ImGui::Button( group_name.c_str() ) )
			{
				u32 source_i = clip_group_add_source( clip_data::current_clip, group_i, mpv->current_video );
				replay_editor_set_group( clip_data::current_clip_index, group_i, source_i );
			}
		}

		ImGui::Separator();
	}
}


void draw_playback_controls( int size[ 2 ] )
{
	ImGuiStyle& style            = ImGui::GetStyle();
	ImVec2      region_avail     = ImGui::GetContentRegionAvail();

	double      time_pos         = 0;
	double      duration         = 0;
	s32         paused           = 0;

	double      volume           = 0;
	char*       audio_track      = 0;
	char*       audio_track_name = 0;

	if ( get_mpv_data() )
	{
		time_pos         = get_mpv_data()->time_pos;
		duration         = get_mpv_data()->duration;
		paused           = get_mpv_data()->pause;

		volume           = get_mpv_data()->volume;
		audio_track      = get_mpv_data()->audio_track;
		audio_track_name = get_mpv_data()->audio_track_title;
	}

	// seek bar

	// https://stackoverflow.com/questions/3673226/how-to-print-time-in-format-2009-08-10-181754-811

	char str_time_pos[ TIME_BUFFER ]{ 0 };
	char str_duration[ TIME_BUFFER ]{ 0 };

	util_format_time( str_time_pos, time_pos );
	util_format_time( str_duration, duration );

	// draw audio track name
	// audio track test
	//char* audio_track       = 0;
	// mpv_error audio_ret         = (mpv_error)p_mpv_get_property( g_mpv, "audio", MPV_FORMAT_NONE, &audio_track );
	//mpv_error audio_ret            = (mpv_error)p_mpv_get_property( get_mpv(), "audio", MPV_FORMAT_STRING, &audio_track );
	// mpv_error audio_ret         = (mpv_error)p_mpv_get_property( g_mpv, "track-list/audio/id", MPV_FORMAT_STRING, &audio_track );
	// mpv_error audio_ret         = (mpv_error)p_mpv_get_property( g_mpv, "track-list/audio/id", MPV_FORMAT_STRING, &audio_track );

	char      track_name_buf[ 32 ] = { 0 };
	// snprintf( track_name_buf, 32, "track-list/%s/title", audio_track );

	//audio_ret                      = (mpv_error)p_mpv_get_property( get_mpv(), "current-tracks/audio/title", MPV_FORMAT_STRING, &audio_track_name );
	//p_mpv_get_property_async( get_mpv(), e_mpv_cmd_audio_track, "audio", MPV_FORMAT_STRING );
	//p_mpv_get_property_async( get_mpv(), e_mpv_cmd_audio_title, "current-tracks/audio/title", MPV_FORMAT_STRING );

	//ImGui::PushStyleVarX( ImGuiStyleVar_ItemSpacing, 0.f );

	ImGui::Text( "%s / %s", str_time_pos, str_duration );

	ImGui::SameLine();
	ImGui::SeparatorEx( ImGuiSeparatorFlags_Vertical );
	ImGui::SameLine();

	ImGui::TextUnformatted( mpv_get_current_video() ? mpv_get_current_video() : "No Video Loaded" );

	ImGui::SameLine();
	ImGui::SeparatorEx( ImGuiSeparatorFlags_Vertical );
	ImGui::SameLine();

	ImGui::Text( "Audio: %s", audio_track_name ? audio_track_name : "" );

	const bool show_timeline = get_mpv_index() != EXTRA_VID_ID;

	//ImGui::Separator();

	//ImGui::PopStyleVar();
	{
		ImVec2 button_size = region_avail;
		button_size.y      = ImGui::GetTextLineHeight();
		button_size.x *= 0.5;

		if ( ImGui::BeginTabBar( "##video_preview_tabs" ) )
		{
			if ( show_timeline )
				ImGui::PushStyleColor( ImGuiCol_Tab, style.Colors[ ImGuiCol_TabSelected ] );

			ImGui::SetNextItemWidth( button_size.x );

			ImVec2 cursor_pos = ImGui::GetCursorPos();

			if ( ImGui::TabItemButton( "##timeline_view" ) )
			{
				set_mpv_index( 0 );
				replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, clip_data::get_current_group_source() );
			}

			if ( show_timeline )
				ImGui::PopStyleColor();
			else
				ImGui::PushStyleColor( ImGuiCol_Tab, style.Colors[ ImGuiCol_TabSelected ] );

			// Hack to center text on the tab
			ImGui::SameLine();
			ImGui::SetCursorPosX( cursor_pos.x );
			ImGui::TextAligned( 0.5, button_size.x, "Timelime View" );
			ImGui::SameLine();

			//ImGui::SameLine();
			//ImGui::Spacing();
			//ImGui::SameLine();

			ImGui::BeginDisabled( !g_mpv_extra_vid_on );

			char        loose_vid_name[ 300 ]{};

			mpv_data_t* mpv_extra = get_mpv_data( EXTRA_VID_ID );

			if ( mpv_extra && mpv_extra->current_video )
			{
				snprintf( loose_vid_name, 300, "Clip Preview - %s", mpv_extra->current_video );
			}
			else
			{
				snprintf( loose_vid_name, 300, "Clip Preview" );
			}

			ImGui::SetNextItemWidth( button_size.x );

			//cursor_pos = ImGui::GetCursorPos();

			// if ( ImGui::Selectable( loose_vid_name, g_show_loose_video, 0, button_size ) )
			if ( ImGui::TabItemButton( "##clip_preview" ) )
			{
				set_mpv_index( EXTRA_VID_ID );
			}

			//cursor_pos = ImGui::GetCursorPos();

			if ( !show_timeline )
				ImGui::PopStyleColor();

			ImGui::SameLine();
			ImGui::SetCursorPosX( cursor_pos.x + button_size.x );
			ImGui::TextAligned( 0.5, button_size.x, loose_vid_name );

			ImGui::EndDisabled();
		}

		ImGui::EndTabBar();

		// ImGui::Separator();
	}

	// can offset the seek bar to the right depending on the current playback time
	// ImGui::SameLine();

	// what if we had a custom seek bar that was snapshots of the video, kind of like the vscode text preview on the scrollbar
	// or have a thumbnail of the frame as a popup when you hover over the seek bar

	if ( show_timeline )
	{
		ImGui::Separator();
		timeline_draw();
	}
	else
	{
		if ( ImGui::BeginChild( "clip_creation", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY ) )
		{
			mpv_data_t* mpv = get_mpv_data( EXTRA_VID_ID );
			ImGui::BeginDisabled( !mpv->current_video );

			draw_replay_edit_creation_info();

			ImGui::PushItemWidth( -1 );

			// Basic video timeline
			float time_pos_f = (float)time_pos;
			if ( ImGui::SliderFloat( "##seek", &time_pos_f, 0.f, (float)duration ) )
			{
				mpv_cmd_seek( time_pos_f );
			}

			ImGui::PopItemWidth();
			ImGui::EndDisabled();
		}

		ImGui::EndChild();
	}

	ImVec2 region_avail2 = ImGui::GetContentRegionAvail();

	if ( region_avail2.y > ImGui::GetFrameHeightWithSpacing() )
	{
		ImGui::SetCursorPosY( size[ 1 ] - ImGui::GetFrameHeightWithSpacing() );
	}

	// ------------------------------------------------------------------------------
	// Base Video Controls

	const ImVec2 label_size    = ImGui::CalcTextSize( "Pause", NULL, true );
	ImVec2       play_btn_size = ImGui::CalcItemSize( { 0, 0 }, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f );

	if ( paused )
	{
		if ( ImGui::Button( "Play", play_btn_size ) )
		{
			if ( duration - 0.15 < time_pos )
			{
				// try to seek to next vid
				if ( show_timeline )
					timeline_advance();
			}

			const char* cmd[]   = { "set", "pause", "no", NULL };
			int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
			printf( "play- %d\n", cmd_ret );
		}
	}
	else
	{
		if ( ImGui::Button( "Pause", play_btn_size ) )
		{
			const char* cmd[]   = { "set", "pause", "yes", NULL };
			int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
			printf( "pause- %d\n", cmd_ret );
		}
	}

	ImGui::SameLine();
	ImGui::Spacing();

	ImGui::SameLine();
	if ( ImGui::Button( "<|" ) )
	{
		const char* cmd[]   = { "seek", "0", "absolute", NULL };
		int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
	}

	ImGui::SameLine();
	if ( ImGui::Button( "|>" ) )
	{
		char duration_str[ 16 ];
		gcvt( duration, 4, duration_str );

		// const char* cmd[]   = { "seek", duration_str, "absolute", NULL };
		const char* cmd[]   = { "seek", "100", "absolute-percent+exact", NULL };
		int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
	}

	ImGui::SameLine();
	ImGui::Spacing();

	ImGui::SameLine();
	if ( ImGui::Button( "<" ) )
	{
		const char* cmd[]   = { "frame-back-step", NULL };
		int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
	}

	ImGui::SameLine();
	if ( ImGui::Button( ">" ) )
	{
		const char* cmd[]   = { "frame-step", NULL };
		int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
	}

	// TODO: add speed controls here

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	// audio track selection
	char        audio_btn[ 16 ] = { 0 };

	mpv_data_t* mpv             = get_mpv_data();

	if ( audio_track && mpv )
	{
		/*if ( strcmp( audio_track, "auto" ) == 0 )
		{
			snprintf( audio_btn, 16, "Audio: auto/%lld", g_video_media_info.track_count_audio );
		}
		else*/
		if ( strcmp( audio_track, "no" ) == 0 )
		{
			snprintf( audio_btn, 16, "Audio: -/%lld", mpv->track_count_audio );
		}
		else
		{
			snprintf( audio_btn, 16, "Audio: %s/%lld", audio_track, mpv->track_count_audio );
		}
	}
	else
	{
		snprintf( audio_btn, 16, "Audio: none" );
	}

	char temp_test[ 16 ] = { 0 };
	if ( mpv )
		snprintf( temp_test, 16, "Audio: auto/%lld", mpv->track_count_audio );
	else
		snprintf( temp_test, 16, "Audio: none" );

	ImVec2 audio_btn_text = ImGui::CalcTextSize( temp_test );

	audio_btn_text.x += style.FramePadding.x * 2;
	audio_btn_text.y += style.FramePadding.y * 2;

	if ( ImGui::Button( audio_btn, audio_btn_text ) )
	{
		const char* cmd[]   = { "cycle", "audio", NULL };
		int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
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
		enable_sidebar( !app::sidebar );
	}

	ImGui::SameLine();

	ImGui::BeginDisabled( true );

	if ( ImGui::Button( "Take Screenshot" ) )
	{
	}

	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::SetNextItemWidth( 130.f );

	float volume_f = volume;
	if ( ImGui::SliderFloat( "Volume", &volume_f, 0.f, 130.f ) )
	{
		// convert float to string in c
		char volume_str[ 16 ];
		gcvt( volume_f, 4, volume_str );

		const char* cmd[]   = { "set", "volume", volume_str, NULL };
		int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
	}
}


void draw_imgui_window( int window_size[ 2 ] )
{
	//ImGui::ShowDemoWindow();
	//return;

	if ( g_encode_running )
	{
		encode_draw();
	}
	else
	{
		// draw sidebar
		if ( app::sidebar )
		{
			draw_replay_editor_window( window_size );
		}

		// draw playback controls
		{
			int element_size[ 2 ] = { 0, 0 };
			element_size[ 0 ]     = app::mpv_size[ 0 ] + 1;
			element_size[ 1 ]     = window_size[ 1 ] - app::mpv_size[ 1 ];

			ImGui::SetNextWindowSize( { (float)element_size[ 0 ], (float)element_size[ 1 ] } );
			ImGui::SetNextWindowPos( { float( window_size[ 0 ] - app::mpv_size[ 0 ] ), (float)app::mpv_size[ 1 ] } );

			if ( !ImGui::Begin( "##Playback Controls", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse ) )
			// if ( !ImGui::Begin( "##Playback Controls", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse ) )
			{
				ImGui::End();
				return;
			}

			draw_playback_controls( element_size );

			ImGui::End();
		}
	}
}


// ===============================================================================================
// Main UI Rendering Entry


void render_imgui()
{
	int width, height;
	SDL_GetWindowSize( app::window, &width, &height );

	int    window_size[ 2 ] = { width, height };

	ImVec4 clear_color      = ImVec4( 0.1f, 0.1f, 0.1f, 1.00f );

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	draw_imgui_window( window_size );

	// Rendering
	ImGui::Render();

	glViewport( 0, 0, width, height );
	glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
	glClear( GL_COLOR_BUFFER_BIT );

	if ( !g_encode_running )
	{
		mpv_draw_frame();
	}

	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

	// Present
	SDL_GL_SwapWindow( app::window );
}


void window_render_all()
{
	app::in_draw = true;

	if ( !app::fullscreen )
	{
		if ( !g_encode_running )
		{
			update_dividers();
		}

		render_imgui();
	}
	else
	{
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		ImGui::EndFrame();

		mpv_draw_frame();
	}

	app::in_draw = false;
}

