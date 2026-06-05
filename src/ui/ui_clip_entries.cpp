#include "main.h"


namespace clip_reorder_drag
{
	static bool active;
	static bool just_selected;
	static u64  clip_id;    // source clip
	static u64  target_id;  // inserts it at this position, shifting everything down after it
};


extern char   g_output_name_buf[ 512 ];
static ImVec2 mouse_pos_diff{};


void draw_replay_edit_video_info( int size[ 2 ] )
{
	// if ( !ImGui::BeginChild( "##clip_info_edit", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY ) )
	if ( !ImGui::BeginChild( "##clip_info_edit", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY ) )
	{
		ImGui::EndChild();
		return;
	}

	ImGuiStyle& style = ImGui::GetStyle();

	ImVec2 save_pos = ImGui::GetCursorPos();

	ImGui::SetCursorPosY( save_pos.y + ( ImGui::GetFrameHeight() - ImGui::GetTextLineHeight() ) * 0.5f );
	ImGui::TextUnformatted( "Clip Entry Info" );
	ImGui::SameLine();

	ImGui::SetCursorPos( save_pos );

	ImGui::BeginDisabled( !clip_data::current_clip );

	ImVec2      line_remain   = ImGui::GetContentRegionAvail();

	float       spacing_width = line_remain.x;
	spacing_width -= ImGui::CalcTextSize( "Delete Video" ).x;
	spacing_width -= style.FramePadding.x * 2;
	spacing_width -= style.ItemSpacing.x;

	spacing_width                 = MAX( -style.ItemSpacing.x, spacing_width );

	ImGui::Dummy( { spacing_width, 0.f } );
	ImGui::SameLine();

	ImGui::PushStyleColor( ImGuiCol_ButtonActive, COLOR_BTN_RED_ACTIVE );
	ImGui::PushStyleColor( ImGuiCol_ButtonHovered, COLOR_BTN_RED_HOVER );
	ImGui::PushStyleColor( ImGuiCol_Button, COLOR_BTN_RED );

	ImGui::SetCursorPosY( save_pos.y );
	if ( ImGui::Button( "Delete Video" ) )
	{
		ImGui::PopStyleColor( 3 );

		clip_remove_entry( clip_data::current_clip );
		replay_editor_reset();
		ImGui::EndDisabled();
		ImGui::EndChild();
		return;
	}

	ImGui::PopStyleColor( 3 );

	ImGui::Separator();

	const ImVec2 name_text_size   = ImGui::CalcTextSize( "Name" );
	const ImVec2 prefix_text_size = ImGui::CalcTextSize( "Prefix" );

	float        avaliable_width  = size[ 0 ] - ( style.ItemSpacing.x * 2 + style.WindowPadding.x * 2 );
	float        name_bar_width   = ( avaliable_width - prefix_text_size.x );

	name_bar_width -= style.ItemSpacing.x * 2;

	ImGui::SetNextItemWidth( name_bar_width );

	// display output video data
	if ( ImGui::InputText( "Name", g_output_name_buf, 512 ) )
	{
		// lol
		size_t name_len = strlen( g_output_name_buf );

		char*  new_data = ch_realloc( clip_data::current_clip->name, name_len + 1 );

		if ( new_data )
		{
			clip_data::current_clip->name = new_data;
			memcpy( clip_data::current_clip->name, g_output_name_buf, name_len * sizeof( char ) );
			clip_data::current_clip->name[ name_len ] = '\0';
		}
	}

	// combo box to select prefix
	char* current_prefix = nullptr;

	if ( clip_data::current_clip )
	{
		if ( clip_data::current_clip->prefix >= clip_data::prefix_count )
		{
			// reset to general profile
			clip_data::current_clip->prefix = 0;
		}

		current_prefix = clip_data::prefix[ clip_data::current_clip->prefix ].name;
	}

	ImGui::SetNextItemWidth( name_bar_width );

	if ( ImGui::BeginCombo( "Prefix", current_prefix ) )
	{
		for ( u32 i = 0; i < clip_data::prefix_count; i++ )
		{
			// if ( i == clip_data::current_clip->prefix )
			// 	continue;

			if ( ImGui::Selectable( clip_data::prefix[ i ].name, clip_data::current_clip && i == clip_data::current_clip->prefix ) )
			{
				clip_data::current_clip->prefix = i;
			}
		}

		ImGui::EndCombo();
	}

	bool enabled = clip_data::current_clip ? clip_data::current_clip->enabled : false;
	if ( ImGui::Checkbox( "Enabled", &enabled ) )
		clip_data::current_clip->enabled = enabled;

	ImGui::EndDisabled();
	ImGui::EndChild();
}


void draw_replay_list_entry( u64& imgui_id, u32 out_i, bool collapse_all )
{
	ImVec2               cursor_screen_pos = ImGui::GetCursorScreenPos();
	ImVec2               cursor_pos        = ImGui::GetCursorPos();
	ImVec2               mouse_pos         = ImGui::GetMousePos();

	ImGuiStyle&          style             = ImGui::GetStyle();

	ImDrawList*          draw_list         = ImGui::GetWindowDrawList();

	bool                 drag_preview      = clip_reorder_drag::active && out_i == clip_reorder_drag::clip_id;

	clip_t& clip            = clip_data::clip[ out_i ];
	clip_prefix_t&       prefix            = clip_data::prefix[ clip.prefix ];

	ImVec2               drag_text_size    = ImGui::CalcTextSize( "--" );
	ImVec2               drag_pos_min( cursor_screen_pos.x, cursor_screen_pos.y );
	// ImVec2 drag_pos_max( cursor_screen_pos.x + drag_text_size.x + ( style.FramePadding.x * 2 ), cursor_screen_pos.y + drag_text_size.y + ( style.FramePadding.y * 2 ) );
	ImVec2               drag_pos_max( cursor_screen_pos.x + drag_text_size.x, cursor_screen_pos.y + drag_text_size.y + ( style.FramePadding.y * 2 ) );

	ImVec2               drag_pos_size( drag_pos_max - drag_pos_min );

	ImVec2               line_pos_0_min( drag_pos_min.x, drag_pos_min.y + ( drag_pos_size.y * 0.4 ) );
	ImVec2               line_pos_0_max( drag_pos_max.x, line_pos_0_min.y );

	ImVec2               line_pos_1_min( drag_pos_min.x, drag_pos_min.y + ( drag_pos_size.y * 0.6 ) );
	ImVec2               line_pos_1_max( drag_pos_max.x, line_pos_1_min.y );

	if ( clip_thread_idle() && !clip_reorder_drag::active || out_i == clip_reorder_drag::clip_id )
	{
		if ( point_in_rect( mouse_pos, drag_pos_min, drag_pos_max ) )
		{
			ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeNS );
		}

		if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) && point_in_rect( mouse_pos, drag_pos_min, drag_pos_max ) )
		{
			clip_reorder_drag::active        = true;
			clip_reorder_drag::just_selected = true;
			clip_reorder_drag::clip_id       = out_i;

			mouse_pos_diff                    = ImVec2( cursor_screen_pos.x - mouse_pos.x, cursor_screen_pos.y - mouse_pos.y );
		}
		else if ( clip_reorder_drag::active && out_i == clip_reorder_drag::clip_id )
		{
			//draw_list->AddRectFilled( drag_pos_min, drag_pos_max, ImColor( 0, 255, 0 ) );
			draw_list->AddLine( line_pos_0_min, line_pos_0_max, ImColor( 128, 128, 128 ), 2.f );
			draw_list->AddLine( line_pos_1_min, line_pos_1_max, ImColor( 128, 128, 128 ), 2.f );
		}
		else
		{
			// draw_list->AddRectFilled( drag_pos_min, drag_pos_max, ImColor( 64, 64, 64 ) );
			draw_list->AddLine( line_pos_0_min, line_pos_0_max, ImColor( 64, 64, 64 ), 2.f );
			draw_list->AddLine( line_pos_1_min, line_pos_1_max, ImColor( 64, 64, 64 ), 2.f );
		}
	}
	else
	{
		draw_list->AddLine( line_pos_0_min, line_pos_0_max, ImColor( 64, 64, 64 ), 2.f );
		draw_list->AddLine( line_pos_1_min, line_pos_1_max, ImColor( 64, 64, 64 ), 2.f );
	}

	// offset cursor to draw the rest of this
	// ImGui::SetCursorPosX( ImGui::GetCursorPosX() + drag_text_size.x + ( style.FramePadding.x * 2 ) + style.ItemSpacing.x );
	ImGui::SetCursorPosX( ImGui::GetCursorPosX() + drag_text_size.x + style.ItemSpacing.x );

	// ImGui::PushID( imgui_id++ );
	// if ( ImGui::Button( "Load" ) )
	// {
	// 	replay_editor_set_group( out_i, 0, 0 );
	// }
	// ImGui::PopID();
	// 
	// ImGui::SameLine();

	//ImVec2 load_text_size = ImGui::CalcTextSize( "Load" );
	//load_text_size.x += style.ItemInnerSpacing.x * 2;
	//load_text_size.y += style.ItemInnerSpacing.y * 2;

	char            header_name[ 512 ] = { 0 };
	//memset( header_name, 0, sizeof( char ) * 512 );

	ChVector< u32 > used_presets;
	used_presets.reserve( clip_data::preset_count );

	for ( clip_group_t& group : clip.groups )
	{
		for ( u32 preset : group.presets )
		{
			if ( used_presets.index( preset ) == UINT32_MAX )
			{
				used_presets.push_back( preset );
			}
		}
	}

	// snprintf( header_name, 512, "%d %s - %s - %d Inputs", out_i, prefix.name, output.name, output.source_count );
	snprintf( header_name, 512, "%s - %s - %u Export%s", prefix.name, clip.name ? clip.name : "Loading...", used_presets.size(), used_presets.size() > 1 ? "s" : "" );

	//if ( collapse_all )
	//	ImGui::SetNextItemOpen( false );

	ImGui::PushID( imgui_id++ );

	bool current_clip = &clip == clip_data::current_clip;

	// TODO THEME: change to a green color
	if ( current_clip )
		ImGui::PushStyleColor( ImGuiCol_Button, { 0.28f, 1.f, 0.21f, 0.31f } );

	else if ( clip.state == e_clip_state_invalid )
		ImGui::PushStyleColor( ImGuiCol_Button, COLOR_BTN_RED );

	ImGui::PushStyleVar( ImGuiStyleVar_ButtonTextAlign, { 0.f, 0.5f } );

	// if ( !ImGui::CollapsingHeader( output.name ? header_name : "Loading...", current_clip ? ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_Framed : 0 ) )
	if ( ImGui::Button( clip.name ? header_name : "Loading...", { -FLT_MIN, 0 } ) )
	{
		replay_editor_set_group( out_i, 0, 0 );

		//if ( current_clip || output.state == e_clip_state_invalid )
		//	ImGui::PopStyleColor();
		//
		//ImGui::PopID();
		//return;
	}

	ImGui::PopStyleVar();

	if ( current_clip || clip.state == e_clip_state_invalid )
		ImGui::PopStyleColor();

	ImGui::PopID();

	//for ( u32 in_i = 0; in_i < output.source_count; in_i++ )
	//{
	//	clip_source_t& source = output.source[ in_i ];
	//
	//	ImGui::PushID( in_i + 1 );
	//
	//	ImGui::TextUnformatted( source.path );
	//
	//	// if ( ImGui::TreeNode( source.path ) )
	//	{
	//		// display encode presets
	//		//for ( u32 preset_i = 0; preset_i < source.encode_overrides.presets_count; preset_i++ )
	//		//{
	//		//
	//		//}
	//
	//		// display source video times
	//		//for ( u32 time_range_i = 0; time_range_i < source.time_range_count; time_range_i++ )
	//		//{
	//		//	char start_str[ TIME_BUFFER ] = { 0 };
	//		//	char end_str[ TIME_BUFFER ]   = { 0 };
	//		//
	//		//	util_format_time( start_str, source.time_range[ time_range_i ].start );
	//		//	util_format_time( end_str, source.time_range[ time_range_i ].end );
	//		//
	//		//	ImGui::Text( "  %d - %s - %s", time_range_i, start_str, end_str );
	//		//}
	//
	//		// ImGui::TreePop();
	//	}
	//
	//	ImGui::PopID();
	//}
}


void draw_replay_list_entry_dummy( ImVec2 cursor_screen_pos, ImVec2 region_avail )
{
	ImGui::Dummy( { cursor_screen_pos.x + region_avail.x, ImGui::GetFrameHeight() } );
}


void draw_replay_list( int size[ 2 ] )
{
	ImGuiStyle&               style             = ImGui::GetStyle();

	// draw_replay_edit_video_info( size );

	bool                      collapse_all      = false;
	static bool               sort_newest_top   = true;
	static char               search_box[ 128 ] = { 0 };
	static u32                prefix_search     = UINT32_MAX;
	static std::vector< u32 > preset_search;

	static float              source_videos_height = ImGui::GetFrameHeightWithSpacing();
	static float              export_area_height   = ImGui::GetFrameHeightWithSpacing() * 2;

	{
		//float min_height = 0.f;
		//min_height += ImGui::GetFrameHeightWithSpacing() * 4.f;

		//region_avail.y -= ImGui::GetFrameHeightWithSpacing() * 2.f;

		// ImGui::SetNextWindowSizeConstraints( { -1, min_height }, { -1, region_avail.y } );

		//video_list_size -= ( ImGui::GetFrameHeightWithSpacing() * 2 + style.ItemSpacing.y * 2 );
		//ImGui::SetNextWindowSize( { -1, video_list_size } );
	}

	if ( ImGui::BeginChild( "clip_filtering", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
	{
		ImGui::BeginDisabled( clip_thread_loading() );

		ImGui::TextUnformatted( "Clip Entries" );
		ImGui::Separator();

		ImVec2 prefix_filter_size = ImGui::CalcTextSize( "Prefix Filter" );
		ImVec2 search_size        = ImGui::CalcTextSize( "Search" );

		ImGui::TextUnformatted( "Search" );

		ImGui::SameLine();
		ImGui::Dummy( { prefix_filter_size.x - search_size.x - style.ItemSpacing.x, ImGui::GetTextLineHeight() } );
		ImGui::SameLine();

		ImGui::SetNextItemWidth( -FLT_MIN );

		// Search Box
		ImGui::InputText( "##search", search_box, 128 );

		// Search by Prefixes
		if ( prefix_search < UINT32_MAX )
			prefix_search = MIN( clip_data::prefix_count, prefix_search );

		ImGui::TextUnformatted( "Prefix Filter" );

		ImGui::SameLine();
		ImGui::SetNextItemWidth( -FLT_MIN );

		if ( ImGui::BeginCombo( "##prefix_filter", prefix_search == UINT32_MAX ? "" : clip_data::prefix[ prefix_search ].name ) )
		{
			if ( ImGui::Selectable( "None", prefix_search == UINT32_MAX ) )
				prefix_search = UINT32_MAX;

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
		for ( u32 out_i = 0; out_i < clip_data::clip_count; out_i++ )
		{
			clip_t& clip = clip_data::clip[ out_i ];

			if ( prefix_search != UINT32_MAX )
				if ( prefix_search != clip.prefix )
					continue;

			if ( search_box[ 0 ] != '\0' )
				if ( !strcasestr( clip.name, search_box ) )
					continue;

			result_count++;
		}

		// Advanced filters, filter by encode presets
		if ( ImGui::BeginCombo( "##presets", "Preset Filter", ImGuiComboFlags_HeightLargest | ImGuiComboFlags_WidthFitPreview ) )
		{
			for ( u32 i = 0; i < clip_data::preset_count; i++ )
			{
				// check if it's already in the search list
				bool skip = false;
				for ( size_t used_preset_i = 0; used_preset_i < preset_search.size(); used_preset_i++ )
				{
					if ( i == preset_search[ used_preset_i ] )
					{
						skip = true;
						break;
					}
				}

				if ( skip )
					continue;

				if ( ImGui::Selectable( clip_data::preset[ i ].name ) )
				{
					preset_search.emplace_back( i );
				}
			}

			ImGui::EndCombo();
		}

		// index in the array to remove
		size_t preset_remove = SIZE_MAX;

		for ( size_t i = 0; i < preset_search.size(); i++ )
		{
			ImGui::SameLine();

			clip_encode_preset_t& encode = clip_data::preset[ preset_search[ i ] ];

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
			preset_search.erase( preset_search.begin() + preset_remove );
			preset_remove = SIZE_MAX;
		}

		ImGui::Separator();

		// collapse_all = ImGui::Button( "Collapse All" );
		// ImGui::SameLine();

		if ( ImGui::Button( sort_newest_top ? "Sort: Newest First" : "Sort: Oldest First" ) )
			sort_newest_top = !sort_newest_top;

		ImGui::SameLine();
		ImGui::Text( "%d Entries", result_count );


		// ImGui::Separator();

		float video_list_size{};

		{
			ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();
			ImVec2 region_avail      = ImGui::GetContentRegionAvail();
			// video_list_size          = region_avail;
			video_list_size          = size[ 1 ] - cursor_screen_pos[ 1 ];

			//video_list_size -= ( ImGui::GetFrameHeightWithSpacing() * 2 + style.ItemSpacing.y * 2 );
		}

		//ImGui::SetNextWindowSize( { -1, video_list_size } );

		ImGui::EndDisabled();
	}
	ImGui::EndChild();


	ImVec2 region_avail       = ImGui::GetContentRegionAvail();

	// ImGui::SetNextWindowSizeConstraints( { -1, ImGui::GetFrameHeightWithSpacing() }, { -1, region_avail.y - ( source_videos_height + export_area_height ) } );

	float  bottom_area_height = source_videos_height + ( style.ItemSpacing.y * 1 );
	bottom_area_height += ImGui::GetFrameHeightWithSpacing();      // source videos text
	bottom_area_height += ImGui::GetFrameHeightWithSpacing() * 2;  // encode section
	bottom_area_height += ImGui::GetFrameHeightWithSpacing() * 2;  // video name and prefix field
	bottom_area_height += ImGui::GetFrameHeightWithSpacing();      // enabled checkbox
	bottom_area_height += ImGui::GetFrameHeightWithSpacing();      // output vid section title
	bottom_area_height += style.ItemSpacing.y * 2.f;      // output vid delete video button

	//if ( !ImGui::BeginChild( "##video_list", {}, ImGuiChildFlags_Border ) )
	if ( ImGui::BeginChild( "##video_list", { -1, region_avail.y - bottom_area_height }, ImGuiChildFlags_Border ) )
	{
		//ImGui::EndChild();
		//ImGui::EndChild();
		//return;

		//	ImGui::Text( "SELECTED ID: %d", clip_reorder_drag::clip_id );
		//	ImGui::Text( "TARGET ID: %d", clip_reorder_drag::target_id );

		ImVec2 region_avail      = ImGui::GetContentRegionAvail();
		ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();
		ImVec2 cursor_pos        = ImGui::GetCursorPos();
		ImVec2 mouse_pos         = ImGui::GetMousePos();

		ImGui::BeginDisabled( clip_thread_loading() );

		float entry_height = ImGui::GetFontSize() + ( style.FramePadding.y * 2 );

		if ( clip_reorder_drag::active )
		{
			ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeNS );

			u32         out_i     = sort_newest_top ? clip_data::clip_count - 1 : 0;

			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			// for ( u32 out_i = clip_data::clip_count; out_i > 0; --out_i )
			for ( u32 loop_index = 0; loop_index < clip_data::clip_count; loop_index++ )
			{
				// check if point is in rect here
				ImVec2 insert_area_min( cursor_screen_pos.x, cursor_screen_pos.y + ( loop_index * ( entry_height + style.ItemSpacing.y ) ) );
				// ImVec2 insert_area_max( cursor_screen_pos.x + drag_text_size.x + ( style.FramePadding.x * 2 ), ( cursor_screen_pos.y + entry_height + ( loop_index * ( entry_height + style.ItemSpacing.y ) ) ) );
				ImVec2 insert_area_max( cursor_screen_pos.x + region_avail.x * ( style.FramePadding.x * 1 ), ( cursor_screen_pos.y + entry_height + ( loop_index * ( entry_height + style.ItemSpacing.y ) ) ) );

				//			draw_list->AddRectFilled( insert_area_min, insert_area_max, ImColor( 180, 0, 160 ) );
				//			char temp_test[ 8 ]{};
				//			snprintf( temp_test, 8, "%d", out_i );
				//			draw_list->AddText( insert_area_min, ImColor( 255, 255, 255 ),  temp_test );

				if ( point_in_rect( mouse_pos, insert_area_min, insert_area_max ) )
					clip_reorder_drag::target_id = out_i;

				out_i += sort_newest_top ? -1 : 1;
			}
		}

		u64    imgui_id        = 1;

		ImVec2 prev_cursor_pos = ImGui::GetCursorPos();
		u32    out_i           = sort_newest_top ? clip_data::clip_count - 1 : 0;

		// for ( u32 out_i = clip_data::clip_count; out_i > 0; --out_i )
		for ( u32 loop_index = 0; loop_index < clip_data::clip_count; loop_index++ )
		{
			clip_t& clip = clip_data::clip[ out_i ];
			clip_prefix_t&       prefix = clip_data::prefix[ clip.prefix ];

			if ( prefix_search != UINT32_MAX )
				if ( prefix_search != clip.prefix )
					continue;

			if ( search_box[ 0 ] != '\0' )
				if ( !strcasestr( clip.name, search_box ) )
					continue;

			if ( preset_search.size() )
			{
				bool preset_found = false;

				for ( u32 preset_i : preset_search )
				{
					if ( preset_found )
						break;

					for ( size_t preset_use_i = 0; preset_use_i < clip.groups.size(); preset_use_i++ )
					{
						clip_group_t& preset_use = clip.groups[ preset_use_i ];

						if ( preset_use.presets.index( preset_i ) != UINT32_MAX )
						{
							preset_found = true;
							break;
						}
					}
				}

				if ( !preset_found )
					continue;
			}

			if ( clip_reorder_drag::active && !clip_reorder_drag::just_selected )
			{
				if ( out_i == clip_reorder_drag::clip_id )
				{
					// limit placement?
					//float y_pos = MIN( mouse_pos.y + mouse_pos_diff.y, ImGui::GetCurrentWindow()->DC.CursorMaxPos.y - ImGui::GetFrameHeightWithSpacing() );
					float y_pos = mouse_pos.y + mouse_pos_diff.y;

					// Follow mouse cursor
					ImGui::SetCursorScreenPos( ImVec2( cursor_screen_pos.x, y_pos ) );
				}

				// if ( !sort_newest_top && out_i == 0 && out_i == clip_reorder_drag::target_id )
				if ( clip_reorder_drag::clip_id != out_i && loop_index == 0 && out_i == clip_reorder_drag::target_id )
				{
					ImGui::SetCursorPos( ImVec2( prev_cursor_pos.x, prev_cursor_pos.y + entry_height + style.ItemSpacing.y ) );
				}
			}

			//if ( clip_reorder_drag::active && clip_reorder_drag::clip_id == out_i )
			//{
			//	ImGui::Dummy( { cursor_screen_pos.x + region_avail.x, ImGui::GetFrameHeight() } );
			//}
			//else
			//{
			//}

			draw_replay_list_entry( imgui_id, out_i, collapse_all );

			//if ( ImGui::GetCurrentWindow()->DC.CursorMaxPos.y < ImGui::GetCursorScreenPos().y )
			//{
			//	printf( "???\n" );
			//}

			// if ( clip_reorder_drag::active && !clip_reorder_drag::just_selected )
			if ( clip_reorder_drag::active )
			{
				if ( out_i == clip_reorder_drag::clip_id )
				{
					if ( out_i == clip_reorder_drag::target_id )
					{
						// Reset cursor
						ImGui::SetCursorPos( ImVec2( prev_cursor_pos.x, prev_cursor_pos.y + entry_height + style.ItemSpacing.y ) );
					}
					else
					{
						// Reset cursor
						ImGui::SetCursorPos( ImVec2( prev_cursor_pos.x, prev_cursor_pos.y ) );
					}
				}

				// this is stupid lmao
				if ( sort_newest_top )
				{
					//printf( "TARGET ID: %d\n", clip_reorder_drag::target_id );
					// if the next one is the target id, move that one down twice
					if ( out_i < clip_reorder_drag::clip_id && out_i == clip_reorder_drag::target_id )
					{
						//draw_replay_list_entry_dummy( cursor_screen_pos, region_avail );
						//draw_replay_list_entry( imgui_id, out_i, collapse_all );
						ImGui::SetCursorPos( ImVec2( prev_cursor_pos.x, prev_cursor_pos.y + ( 2 * ( entry_height + style.ItemSpacing.y ) ) ) );
					}
					// behavior for moving back on the list
					else if ( out_i - 1 > clip_reorder_drag::clip_id && out_i - 1 == clip_reorder_drag::target_id )
					{
						//draw_replay_list_entry_dummy( cursor_screen_pos, region_avail );
						//printf( "CASE 1\n" );
						//draw_replay_list_entry( imgui_id, out_i, collapse_all );
						ImGui::SetCursorPos( ImVec2( prev_cursor_pos.x, prev_cursor_pos.y + ( 2 * ( entry_height + style.ItemSpacing.y ) ) ) );
					}
					//else if ( out_i == clip_reorder_drag::clip_id && out_i == clip_reorder_drag::target_id )
					//{
					//	printf( "CASE HUH\n" );
					//	draw_replay_list_entry_dummy( cursor_screen_pos, region_avail );
					//}
					//else if ( out_i != clip_reorder_drag::clip_id )
					//{
					//	draw_replay_list_entry( imgui_id, out_i, collapse_all );
					//}
					//else
					//{
					//	printf( "CASE END\n" );
					//	//draw_replay_list_entry_dummy( cursor_screen_pos, region_avail );
					//}
				}
				else
				{
					// if the next one is the target id, move that one down twice
					if ( out_i > clip_reorder_drag::clip_id && out_i == clip_reorder_drag::target_id )
					{
						//draw_replay_list_entry_dummy( cursor_screen_pos, region_avail );
						//draw_replay_list_entry( imgui_id, out_i, collapse_all );
						ImGui::SetCursorPos( ImVec2( prev_cursor_pos.x, prev_cursor_pos.y + ( 2 * ( entry_height + style.ItemSpacing.y ) ) ) );
					}
					// behavior for moving back on the list
					else if ( out_i + 1 < clip_reorder_drag::clip_id && out_i + 1 == clip_reorder_drag::target_id )
					{
						//draw_replay_list_entry_dummy( cursor_screen_pos, region_avail );
						//draw_replay_list_entry( imgui_id, out_i, collapse_all );
						ImGui::SetCursorPos( ImVec2( prev_cursor_pos.x, prev_cursor_pos.y + ( 2 * ( entry_height + style.ItemSpacing.y ) ) ) );
					}
					//else if ( out_i == clip_reorder_drag::clip_id && out_i == clip_reorder_drag::target_id )
					//{
					//	//draw_replay_list_entry_dummy( cursor_screen_pos, region_avail );
					//}
					//else
					//{
					//	draw_replay_list_entry( imgui_id, out_i, collapse_all );
					//}
				}
			}
			else
			{
				//draw_replay_list_entry( imgui_id, out_i, collapse_all );
			}

			prev_cursor_pos = ImGui::GetCursorPos();
			out_i += sort_newest_top ? -1 : 1;
		}


		// draw video dragging
		//if ( clip_reorder_drag::active )
		//{
		//	ImVec2 base_pos = ImGui::GetCursorScreenPos();
		//	//float  y_pos    = MIN( mouse_pos.y, ImGui::GetCurrentWindow()->DC.CursorMaxPos.y - ImGui::GetFrameHeightWithSpacing() );
		//	float  y_pos    = mouse_pos.y;
		//	ImGui::SetCursorScreenPos( ImVec2( cursor_screen_pos.x, y_pos ) );
		//	draw_replay_list_entry( imgui_id, clip_reorder_drag::clip_id, collapse_all );
		//
		//	ImGui::SetCursorScreenPos( base_pos );
		//}

		// imgui throws a fit if i don't do this, it used to work fine before
		if ( clip_reorder_drag::active )
			draw_replay_list_entry_dummy( cursor_screen_pos, region_avail );

		ImGui::EndDisabled();

		//ImGui::EndChild();
	}

	//ImGuiWindow* window   = ImGui::GetCurrentWindow();
	//float        cursor_y  = ImGui::GetCursorScreenPos().y;
	//float        cursor_y2 = window->DC.CursorPos.y;
	//
	//if ( window->DC.CursorMaxPos.y < cursor_y2 )
	//{
	//	printf( "???\n" );
	//}

	ImGui::EndChild();

	// ===================================================================================
	// Output Source List

	draw_replay_edit_video_info( size );

	ImGui::TextUnformatted( "Source Videos" );

	ImGui::SetNextWindowSizeConstraints( { -1, ImGui::GetFrameHeightWithSpacing() }, { -1, ImGui::GetFrameHeightWithSpacing() * 8.f } );

	if ( ImGui::BeginChild( "source_list", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY ) )
	{
		if ( clip_data::current_clip )
		{
			for ( u32 source_i = 0; source_i < clip_data::current_clip->source_count; source_i++ )
			{
				clip_source_t& source               = clip_data::current_clip->source[ source_i ];

				// Check if this source is used in the current group
				bool           source_used_in_group = false;

				if ( clip_data::current_group != UINT32_MAX && clip_data::current_group < clip_data::current_clip->groups.size() )
				{
					clip_group_t& group = clip_data::current_clip->groups[ clip_data::current_group ];

					for ( u32 source_use_i = 0; source_use_i < group.sources.size(); source_use_i++ )
					{
						clip_source_usage_t& source_use = group.sources[ source_use_i ];

						if ( source_i != source_use.source_index )
							continue;

						source_used_in_group = true;
						break;
					}
				}

				if ( ImGui::BeginChild( source_i + 1, {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle ) )
				{
					ImGui::TextUnformatted( source.path );
					ImGui::Separator();

					if ( ImGui::Button( "Add to Current Group" ) )
					{
						u32 i = clip_group_add_source( clip_data::current_clip, clip_data::current_group, source.path );
						replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, i );
					}

					ImGui::SameLine();

					if ( ImGui::Button( "Preview Video" ) )
					{
						// Set to MPV Loose video
						replay_editor_load_loose_video( source.path );
					}

					ImGui::SameLine();

					if ( ImGui::Button( "Delete" ) )
					{
						// clip_remove_source();
					}
				}

				ImGui::EndChild();
			}
		}
	}

	source_videos_height = ImGui::GetWindowHeight();
	ImGui::EndChild();

	// ===================================================================================
	// Export Area

	// ImGui::Separator();

	ImGui::BeginDisabled( clip_thread_loading() );

	//if ( ImGui::BeginChild( "##export", {}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings ) )
	{
		ImGui::InputText( "Output Path", g_output_dir, 512 );
		// ImGui::InputText( "Temp Path", g_temp_video_dir, 512 );

		ImGui::BeginDisabled( clip_data::clip_count == 0 || g_encode_running );

		if ( ImGui::Button( "Export" ) )
		{
			encode_thread_start();
		}

		ImGui::SameLine();

		// implement later
		ImGui::BeginDisabled();
		ImGui::BeginDisabled( clip_data::current_clip == nullptr );

		if ( ImGui::Button( "Export Selected" ) )
		{
		}

		ImGui::EndDisabled();

		ImGui::SameLine();

		if ( ImGui::Button( "Export Advanced" ) )
		{
		}
		ImGui::EndDisabled();

		ImGui::EndDisabled();

		if ( app::save_timer > 0.f )
		{
			ImGui::SameLine();
			ImGui::Text( "Saved: %s", g_videos_file_path );
		}
	}

	//export_area_height = ImGui::GetWindowHeight();
	//ImGui::EndChild();

	// ===================================================================================

	if ( clip_thread_idle() && clip_reorder_drag::active && ImGui::IsMouseReleased( ImGuiMouseButton_Left ) )
	{
		clip_reorder_drag::active = false;
		clip_move_entry( clip_reorder_drag::clip_id, clip_reorder_drag::target_id );

		// update the current output index and pointer
		if ( clip_data::current_clip_index != UINT32_MAX )
		{
			u32 min_i = std::min( clip_reorder_drag::clip_id, clip_reorder_drag::target_id );
			u32 max_i = std::max( clip_reorder_drag::clip_id, clip_reorder_drag::target_id );

			if ( clip_data::current_clip_index == clip_reorder_drag::clip_id )
			{
				clip_data::current_clip_index = clip_reorder_drag::target_id;
			}
			else if ( min_i <= clip_data::current_clip_index && max_i >= clip_data::current_clip_index )
			{
				if ( clip_reorder_drag::clip_id < clip_reorder_drag::target_id )
					clip_data::current_clip_index--;

				else if ( clip_reorder_drag::clip_id > clip_reorder_drag::target_id )
					clip_data::current_clip_index++;
			}

			clip_data::current_clip = &clip_data::clip[ clip_data::current_clip_index ];
		}

		clip_reorder_drag::clip_id   = 0;
		clip_reorder_drag::target_id = 0;
	}

	clip_reorder_drag::just_selected = false;

	ImGui::EndDisabled();
}

