#include "main.h"
#include "clip/clip.h"

#include "imgui.h"

// native file dialog
// https://github.com/btzy/nativefiledialog-extended
#include "nfd.h"

#include <algorithm>


constexpr int               TIMELINE_HEIGHT      = 40;
constexpr int               TIMELINE_BORDER_SIZE = 1;
constexpr int               SECTION_TTILEBAR_HEIGHT = 12;

ImVec2                      g_timeline_size;
ImVec2                      g_timeline_pos;

extern clip_data_t*         g_clip_data;
extern clip_output_video_t* g_clip_current_output;
extern u32                  g_clip_current_input;


static bool   point_in_rect( ImVec2 point, ImVec2 min_size, ImVec2 max_size )
{
	// return point[ 0 ] >= min_size.left && point[ 0 ] <= rect.right && point[ 1 ] <= rect.bottom && point[ 1 ] >= rect.top;
	return point[ 0 ] >= min_size[ 0 ] && point[ 0 ] <= max_size[ 0 ] && point[ 1 ] <= max_size[ 1 ] && point[ 1 ] >= min_size[ 1 ];
}


const ImColor SECTION_COLOR_BORDER( 230, 230, 0 );
const ImColor SECTION_COLOR_BASE( 128, 128, 128 );


void          draw_timeline()
{
	ImGuiIO&    io           = ImGui::GetIO();

	ImDrawList* draw_list    = ImGui::GetWindowDrawList();

	ImVec2      window_pos   = ImGui::GetWindowPos();
	ImVec2      cursor_pos   = ImGui::GetCursorPos();
	ImVec2      region_avail = ImGui::GetContentRegionAvail();

	ImGuiStyle& style        = ImGui::GetStyle();

	g_timeline_size.x        = region_avail.x;
	g_timeline_size.y        = region_avail.y - cursor_pos.y;

	ImVec2  window_cursor_pos( window_pos.x + cursor_pos.x, window_pos.y + cursor_pos.y );
	ImVec2  timeline_size = ImVec2( window_cursor_pos.x + g_timeline_size.x, window_cursor_pos.y + g_timeline_size.y );

	ImVec2  window_area_min( window_cursor_pos.x + TIMELINE_BORDER_SIZE, window_cursor_pos.y + TIMELINE_BORDER_SIZE );
	ImVec2  window_area_max( timeline_size.x - TIMELINE_BORDER_SIZE, timeline_size.y - TIMELINE_BORDER_SIZE );

	// ------------------------------------------------------------------------------------------

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
	double duration = 0;
	p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );
	p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );

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

	if ( g_clip_current_output )
	{
		clip_input_video_t& current_input = g_clip_current_output->input[ g_clip_current_input ];

		for ( u32 time_i = 0; time_i < current_input.time_range_count; time_i++ )
		{
			clip_time_range_t& time_range        = current_input.time_range[ time_i ];

			int                section_pos_left  = seek_pos_start + seek_area * ( time_range.start / duration );
			int                section_pos_right = seek_pos_start + seek_area * ( time_range.end / duration );

			draw_list->AddRectFilled( ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_right, window_area_max.y ), SECTION_COLOR_BORDER );
			draw_list->AddRectFilled( ImVec2( section_pos_left + 1, window_area_min.y + 1 ), ImVec2( section_pos_right - 1, window_area_max.y - 1 ), SECTION_COLOR_BASE );

			// draw titlebar
			draw_list->AddRectFilled( ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_right, window_area_min.y + SECTION_TTILEBAR_HEIGHT ), SECTION_COLOR_BORDER );

			// draw grab points
			//draw_list->AddRectFilled( ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_left + 16, window_area_max.y ), SECTION_COLOR_BORDER );
			//draw_list->AddRectFilled( ImVec2( section_pos_right - 16, window_area_min.y ), ImVec2( section_pos_right, window_area_max.y ), SECTION_COLOR_BORDER );

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

	//ImVec2 mouse_pos = ImGui::GetMousePos();

	// is mouse within the frame here
	// bool mouse_hovered = point_in_rect( mouse_pos, window_area_min, window_area_max );

	static bool  seek_drag        = false;
	static float new_seek_percent = 0.f;
	static float new_time_pos     = 0.f;

	if ( duration /*&& !section_resize*/ )
	{
		if ( mouse_hovered && io.MouseClicked[ 0 ] )
		{
			seek_drag = true;
		}

		if ( io.MouseReleased[ 0 ] )
			seek_drag = false;

		if ( seek_drag )
		{
			new_seek_percent = mouse_pos_local.x / seek_area;
			new_time_pos     = section_resize ? section_resize_seek_time : duration * new_seek_percent;

			// convert float to string in c
			char time_pos_str[ 16 ];
			gcvt( new_time_pos, 4, time_pos_str );

			const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}
		else
		{
			new_seek_percent = 0.f;
			new_time_pos     = 0.f;
		}
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
	// Set new cursor pos

	ImGui::SetCursorPos( ImVec2( cursor_pos.x, cursor_pos.y + g_timeline_size.y + ( style.ItemSpacing.y * 1.5 ) ) );
}

