#include "main.h"
#include "clip/clip.h"
#include "imgui.h"
#include "imgui_internal.h"


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

bool                        g_preset_combo_open;

extern clip_data_t*         g_clip_data;
extern clip_output_video_t* g_clip_current_output;
extern u32                  g_clip_current_output_index;
extern u32                  g_clip_current_input;
extern u32                  g_clip_current_group_source;
extern u32                  g_clip_current_group;

u32                         g_selected_section      = UINT32_MAX;

bool                        g_timeline_marker_active[ 2 ]{};
float                       g_timeline_marker_times[ 2 ]{};

constexpr double            TIMELINE_SKIP_TIME = 0.15;


struct duration_t
{
	float duration   = 0.f;
	float total_prev = 0.f;
};


void timeline_set_video()
{
}


void timeline_set_seek_time_fast( float seconds )
{
	char time_pos_str[ 16 ];
	gcvt( seconds, 4, time_pos_str );

	const char* cmd[] = { "seek", time_pos_str, "absolute", "keyframes", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void timeline_set_seek_time( float seconds )
{
	char time_pos_str[ 16 ];
	gcvt( seconds, 4, time_pos_str );

	const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void timeline_pause()
{
	const char* cmd[]   = { "set", "pause", "yes", NULL };
	int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
}


void timeline_advance( bool prev )
{
	if ( prev )
	{
		// try to change to the previous video
		if ( g_clip_current_group_source > 0 )
		{
			g_clip_current_group_source--;
			replay_editor_set_group( g_clip_current_output_index, g_clip_current_group, g_clip_current_group_source );
		}
	}
	else
	{
		// try to change to the next video
		clip_output_group_t* group = clip_get_group( g_clip_current_output, g_clip_current_group );

		if ( group && group->sources.size() > g_clip_current_group_source + 1 )
		{
			g_clip_current_group_source++;
			replay_editor_set_group( g_clip_current_output_index, g_clip_current_group, g_clip_current_group_source );
		}
	}
}


void timeline_seek( float seconds )
{
	double duration = 0;
	double time_pos = 0;
	p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );
	p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );

	float new_time_pos = time_pos + seconds;

	if ( new_time_pos < 0 )
	{
		// try to change to the previous video
		if ( g_clip_current_group_source > 0 )
		{
			g_clip_current_group_source--;
			replay_editor_set_group( g_clip_current_output_index, g_clip_current_group, g_clip_current_group_source );

			double duration = 0;
			p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );

			new_time_pos = duration + new_time_pos;
			timeline_set_seek_time_fast( new_time_pos );
		}
	}
	else if ( new_time_pos > duration )
	{
		// try to change to the next video
		clip_output_group_t* group = clip_get_group( g_clip_current_output, g_clip_current_group );

		if ( group && group->sources.size() > g_clip_current_group_source + 1 )
		{
			g_clip_current_group_source++;
			replay_editor_set_group( g_clip_current_output_index, g_clip_current_group, g_clip_current_group_source );

			new_time_pos = ( duration - time_pos ) - seconds;
			timeline_set_seek_time_fast( abs( new_time_pos ) );
		}
	}
	else
	{
		timeline_set_seek_time_fast( new_time_pos );
	}
}


void timeline_reset()
{
	g_selected_section = UINT32_MAX;

	// TODO: maybe later, have saved markers for each source video, so if your swapping back and forth to line something up, this can stay saved?
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
	p_mpv_set_option_string( g_mpv, "start", "0%" );

	g_preset_combo_open             = false;

	ImGuiIO&     io                 = ImGui::GetIO();
	ImGuiStyle&  style              = ImGui::GetStyle();

	ImDrawList*  draw_list          = ImGui::GetWindowDrawList();

	u32          focused_source     = 0;
	float        duration_total     = 0.f;
	static float new_time_pos       = 0.f;
	static bool  update_time_next_draw = false;

	static bool  stay_paused        = false;
	static bool  was_playing        = false;

	if ( update_time_next_draw )
	{
		timeline_set_seek_time( new_time_pos );
		new_time_pos          = 0.f;
		update_time_next_draw = false;
	}

	u32          change_to_source_i = UINT32_MAX;

	s32          paused             = 0;
	p_mpv_get_property( g_mpv, "pause", MPV_FORMAT_FLAG, &paused );

	std::vector< duration_t > durations;

	// TODO: make sure no inputs get captured if focused in a drop down or typing in a text box

	bool                      capture_inputs         = !io.WantTextInput && !g_preset_combo_open;

	// test WantCaptureMouseUnlessPopupClose?

	// ------------------------------------------------------------------------------------------

	clip_output_group_t*      group                  = clip_get_group( g_clip_current_output, g_clip_current_group );

	bool                      draw_tabs_and_sections = g_clip_current_output && g_clip_current_group_source != UINT32_MAX &&
	                              ( mpv_get_current_video() ? strcmp( g_clip_current_output->source[ g_clip_current_input ].path, mpv_get_current_video() ) == 0 : false );

	// ------------------------------------------------------------------------------------------
	// Draw tabs on top for which encode preset currently in use and current source video?
	float text_height = ImGui::GetFontSize();

	if ( ImGui::BeginTabBar( "##timeline_tabs" ) )
	{
		if ( g_clip_current_output )
		{
			for ( u32 group_i = 0; group_i < g_clip_current_output->groups.size(); group_i++ )
			{
				clip_output_group_t& group = g_clip_current_output->groups[ group_i ];

				// collect presets used
				char                 title[ 128 ]{};
				snprintf( title, 128, "Group: " );

				if ( group.presets.size() )
				{
					for ( u32 preset_i = 0; preset_i < group.presets.size(); preset_i++ )
					{
						clip_encode_preset_t& preset = g_clip_data->preset[ group.presets[ preset_i ] ];
						strcat( title, preset.name );

						if ( preset_i + 1 < group.presets.size() )
							strcat( title, ", " );
					}
				}
				else
				{
					strcat( title, "[NO PRESETS]" );

					ImGui::PushStyleColor( ImGuiCol_Tab, COLOR_BTN_RED );
					ImGui::PushStyleColor( ImGuiCol_TabHovered, COLOR_BTN_RED_HOVER );
					ImGui::PushStyleColor( ImGuiCol_TabSelected, COLOR_BTN_RED_ACTIVE );
				}

				bool selected_tab = g_clip_current_group == group_i;

				if ( selected_tab )
				{
					ImGui::PushStyleColor( ImGuiCol_Tab, style.Colors[ ImGuiCol_TabSelected ] );
				}

				ImGui::PushID( group_i + 1 );

				// if ( ImGui::TabItemButton( title, selected_tab ? ImGuiTabItemFlags_SetSelected : 0 ) )
				if ( ImGui::TabItemButton( title ) )
				{
					if ( g_clip_current_group != group_i )
					{
						g_clip_current_group = group_i;

						if ( group.sources.size() )
							replay_editor_set_group( g_clip_current_output_index, group_i, 0 );
					}
				}

				ImGui::PopID();

				if ( selected_tab )
				{
					ImGui::PopStyleColor();
				}

				if ( group.presets.empty() )
				{
					ImGui::PopStyleColor( 3 );
				}
			}

			ImGui::PushStyleColor( ImGuiCol_Tab, COLOR_PURPLE );
			ImGui::PushStyleColor( ImGuiCol_TabHovered, COLOR_PURPLE_HOVER );
			ImGui::PushStyleColor( ImGuiCol_TabSelected, COLOR_PURPLE_ACTIVE );

			if ( ImGui::TabItemButton( "New Group" ) )
			{
				g_clip_current_group = g_clip_current_output->groups.size();
				g_clip_current_output->groups.emplace_back();

				replay_editor_set_group( g_clip_current_output_index, g_clip_current_group, 0 );
			}

			ImGui::PopStyleColor( 3 );
		}

		ImGui::EndTabBar();
	}

	// ------------------------------------------------------------------------------------------
	{
		// ImGui::BeginDisabled( !draw_tabs_and_sections );
		// ImGui::SameLine();
		ImGui::BeginDisabled( !g_clip_current_output );

		if ( ImGui::Button( "Add Loaded Video" ) )
		{
			u32 i = clip_group_add_source( g_clip_current_output, g_clip_current_group, mpv_get_current_video() );
			replay_editor_set_group( g_clip_current_output_index, g_clip_current_group, i );
		}

		ImGui::SameLine();

		ImGui::PushStyleColor( ImGuiCol_ButtonActive, COLOR_BTN_RED_ACTIVE );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, COLOR_BTN_RED_HOVER );
		ImGui::PushStyleColor( ImGuiCol_Button, COLOR_BTN_RED );

		ImGui::EndDisabled();
		ImGui::BeginDisabled( !( g_clip_current_output && g_clip_current_group_source != UINT32_MAX ) );

		if ( ImGui::Button( "Delete Current Video" ) )
		{
			clip_group_remove_source( g_clip_current_output, g_clip_current_group, g_clip_current_group_source );
			timeline_reset();
			// replay_editor_reset();

			if ( group && g_clip_current_group_source > 0 && g_clip_current_group_source == group->sources.size() )
				g_clip_current_group_source--;

			if ( group->sources.size() )
			{
				g_clip_current_input = group->sources[ g_clip_current_group_source ].source_index;
				replay_editor_set_group( g_clip_current_output_index, g_clip_current_group, g_clip_current_input );
			}
			else
			{
				replay_editor_set_group( g_clip_current_output_index, g_clip_current_group, 0 );
				g_clip_current_input        = UINT32_MAX;
				g_clip_current_group_source = UINT32_MAX;
			}

			ImGui::EndDisabled();

			ImGui::PopStyleColor();
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();

			return;
		}

		ImGui::EndDisabled();

		ImGui::SameLine();

		ImGui::BeginDisabled( !group );

		if ( ImGui::Button( "Delete Current Group" ) )
		{
			g_clip_current_output->groups.remove( g_clip_current_group );

			if ( g_clip_current_group > 0 && g_clip_current_group == g_clip_current_output->groups.size() )
				g_clip_current_group--;

			timeline_reset();

			ImGui::PopStyleColor();
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();

			ImGui::EndDisabled();

			return;
		}

		ImGui::EndDisabled();

		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();

		if ( group )
		{
			ImGui::SameLine();
			ImGui::SeparatorEx( ImGuiSeparatorFlags_Vertical );
			ImGui::SameLine();

			draw_preset_dropdown( *g_clip_current_output, *group, true );
		}
	}

	// ------------------------------------------------------------------------------------------
	// Marker controls for creating new sections (Q and W, E to create as a section and clear markers)

	if ( group )
	{
		for ( clip_source_usage_t& source_use : group->sources )
		{
			clip_source_t& source = g_clip_current_output->source[ source_use.source_index ];
			durations.emplace_back( source.metadata.duration, duration_total );
			duration_total += source.metadata.duration;
		}
	}

	if ( durations.size() && capture_inputs )
	{
		timeline_marker_control( io, ImGuiKey_Q, 0 );
		timeline_marker_control( io, ImGuiKey_E, 1 );

		// Create section from markers, both markers don't need to be active, it will default to the start or end of the video
		if ( ImGui::IsKeyPressed( ImGuiKey_R, false ) )
		{
			float start_time = g_timeline_marker_active[ 0 ] ? g_timeline_marker_times[ 0 ] : 0.f;
			float end_time   = g_timeline_marker_active[ 1 ] ? g_timeline_marker_times[ 1 ] : durations[ focused_source ].duration;

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

	if ( durations.size() && draw_tabs_and_sections && capture_inputs && group )
	{
		clip_source_usage_t& source = group->sources[ g_clip_current_group_source ];

		if ( ImGui::IsKeyPressed( ImGuiKey_A, false ) )
		{
			double time_pos = 0;
			p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

			// search for the first notable time to snap to
			float closest_time = 0.f;

			if ( time_pos < TIMELINE_SKIP_TIME && g_clip_current_group_source > 0 )
			{
				clip_source_usage_t& prev_source = group->sources[ g_clip_current_group_source - 1 ];
				float                duration    = durations[ g_clip_current_group_source - 1 ].duration;

				for ( u32 time_i = 0; time_i < prev_source.time_range.size(); time_i++ )
				{
					clip_time_range_t& time_range = prev_source.time_range[ time_i ];

					if ( time_range.start < duration - TIMELINE_SKIP_TIME )
						closest_time = std::max( closest_time, time_range.start );

					if ( time_range.end < duration - TIMELINE_SKIP_TIME )
						closest_time = std::max( closest_time, time_range.end );
				}

				change_to_source_i = g_clip_current_group_source - 1;
				new_time_pos       = closest_time;
			}
			else
			{
				for ( u32 time_i = 0; time_i < source.time_range.size(); time_i++ )
				{
					clip_time_range_t& time_range = source.time_range[ time_i ];

					if ( time_range.start < time_pos - TIMELINE_SKIP_TIME )
						closest_time = std::max( closest_time, time_range.start );

					if ( time_range.end < time_pos - TIMELINE_SKIP_TIME )
						closest_time = std::max( closest_time, time_range.end );
				}

				timeline_set_seek_time( closest_time );
			}

			timeline_pause();
			stay_paused = true;
		}

		if ( ImGui::IsKeyPressed( ImGuiKey_D, false ) )
		{
			double time_pos = 0;
			p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

			// search for the first notable time to snap to
			float closest_time = durations[ focused_source ].duration;

			if ( time_pos > closest_time - TIMELINE_SKIP_TIME && g_clip_current_group_source + 1 < group->sources.size() )
			{
				clip_source_usage_t& prev_source = group->sources[ g_clip_current_group_source + 1 ];
				float                duration    = durations[ g_clip_current_group_source + 1 ].duration;
				closest_time                     = duration;

				for ( u32 time_i = 0; time_i < prev_source.time_range.size(); time_i++ )
				{
					clip_time_range_t& time_range = prev_source.time_range[ time_i ];

					if ( time_range.start > TIMELINE_SKIP_TIME )
						closest_time = std::min( closest_time, time_range.start );

					if ( time_range.end > TIMELINE_SKIP_TIME )
						closest_time = std::min( closest_time, time_range.end );
				}

				change_to_source_i = g_clip_current_group_source + 1;
				new_time_pos       = closest_time;
			}
			else
			{
				for ( u32 time_i = 0; time_i < source.time_range.size(); time_i++ )
				{
					clip_time_range_t& time_range = source.time_range[ time_i ];

					if ( time_range.start > time_pos + TIMELINE_SKIP_TIME )
						closest_time = std::min( closest_time, time_range.start );

					if ( time_range.end > time_pos + TIMELINE_SKIP_TIME )
						closest_time = std::min( closest_time, time_range.end );
				}

				timeline_set_seek_time( closest_time );
			}

			timeline_pause();
			stay_paused = true;
		}
	}

	// ------------------------------------------------------------------------------------------
	// Positioning and Sizing Setup

	ImVec2 window_pos   = ImGui::GetWindowPos();
	ImVec2 cursor_pos   = ImGui::GetCursorPos();
	ImVec2 region_avail = ImGui::GetContentRegionAvail();

	g_timeline_size.x   = region_avail.x;
	// g_timeline_size.y        = region_avail.y - cursor_pos.y;
	g_timeline_size.y   = region_avail.y - ( text_height + style.FramePadding.y * 2 + style.ItemSpacing.y );

	ImVec2 window_cursor_pos( window_pos.x + cursor_pos.x, window_pos.y + cursor_pos.y );
	ImVec2 timeline_size = ImVec2( window_cursor_pos.x + g_timeline_size.x, window_cursor_pos.y + g_timeline_size.y );

	ImVec2 window_area_min( window_cursor_pos.x + TIMELINE_BORDER_SIZE, window_cursor_pos.y + TIMELINE_BORDER_SIZE );
	ImVec2 window_area_max( timeline_size.x - TIMELINE_BORDER_SIZE, timeline_size.y - TIMELINE_BORDER_SIZE );

	ImVec2 mouse_pos     = ImGui::GetMousePos();

	// is mouse within the frame here
	bool   mouse_hovered = point_in_rect( mouse_pos, window_area_min, window_area_max );

	// ------------------------------------------------------------------------------------------
	// Draw Background

	{
		ImColor main_border_color = style.Colors[ ImGuiCol_Border ];
		ImColor main_bg_color     = style.Colors[ ImGuiCol_WindowBg ];

		// if ( !mouse_hovered )
		{
			main_bg_color.Value.x *= style.DisabledAlpha;
			main_bg_color.Value.y *= style.DisabledAlpha;
			main_bg_color.Value.z *= style.DisabledAlpha;
		}
		// main_bg_color.Value.w *= style.DisabledAlpha;

		// draw border and background on top
		draw_list->AddRectFilled( window_area_min, window_area_max, main_bg_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
		draw_list->AddRect( window_cursor_pos, timeline_size, main_border_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
	}

	// ------------------------------------------------------------------------------------------
	// Draw Source videos on top of bg

	if ( group )
	{
		float last_percent = 0.f;

		for ( size_t source_use_i = 0; source_use_i < group->sources.size(); source_use_i++ )
		{
			clip_source_usage_t& source_use        = group->sources[ source_use_i ];
			clip_source_t&       source            = g_clip_current_output->source[ source_use.source_index ];
			duration_t           duration          = durations[ source_use_i ];

			ImColor              main_border_color = style.Colors[ ImGuiCol_FrameBgActive ];
			ImColor              main_bg_color     = style.Colors[ ImGuiCol_FrameBg ];

			if ( g_clip_current_group_source != source_use_i )
			{
				main_bg_color.Value.x *= style.DisabledAlpha;
				main_bg_color.Value.y *= style.DisabledAlpha;
				main_bg_color.Value.z *= style.DisabledAlpha;

				main_border_color.Value.x *= style.DisabledAlpha;
				main_border_color.Value.y *= style.DisabledAlpha;
				main_border_color.Value.z *= style.DisabledAlpha;
			}
			// main_bg_color.Value.w *= style.DisabledAlpha;

			ImVec2 vid_area_min    = window_area_min;
			ImVec2 vid_area_max    = window_area_max;

			float  total_area      = window_area_max.x - window_area_min.x;

			float  percent_of_area = duration.duration / duration_total;
			float  offset          = timeline_size.x / duration_total;

			vid_area_min.x         = window_area_min.x + total_area * ( last_percent );
			vid_area_max.x         = window_area_min.x + total_area * ( percent_of_area + last_percent );

			last_percent += percent_of_area;

			// draw border and background on top
			draw_list->AddRectFilled( vid_area_min, vid_area_max, main_bg_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
			draw_list->AddRect( vid_area_min, vid_area_max, main_border_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );

			float  title_size = ImGui::GetFrameHeight();

			ImVec2 title_pos( vid_area_min.x + style.FramePadding.x, vid_area_min.y + style.FramePadding.y );
			ImVec2 title_max( vid_area_max.x, vid_area_min.y + title_size );

			// draw titlebar
			draw_list->AddRectFilled( vid_area_min, title_max, main_border_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
			draw_list->AddText( title_pos, ImColor( 255, 255, 255 ), source.filename );

			// check if we want to select this one
			if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) && point_in_rect( mouse_pos, vid_area_min, title_max ) )
			{
				change_to_source_i = source_use_i;
			}
		}
	}

	// draw_list->AddText( window_cursor_pos, ImColor( 0, 255, 0 ), "TEST" );

	double time_pos = 0;
	p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

	// int          seek_pos_start = window_cursor_pos.x + TIMELINE_BORDER_SIZE;
	// int          seek_pos_end   = timeline_size.x - TIMELINE_BORDER_SIZE;
	// int          seek_area      = seek_pos_end - seek_pos_start;

	// mouse position local to inside timeline window
	// ImVec2       mouse_pos_local( mouse_pos.x - seek_pos_start, mouse_pos.y - timeline_size.y );

	// ------------------------------------------------------------------------------------------
	// TODO: Draw Thumbnails and Current Audio Track Waveform

	// ------------------------------------------------------------------------------------------
	// Draw sections

	static bool       section_resize           = false;
	static bool       section_resize_left      = false;
	static u32        section_resize_index     = 0;
	static float      section_resize_seek_time = 0.f;

	static bool       just_selected_section    = false;

	static bool       seek_drag                = false;
	static float      new_seek_percent         = 0.f;

	ChVector< float > area_percents;

	if ( draw_tabs_and_sections && group )
	{
		float last_percent = 0.f;

		for ( size_t source_use_i = 0; source_use_i < group->sources.size(); source_use_i++ )
		{
			clip_source_usage_t& source_use      = group->sources[ source_use_i ];
			clip_source_t&       source          = g_clip_current_output->source[ source_use.source_index ];
			duration_t           duration        = durations[ source_use_i ];

			float                total_area      = window_area_max.x - window_area_min.x;

			float                percent_of_area = duration.duration / duration_total;
			float                offset          = timeline_size.x / duration_total;

			ImVec2               vid_area_min    = window_area_min;
			ImVec2               vid_area_max    = window_area_max;

			vid_area_min.x                       = window_area_min.x + total_area * ( last_percent );
			vid_area_max.x                       = window_area_min.x + total_area * ( percent_of_area + last_percent );

			int    seek_pos_start                = vid_area_min.x;
			int    seek_pos_end                  = vid_area_max.x;
			int    seek_area                     = seek_pos_end - seek_pos_start;

			// mouse position local to inside timeline window
			ImVec2 mouse_pos_local( mouse_pos.x - seek_pos_start, mouse_pos.y - timeline_size.y );

			area_percents.push_back( last_percent );
			last_percent += percent_of_area;

			bool mouse_hovered_area = point_in_rect( mouse_pos, vid_area_min, vid_area_max );

			for ( size_t time_i = 0; time_i < source_use.time_range.size(); time_i++ )
			{
				clip_time_range_t& time_range        = source_use.time_range[ time_i ];

				int                section_pos_left  = seek_pos_start + seek_area * ( time_range.start / source.metadata.duration );
				int                section_pos_right = seek_pos_start + seek_area * ( time_range.end / source.metadata.duration );

				bool               is_selected       = g_selected_section == time_i;
				ImColor            border_color      = is_selected ? SECTION_COLOR_SELECT_BORDER : SECTION_COLOR_BORDER;

				float              height_min        = window_area_min.y + ImGui::GetFrameHeight();

				draw_list->AddRectFilled( ImVec2( section_pos_left, height_min ), ImVec2( section_pos_right, window_area_max.y ), border_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
				draw_list->AddRectFilled( ImVec2( section_pos_left + 1, height_min + 1 ), ImVec2( section_pos_right - 1, window_area_max.y - 1 ), SECTION_COLOR_BASE, style.FrameRounding, ImDrawFlags_RoundCornersAll );

				char title[ 16 ]{};
				snprintf( title, 16, "%d", time_i );

				float  title_size = ImGui::GetFrameHeight();

				ImVec2 title_pos( section_pos_left + style.FramePadding.x, height_min + style.FramePadding.y );

				// draw titlebar
				draw_list->AddRectFilled( ImVec2( section_pos_left, height_min ), ImVec2( section_pos_right, height_min + title_size ), border_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
				draw_list->AddText( title_pos, ImColor( 0, 0, 0 ), title );

				// draw grab points
				//draw_list->AddRectFilled( ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_left + 16, window_area_max.y ), SECTION_COLOR_BORDER );
				//draw_list->AddRectFilled( ImVec2( section_pos_right - 16, window_area_min.y ), ImVec2( section_pos_right, window_area_max.y ), SECTION_COLOR_BORDER );

				// if ( !capture_inputs )
				// 	continue;

#if 0

				// check if we want to select this one
				if ( io.MouseClicked[ 0 ] && point_in_rect( mouse_pos, ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_right, window_area_max.y ) ) )
				{
					g_selected_section    = time_i;
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
#endif

#if 0
			// Process section resizing
				if ( section_resize )
				{
					static float min_start_time = 0.f;
					static float max_end_time   = duration.duration;
					static bool  calc_times     = true;

					ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeEW );

					if ( io.MouseReleased[ 0 ] )
					{
						section_resize           = false;
						section_resize_left      = false;

						min_start_time           = 0.f;
						max_end_time             = duration.duration;
						calc_times               = true;
						section_resize_seek_time = 0.f;
					}
					else
					{
						new_seek_percent              = mouse_pos_local.x / seek_area;
						new_time_pos                  = duration.duration * new_seek_percent;

						clip_time_range_t& time_range       = source_use.time_range[ section_resize_index ];

						// get earliest start time and latest end time for other sections around this one
						if ( calc_times )
						{
							calc_times = false;

							for ( u32 time_i = 0; time_i < source_use.time_range.size(); time_i++ )
							{
								if ( time_i == section_resize_index )
									continue;

								clip_time_range_t& scan_time = source_use.time_range[ time_i ];

								if ( scan_time.end <= time_range.start )
									min_start_time = std::max( scan_time.end, min_start_time );

								if ( scan_time.start >= time_range.end )
									max_end_time = std::min( scan_time.start, max_end_time );
							}
						}

						constexpr double SEEK_POS_SNAP = 0.5;

						// check if close enough to seek time to snap to
						if ( MAX( 0, time_pos - SEEK_POS_SNAP ) <= new_time_pos && new_time_pos <= MIN( duration.duration, time_pos + SEEK_POS_SNAP ) )
						{
							new_time_pos = time_pos;
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
#endif
			}

			// ------------------------------------------------------------------------------------------
			// Seek Bar Hit Detection

			// if ( g_clip_current_group_source == source_use_i )
			{
				if ( capture_inputs && !section_resize )
				{
					if ( mouse_hovered_area && io.MouseClicked[ 0 ] )
					{
						seek_drag = true;

						if ( !just_selected_section )
							g_selected_section = UINT32_MAX;
					}

					if ( io.MouseReleased[ 0 ] )
					{
						// timeline_set_seek_time( new_time_pos );
						seek_drag        = false;
						new_seek_percent = 0.f;
						new_time_pos     = 0.f;
					}

					if ( seek_drag )
					{
						if ( mouse_hovered_area && g_clip_current_group_source != source_use_i )
						{
							change_to_source_i = source_use_i;
						}

						new_seek_percent  = mouse_pos_local.x / seek_area;
						new_time_pos      = section_resize ? section_resize_seek_time : duration.duration * new_seek_percent;

						bool mouse_moving = g_mouse_delta[ 0 ] != 0 || g_mouse_delta[ 1 ] != 0;

						if ( mouse_moving || io.MouseClicked[ 0 ] )
						{
							// ui actually feels worse with this lol
							// timeline_set_seek_time_fast( new_time_pos );
							timeline_set_seek_time( new_time_pos );
						}
					}
				}
			}

			// ------------------------------------------------------------------------------------------
			// Draw Seek position on top of everything

			if ( g_clip_current_group_source == source_use_i )
			{
				double time_pos_seconds = duration.duration ? duration.duration / time_pos : 0;
				int    seek_pos_final   = duration.duration ? ( seek_area / time_pos_seconds ) + seek_pos_start : seek_pos_start;

				// if ( section_resize )
				// {
				// 	seek_pos_final = seek_area / ( duration / section_resize_seek_time ) + seek_pos_start;
				// }
				// else if ( seek_drag )
				if ( seek_drag && !section_resize )
				{
					// use seek drag position instead
					seek_pos_final = duration.duration ? ( seek_area * new_seek_percent ) + seek_pos_start : seek_pos_start;
				}

				ImColor seek_color = style.Colors[ ImGuiCol_Text ];

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
			}
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

#if 0
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
		  marker_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );

		draw_list->AddText( ImVec2( marker_pos_final - ( style.FramePadding.x - 1 ), window_cursor_pos.y + style.FramePadding.y ), ImColor( 0, 0, 0 ), marker_i == 0 ? "A" : "B" );
	}
#endif


	// ------------------------------------------------------------------------------------------
	// Set new cursor pos for normal imgui widget drawing

	// ImGui::SetCursorPos( ImVec2( cursor_pos.x, cursor_pos.y + g_timeline_size.y + ( style.ItemSpacing.y * 1.5 ) ) );
	ImGui::SetCursorPos( ImVec2( cursor_pos.x, cursor_pos.y + g_timeline_size.y + style.ItemSpacing.y ) );

	if ( !seek_drag && was_playing && paused )
	{
		if ( time_pos >= durations[ g_clip_current_group_source ].duration - 0.5f )
		{
			if ( group && group->sources.size() > g_clip_current_group_source + 1 )
			{
				change_to_source_i = ++g_clip_current_group_source;
				new_time_pos       = 0.f;
			}
		}
	}

	if ( change_to_source_i != UINT32_MAX )
	{
		char time_pos_str[ 16 ];
		gcvt( new_time_pos, 4, time_pos_str );

		int ret = p_mpv_set_option_string( g_mpv, "start", time_pos_str );

		// TODO: what if you had multiple mpv clients open, one for each source video....
		// then you could hot swap to a different one quickly
		replay_editor_set_group( g_clip_current_output_index, g_clip_current_group, change_to_source_i );
		// timeline_set_seek_time( new_time_pos );

		// HACK: despite trying to wait for mpv to load the video fully, or even starting at at the desired time
		// it still places the start time at 0 sometimes
		// so, do it the next time we draw the timeline if needed lol
		// it looks a bit odd, but works
		update_time_next_draw = true;

		if ( !stay_paused && !seek_drag && was_playing && paused )
		{
			const char* cmd[]   = { "set", "pause", "no", NULL };
			int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd );
		}

		if ( stay_paused )
			stay_paused = false;
	}

	was_playing = !paused;
}

