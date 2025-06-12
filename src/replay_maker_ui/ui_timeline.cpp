#include "main.h"
#include "clip/clip.h"

#include "imgui.h"

// native file dialog
// https://github.com/btzy/nativefiledialog-extended
#include "nfd.h"


constexpr int TIMELINE_HEIGHT = 40;
constexpr int TIMELINE_BORDER_SIZE = 1;

ImVec2        g_timeline_size;
ImVec2        g_timeline_pos;


static bool   point_in_rect( ImVec2 point, ImVec2 min_size, ImVec2 max_size )
{
	// return point[ 0 ] >= min_size.left && point[ 0 ] <= rect.right && point[ 1 ] <= rect.bottom && point[ 1 ] >= rect.top;
	return point[ 0 ] >= min_size[ 0 ] && point[ 0 ] <= max_size[ 0 ] && point[ 1 ] <= max_size[ 1 ] && point[ 1 ] >= min_size[ 1 ];
}


void draw_timeline()
{
	ImDrawList* draw_list          = ImGui::GetWindowDrawList();

	ImVec2      window_pos         = ImGui::GetWindowPos();
	ImVec2      cursor_pos         = ImGui::GetCursorPos();
	ImVec2      region_avail       = ImGui::GetContentRegionAvail();

	ImGuiStyle& style              = ImGui::GetStyle();

	g_timeline_size.x              = region_avail.x;
	g_timeline_size.y              = region_avail.y - cursor_pos.y;

	ImVec2      window_cursor_pos( window_pos.x + cursor_pos.x, window_pos.y + cursor_pos.y );
	ImVec2      timeline_size = ImVec2( window_cursor_pos.x + g_timeline_size.x, window_cursor_pos.y + g_timeline_size.y );

	ImVec2      window_area_min( window_cursor_pos.x + TIMELINE_BORDER_SIZE, window_cursor_pos.y + TIMELINE_BORDER_SIZE );
	ImVec2      window_area_max( timeline_size.x - TIMELINE_BORDER_SIZE, timeline_size.y - TIMELINE_BORDER_SIZE );

	// ------------------------------------------------------------------------------------------

	ImVec2      mouse_pos         = ImGui::GetMousePos();

	// is mouse within the frame here
	bool        mouse_hovered     = point_in_rect( mouse_pos, window_area_min, window_area_max );

	// ------------------------------------------------------------------------------------------
	// Draw Background

	ImColor     main_border_color = ImColor( style.Colors[ ImGuiCol_FrameBgActive ].x, style.Colors[ ImGuiCol_FrameBgActive ].y, style.Colors[ ImGuiCol_FrameBgActive ].z );
	ImColor     main_bg_color = ImColor( style.Colors[ ImGuiCol_FrameBg ].x, style.Colors[ ImGuiCol_FrameBg ].y, style.Colors[ ImGuiCol_FrameBg ].z );

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
	
	// ------------------------------------------------------------------------------------------
	// TODO: Draw Thumbnails and Current Audio Track
	
	// ------------------------------------------------------------------------------------------
	// Draw sections


	// ------------------------------------------------------------------------------------------
	// Seek Bar Hit Detection

	//ImVec2 mouse_pos = ImGui::GetMousePos();

	// is mouse within the frame here
	// bool mouse_hovered = point_in_rect( mouse_pos, window_area_min, window_area_max );
	
	double time_pos = 0;
	double duration = 0;
	p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );
	p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );

	int          seek_pos_start   = window_cursor_pos.x + TIMELINE_BORDER_SIZE;
	int          seek_pos_end     = timeline_size.x - TIMELINE_BORDER_SIZE;
	int          seek_area        = seek_pos_end - seek_pos_start;

	static bool  seek_drag        = false;
	static float new_seek_percent = 0.f;
	static float new_time_pos     = 0.f;

	if ( duration )
	{
		ImGuiIO& io = ImGui::GetIO();

		if ( mouse_hovered && io.MouseClicked[ 0 ] )
		{
			seek_drag = true;
		}

		if ( io.MouseReleased[ 0 ] )
			seek_drag = false;

		if ( seek_drag )
		{
			// get mouse position within timeline area
			ImVec2 mouse_pos_local( mouse_pos.x - seek_pos_start, mouse_pos.y - timeline_size.y );

			new_seek_percent = mouse_pos_local.x / seek_area;
			new_time_pos     = duration * new_seek_percent;

			// convert float to string in c
			char   time_pos_str[ 16 ];
			gcvt( new_time_pos, 4, time_pos_str );

			const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );

			printf( "TEMP\n" );
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

	if ( seek_drag )
	{
		// use seek drag position instead
		seek_pos_final = duration ? ( seek_area * new_seek_percent ) + seek_pos_start : seek_pos_start;
	}

	ImColor seek_color       = ImColor( style.Colors[ ImGuiCol_Border ].x, style.Colors[ ImGuiCol_Border ].y, style.Colors[ ImGuiCol_Border ].z ); 
	draw_list->AddTriangleFilled( ImVec2( seek_pos_final - 9, window_cursor_pos.y ), ImVec2( seek_pos_final + 11, window_cursor_pos.y ), ImVec2( seek_pos_final + 1, window_cursor_pos.y + 10 ), seek_color );
	draw_list->AddLine( ImVec2( seek_pos_final, window_cursor_pos.y + 4 ), ImVec2( seek_pos_final, window_area_max.y - 1 ), seek_color, 1.f );

	// ------------------------------------------------------------------------------------------
	// Set new cursor pos

	ImGui::SetCursorPos( ImVec2( cursor_pos.x, cursor_pos.y + g_timeline_size.y + ( style.ItemSpacing.y * 1.5 ) ) );
}

