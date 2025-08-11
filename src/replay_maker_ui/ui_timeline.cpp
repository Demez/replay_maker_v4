#include "main.h"
#include "clip/clip.h"
#include "imgui.h"


constexpr int               TIMELINE_HEIGHT      = 40;
constexpr int               TIMELINE_BORDER_SIZE = 1;
constexpr int               SECTION_TITLEBAR_HEIGHT = 16;

constexpr ImColor           SECTION_COLOR_BORDER( 192, 192, 192 );
constexpr ImColor           SECTION_COLOR_BASE( 128, 128, 128 );

constexpr ImColor           SECTION_COLOR_SELECT_BORDER( 230, 230, 0 );
constexpr ImColor           SECTION_COLOR_SELECT_BASE( 128, 128, 128 );

// constexpr ImColor           TIMELINE_MARKER_COLOR_A( 0, 160, 160 );
constexpr ImColor           TIMELINE_MARKER_COLOR_A( 11, 160, 255 );
// constexpr ImColor           TIMELINE_MARKER_COLOR_B( 160, 0, 160 );
constexpr ImColor           TIMELINE_MARKER_COLOR_B( 255, 150, 11 );

ImVec2                      g_timeline_size;
ImVec2                      g_timeline_pos;

extern clip_data_t*         g_clip_data;
extern clip_output_video_t* g_clip_current_output;
extern u32                  g_clip_current_output_index;
extern u32                  g_clip_current_input;

u32                         g_selected_section      = UINT32_MAX;

bool                        g_timeline_marker_active[ 2 ]{};
float                       g_timeline_marker_times[ 2 ]{};


void timeline_set_seek_time( float seconds )
{
	char time_pos_str[ 16 ];
	gcvt( seconds, 4, time_pos_str );

	const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void timeline_reset()
{
	g_selected_section = UINT32_MAX;

	// TODO: maybe later, have saved markers for each input video, so if your swapping back and forth to line something up, this can stay saved?
	g_timeline_marker_active[ 0 ] = false;
	g_timeline_marker_active[ 1 ] = false;

	g_timeline_marker_times[ 0 ]  = 0.f;
	g_timeline_marker_times[ 1 ]  = 0.f;
}


void timeline_marker_control( ImGuiIO& io, ImGuiKey key, int marker )
{
	if ( ImGui::IsKeyPressed( key, false ) )
	{
		// If shift key pressed, snap seek pos to this marker pos
		if ( io.KeyShift )
		{
			if ( g_timeline_marker_active[ marker ] )
			{
				timeline_set_seek_time( g_timeline_marker_times[ marker ] );
			}
		}
		else if ( io.KeyCtrl )
		{
			// delete marker
			g_timeline_marker_active[ marker ] = false;
			g_timeline_marker_times[ marker ]  = 0.f;
		}
		else
		{
			// set marker
			double time_pos = 0;
			p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

			g_timeline_marker_active[ marker ] = true;
			g_timeline_marker_times[ marker ]  = time_pos;
		}
	}
}


void timeline_draw()
{
	bool draw_tabs_and_sections = g_clip_current_output && g_clip_current_input != UINT32_MAX &&
		( mpv_get_current_video() ? strcmp( g_clip_current_output->input[ g_clip_current_input ].path, mpv_get_current_video() ) == 0 : false );

	// if ( draw_tabs_and_sections )
	{
		// ImGui::BeginDisabled( !draw_tabs_and_sections );
		ImGui::BeginDisabled( !( g_clip_current_output && g_clip_current_input != UINT32_MAX ) );
		// ImGui::SameLine();

		if ( ImGui::Button( "Add Video" ) )
		{
			u32 i = clip_add_input( g_clip_current_output, mpv_get_current_video() );
			replay_editor_load_input( g_clip_current_output_index, i );
		}

		ImGui::SameLine();

		if ( ImGui::Button( "Delete Video" ) )
		{
			clip_remove_input( g_clip_current_output, g_clip_current_input );
			timeline_reset();
			// replay_editor_reset();
			// return;

			g_clip_current_input = g_clip_current_output->input_count > 0 ? 0 : UINT32_MAX;
		}

		ImGui::EndDisabled();

		if ( draw_tabs_and_sections )
		{
			ImGui::SameLine();

			clip_input_video_t& current_input = g_clip_current_output->input[ g_clip_current_input ];
			draw_preset_override( current_input.encode_overrides, true );
		}
	}

	ImGuiIO&    io           = ImGui::GetIO();
	ImGuiStyle& style        = ImGui::GetStyle();

	ImDrawList* draw_list    = ImGui::GetWindowDrawList();

	double      duration     = 0;
	p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );

	// TODO: make sure no inputs get captured if focused in a drop down or typing in a text box

	bool        capture_inputs = !io.WantTextInput;

	// test WantCaptureMouseUnlessPopupClose?

	// ------------------------------------------------------------------------------------------
	// Draw tabs on top for which encode preset currently in use and current input video?
	float       text_height  = ImGui::CalcTextSize( "TEMP" ).y;

	if ( ImGui::BeginTabBar( "##timeline_tabs" ) )
	{
		if ( draw_tabs_and_sections )
		{
			for ( u32 input_i = 0; input_i < g_clip_current_output->input_count; input_i++ )
			{
				clip_input_video_t& input = g_clip_current_output->input[ input_i ];

				char                title[ 64 ]{};
				snprintf( title, 64, "Entry %d | %d Sections | %d Presets", input_i, input.time_range_count, input.encode_overrides.presets_count );

				bool selected_tab_ref = g_clip_current_input == input_i;
				bool selected_tab = g_clip_current_input == input_i;

				if ( selected_tab )
				{
					ImGui::PushStyleColor( ImGuiCol_Button, style.Colors[ ImGuiCol_ButtonActive ] );
				}

				// if ( ImGui::TabItemButton( title, selected_tab ? ImGuiTabItemFlags_SetSelected : 0 ) )
				if ( ImGui::TabItemButton( title ) )
				{
					if ( g_clip_current_input != input_i )
						replay_editor_load_input( g_clip_current_output_index, input_i );
				}

				if ( selected_tab )
				{
					ImGui::PopStyleColor();
				}
			}
		}

		ImGui::EndTabBar();
	}

	// ------------------------------------------------------------------------------------------
	// Marker controls for creating new sections (Q and W, E to create as a section and clear markers)

	if ( duration && capture_inputs )
	{
		timeline_marker_control( io, ImGuiKey_Q, 0 );
		timeline_marker_control( io, ImGuiKey_E, 1 );

		// Create section from markers, both markers don't need to be active, it will default to the start or end of the video
		if ( ImGui::IsKeyPressed( ImGuiKey_R, false ) )
		{
			float start_time = g_timeline_marker_active[ 0 ] ? g_timeline_marker_times[ 0 ] : 0.f;
			float end_time   = g_timeline_marker_active[ 1 ] ? g_timeline_marker_times[ 1 ] : duration;

			// can't have this be inverted, flip if needed
			if ( end_time < start_time )
			{
				float temp = end_time;
				end_time   = start_time;
				start_time = temp;
			}

			clip_add_time_range( g_clip_current_output, g_clip_current_input, start_time, end_time );

			// reset markers
			g_timeline_marker_active[ 0 ] = false;
			g_timeline_marker_active[ 1 ] = false;

			g_timeline_marker_times[ 0 ]  = 0.f;
			g_timeline_marker_times[ 1 ]  = 0.f;
		}
	}

	// ------------------------------------------------------------------------------------------
	// Use [ and ] keys to jump to start and end points of each section and start/end of video
	
	if ( duration && draw_tabs_and_sections && capture_inputs )
	{
		if ( ImGui::IsKeyPressed( ImGuiKey_A, false ) )
		{
			double time_pos = 0;
			p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

			// search for the first notable time to snap to
			float closest_time = 0.f;

			clip_input_video_t& current_input = g_clip_current_output->input[ g_clip_current_input ];

			for ( u32 time_i = 0; time_i < current_input.time_range_count; time_i++ )
			{
				clip_time_range_t& time_range = current_input.time_range[ time_i ];

				if ( /*time_range.start < time_pos + 0.05 ||*/ time_range.start < time_pos - 0.15 )
					closest_time = std::max( closest_time, time_range.start );

				if ( /*time_range.end < time_pos + 0.05 ||*/ time_range.end < time_pos - 0.15 )
					closest_time = std::max( closest_time, time_range.end );
			}

			// check markers

			timeline_set_seek_time( closest_time );
		}

		if ( ImGui::IsKeyPressed( ImGuiKey_D, false ) )
		{
			double time_pos = 0;
			p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

			// search for the first notable time to snap to
			float closest_time = duration;

			clip_input_video_t& current_input = g_clip_current_output->input[ g_clip_current_input ];

			for ( u32 time_i = 0; time_i < current_input.time_range_count; time_i++ )
			{
				clip_time_range_t& time_range = current_input.time_range[ time_i ];

				if ( time_range.start > time_pos + 0.15 )
					closest_time = std::min( closest_time, time_range.start );

				if ( time_range.end > time_pos + 0.15 )
					closest_time = std::min( closest_time, time_range.end );
			}

			// check markers

			timeline_set_seek_time( closest_time );
		}
	}

	// ------------------------------------------------------------------------------------------
	// Positioning and Sizing Setup

	ImVec2      window_pos   = ImGui::GetWindowPos();
	ImVec2      cursor_pos   = ImGui::GetCursorPos();
	ImVec2      region_avail = ImGui::GetContentRegionAvail();

	g_timeline_size.x        = region_avail.x;
	// g_timeline_size.y        = region_avail.y - cursor_pos.y;
	g_timeline_size.y        = region_avail.y - ( text_height + style.FramePadding.y * 2 + style.ItemSpacing.y );

	ImVec2  window_cursor_pos( window_pos.x + cursor_pos.x, window_pos.y + cursor_pos.y );
	ImVec2  timeline_size = ImVec2( window_cursor_pos.x + g_timeline_size.x, window_cursor_pos.y + g_timeline_size.y );

	ImVec2  window_area_min( window_cursor_pos.x + TIMELINE_BORDER_SIZE, window_cursor_pos.y + TIMELINE_BORDER_SIZE );
	ImVec2  window_area_max( timeline_size.x - TIMELINE_BORDER_SIZE, timeline_size.y - TIMELINE_BORDER_SIZE );

	ImVec2  mouse_pos         = ImGui::GetMousePos();

	// is mouse within the frame here
	bool    mouse_hovered     = point_in_rect( mouse_pos, window_area_min, window_area_max );

	// ------------------------------------------------------------------------------------------
	// Draw Background

	ImColor main_border_color = ImColor( style.Colors[ ImGuiCol_FrameBgActive ].x, style.Colors[ ImGuiCol_FrameBgActive ].y, style.Colors[ ImGuiCol_FrameBgActive ].z );
	ImColor main_bg_color     = ImColor( style.Colors[ ImGuiCol_FrameBg ].x, style.Colors[ ImGuiCol_FrameBg ].y, style.Colors[ ImGuiCol_FrameBg ].z );

	// if ( !mouse_hovered )
	{
		main_bg_color.Value.x *= style.DisabledAlpha;
		main_bg_color.Value.y *= style.DisabledAlpha;
		main_bg_color.Value.z *= style.DisabledAlpha;
	}
	// main_bg_color.Value.w *= style.DisabledAlpha;

	// draw border and background on top
	draw_list->AddRectFilled( window_cursor_pos, timeline_size, main_border_color );
	draw_list->AddRectFilled( window_area_min, window_area_max, main_bg_color );

	// draw_list->AddText( window_cursor_pos, ImColor( 0, 255, 0 ), "TEST" );

	double time_pos = 0;
	p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

	int          seek_pos_start = window_cursor_pos.x + TIMELINE_BORDER_SIZE;
	int          seek_pos_end   = timeline_size.x - TIMELINE_BORDER_SIZE;
	int          seek_area      = seek_pos_end - seek_pos_start;

	// mouse position local to inside timeline window
	ImVec2       mouse_pos_local( mouse_pos.x - seek_pos_start, mouse_pos.y - timeline_size.y );

	// ------------------------------------------------------------------------------------------
	// TODO: Draw Thumbnails and Current Audio Track Waveform

	// ------------------------------------------------------------------------------------------
	// Draw sections

	static bool  section_resize           = false;
	static bool  section_resize_left      = false;
	static u32   section_resize_index     = 0;
	static float section_resize_seek_time = 0.f;

	static bool  just_selected_section = false;

	if ( draw_tabs_and_sections )
	{
		clip_input_video_t& current_input = g_clip_current_output->input[ g_clip_current_input ];

		for ( u32 time_i = 0; time_i < current_input.time_range_count; time_i++ )
		{
			clip_time_range_t& time_range        = current_input.time_range[ time_i ];

			int                section_pos_left  = seek_pos_start + seek_area * ( time_range.start / duration );
			int                section_pos_right = seek_pos_start + seek_area * ( time_range.end / duration );

			bool               is_selected       = g_selected_section == time_i;
			ImColor            border_color      = is_selected ? SECTION_COLOR_SELECT_BORDER : SECTION_COLOR_BORDER;

			draw_list->AddRectFilled( ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_right, window_area_max.y ), border_color );
			draw_list->AddRectFilled( ImVec2( section_pos_left + 1, window_area_min.y + 1 ), ImVec2( section_pos_right - 1, window_area_max.y - 1 ), SECTION_COLOR_BASE );

			char title[ 16 ]{};
			snprintf( title, 16, "%d", time_i );

			ImVec2 title_text_size = ImGui::CalcTextSize( title );

			ImVec2 title_size      = title_text_size;
			title_size.x += style.FramePadding.x * 2;
			title_size.y += style.FramePadding.y * 2;

			ImVec2 title_pos( section_pos_left + style.FramePadding.x, window_area_min.y + style.FramePadding.y );

			// draw titlebar
			draw_list->AddRectFilled( ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_right, window_area_min.y + title_size.y ), border_color );
			draw_list->AddText( title_pos, ImColor( 0, 0, 0 ), title );

			// draw grab points
			//draw_list->AddRectFilled( ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_left + 16, window_area_max.y ), SECTION_COLOR_BORDER );
			//draw_list->AddRectFilled( ImVec2( section_pos_right - 16, window_area_min.y ), ImVec2( section_pos_right, window_area_max.y ), SECTION_COLOR_BORDER );

			// if ( !capture_inputs )
			// 	continue;

			// check if we want to select this one
			if ( io.MouseClicked[ 0 ] && point_in_rect( mouse_pos, ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_right, window_area_max.y ) ) )
			{
				g_selected_section = time_i;
				just_selected_section = true;
			}

			if ( section_resize )
				continue;

			// if ( !io.MouseClicked[ 0 ] )
			// 	continue;

			// check left side for hit detection
			if ( point_in_rect( mouse_pos, ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_left + 4, window_area_max.y ) ) )
			{
				ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeEW );

				if ( !io.MouseClicked[ 0 ] )
					continue;

				section_resize           = true;
				section_resize_left      = true;
				section_resize_index     = time_i;
				section_resize_seek_time = 0.f;
			}

			// check right side for hit detection
			else if ( point_in_rect( mouse_pos, ImVec2( section_pos_right - 4, window_area_min.y ), ImVec2( section_pos_right, window_area_max.y ) ) )
			{
				ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeEW );

				if ( !io.MouseClicked[ 0 ] )
					continue;

				section_resize           = true;
				section_resize_index     = time_i;
				section_resize_seek_time = 0.f;
			}
		}

		// Process section resizing
		if ( section_resize )
		{
			static float min_start_time = 0.f;
			static float max_end_time   = duration;
			static bool  calc_times     = true;

			ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeEW );

			if ( io.MouseReleased[ 0 ] )
			{
				section_resize           = false;
				section_resize_left      = false;

				min_start_time           = 0.f;
				max_end_time             = duration;
				calc_times               = true;
				section_resize_seek_time = 0.f;
			}
			else
			{
				float              new_seek_percent = mouse_pos_local.x / seek_area;
				float              new_time_pos     = duration * new_seek_percent;

				clip_time_range_t& time_range       = current_input.time_range[ section_resize_index ];

				// get earliest start time and latest end time for other sections around this one
				if ( calc_times )
				{
					calc_times = false;

					for ( u32 time_i = 0; time_i < current_input.time_range_count; time_i++ )
					{
						if ( time_i == section_resize_index )
							continue;

						clip_time_range_t& scan_time = current_input.time_range[ time_i ];

						if ( scan_time.end <= time_range.start )
							min_start_time = std::max( scan_time.end, min_start_time );

						if ( scan_time.start >= time_range.end )
							max_end_time = std::min( scan_time.start, max_end_time );
					}
				}

				if ( section_resize_left )
				{
					// time_range.start         = std::clamp( std::min( new_time_pos, time_range.end - 0.1f ), min_start_time, time_range.end );
					time_range.start         = std::max( std::min( new_time_pos, time_range.end - 0.1f ), min_start_time );
					section_resize_seek_time = time_range.start;
				}
				else
				{
					// time_range.end           = std::clamp( std::max( new_time_pos, time_range.start + 0.1f ), time_range.start, max_end_time );
					time_range.end           = std::min( std::max( new_time_pos, time_range.start + 0.1f ), max_end_time );
					section_resize_seek_time = time_range.end;
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------
	// Seek Bar Hit Detection

	static bool  seek_drag        = false;
	static float new_seek_percent = 0.f;
	static float new_time_pos     = 0.f;

	if ( duration && capture_inputs /*&& !section_resize*/ )
	{
		if ( mouse_hovered && io.MouseClicked[ 0 ] )
		{
			seek_drag = true;

			if ( !just_selected_section )
				g_selected_section = UINT32_MAX;
		}

		if ( io.MouseReleased[ 0 ] )
			seek_drag = false;

		if ( seek_drag )
		{
			new_seek_percent = mouse_pos_local.x / seek_area;
			new_time_pos     = section_resize ? section_resize_seek_time : duration * new_seek_percent;

			timeline_set_seek_time( new_time_pos );

			// convert float to string in c
			// char time_pos_str[ 16 ];
			// gcvt( new_time_pos, 4, time_pos_str );
			// 
			// const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
			// int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}
		else
		{
			new_seek_percent = 0.f;
			new_time_pos     = 0.f;
		}
	}

	just_selected_section = false;

	// ------------------------------------------------------------------------------------------
	// Key binding controls

	if ( capture_inputs && g_selected_section != UINT32_MAX && ImGui::IsKeyPressed( ImGuiKey_Delete ) )
	{
		clip_remove_time_range( g_clip_current_output, g_clip_current_input, g_selected_section );
		g_selected_section = UINT32_MAX;
	}

	// ------------------------------------------------------------------------------------------
	// Draw 2 Marker positions

	for ( int marker_i = 0; marker_i < 2; marker_i++ )
	{
		if ( !g_timeline_marker_active[ marker_i ] )
			continue;

		double time_pos_seconds = duration ? duration / g_timeline_marker_times[ marker_i ] : 0.0;
		int    marker_pos_final = duration ? ( seek_area / time_pos_seconds ) + seek_pos_start : seek_pos_start;

		ImColor marker_color     = marker_i == 0 ? TIMELINE_MARKER_COLOR_A : TIMELINE_MARKER_COLOR_B;

		draw_list->AddLine(
		  ImVec2( marker_pos_final, window_cursor_pos.y + 1 ),
		  ImVec2( marker_pos_final, window_area_max.y - 1 ),
		  marker_color,
		  1.f );

		ImVec2 text_size     = ImGui::CalcTextSize( marker_i == 0 ? "A" : "B" );

		float  box_padding_x = ( text_size.x + style.FramePadding.x + style.FramePadding.x ) / 2.f;
		float  box_padding_y = ( text_size.y + style.FramePadding.y + style.FramePadding.y );

		// ImVec2 title_size    = title_text_size;
		// title_size.x += style.FramePadding.x * 2;
		// title_size.y += style.FramePadding.y * 2;

		// ImVec2 title_pos( section_pos_left + style.FramePadding.x, window_area_min.y + style.FramePadding.y );

		// draw_list->AddRectFilled( ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_right, window_area_min.y + title_size.y ), border_color );


		draw_list->AddRectFilled(
		  ImVec2( marker_pos_final - ( box_padding_x - 1 ), window_cursor_pos.y + 1 ),
		  ImVec2( marker_pos_final + box_padding_x, window_cursor_pos.y + 1 + box_padding_y ),
		  marker_color );

		draw_list->AddText( ImVec2( marker_pos_final - ( style.FramePadding.x - 1 ), window_cursor_pos.y + style.FramePadding.y ), ImColor( 0, 0, 0 ), marker_i == 0 ? "A" : "B" );
	}
	
	// ------------------------------------------------------------------------------------------
	// Draw Seek position on top of everything

	double time_pos_seconds = duration ? duration / time_pos : 0;
	int    seek_pos_final   = duration ? ( seek_area / time_pos_seconds ) + seek_pos_start : seek_pos_start;

	if ( section_resize )
	{
		seek_pos_final = seek_area / ( duration / section_resize_seek_time ) + seek_pos_start;
	}
	else if ( seek_drag )
	{
		// use seek drag position instead
		seek_pos_final = duration ? ( seek_area * new_seek_percent ) + seek_pos_start : seek_pos_start;
	}

	ImColor seek_color = ImColor( style.Colors[ ImGuiCol_Border ].x, style.Colors[ ImGuiCol_Border ].y, style.Colors[ ImGuiCol_Border ].z );

	draw_list->AddTriangleFilled(
	  ImVec2( seek_pos_final - 9, window_cursor_pos.y ),
	  ImVec2( seek_pos_final + 11, window_cursor_pos.y ),
	  ImVec2( seek_pos_final + 1, window_cursor_pos.y + 10 ),
	  seek_color );

	draw_list->AddLine(
	  ImVec2( seek_pos_final, window_cursor_pos.y + 4 ),
	  ImVec2( seek_pos_final, window_area_max.y - 1 ),
	  seek_color,
	  1.f );

	// ------------------------------------------------------------------------------------------
	// Set new cursor pos for normal imgui widget drawing

	ImGui::SetCursorPos( ImVec2( cursor_pos.x, cursor_pos.y + g_timeline_size.y + ( style.ItemSpacing.y * 1.5 ) ) );
}

