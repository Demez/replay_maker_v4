#include "main.h"
#include "clip/clip.h"
#include "imgui.h"
#include "imgui_internal.h"


// Timeline
namespace timeline
{
	double zoom         = 1.0;
	int    zoom_step    = 0;
	bool   do_scroll    = false;

	float  scroll_x     = 0.f;
	float  scroll_max_x = 0.f;
}


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

u32                         g_selected_section      = UINT32_MAX;

bool                        g_timeline_marker_active[ 2 ]{};
float                       g_timeline_marker_times[ 2 ]{};
u32                         g_timeline_marker_source = 0;

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

	// const char* cmd[] = { "seek", time_pos_str, "absolute", "keyframes", NULL };
	const char* cmd[] = { "seek", time_pos_str, "absolute", NULL };
	int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
}


void timeline_set_seek_time( float seconds )
{
	mpv_data_t* mpv = get_mpv_data();

	if ( !mpv )
		return;

	// wait for current seek to finish
	if ( mpv->seek_queued )
	{
		printf( "CANT SEEK YET\n" );
		return;
	}

	mpv->seek_queued      = true;
	mpv->seek_queued_time = sys_get_time_ms();

	char time_pos_str[ 16 ];
	gcvt( seconds, 4, time_pos_str );

	u64         _time = sys_get_time_ms();

	//const char* cmd[] = { "seek", time_pos_str, "absolute", "keyframes", NULL };
	const char* cmd[]   = { "seek", time_pos_str, "absolute", NULL };
	int         cmd_ret   = p_mpv_command_async( mpv->mpv, 1, cmd );

	u64         _end_time = sys_get_time_ms();

	if ( _end_time > _time )
		printf( "MPV SEEK QUEUED TIME - %u\n", _end_time - _time );
}


void timeline_pause()
{
	const char* cmd[]   = { "set", "pause", "yes", NULL };
	int         cmd_ret = p_mpv_command_async( get_mpv(), 0, cmd );
}


void timeline_advance( bool prev )
{
	if ( prev )
	{
		// try to change to the previous video
		if ( clip_data::current_group_source[ clip_data::current_group ] > 0 )
		{
			clip_data::current_group_source[ clip_data::current_group ]--;
			replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, clip_data::current_group_source[ clip_data::current_group ] );
		}
	}
	else
	{
		// try to change to the next video
		clip_group_t* group = clip_get_group( clip_data::current_clip, clip_data::current_group );

		if ( group && group->sources.size() > clip_data::current_group_source[ clip_data::current_group ] + 1 )
		{
			clip_data::current_group_source[ clip_data::current_group ]++;
			replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, clip_data::current_group_source[ clip_data::current_group ] );
		}
	}
}


void timeline_seek( float seconds )
{
	double duration = 0;
	double time_pos = 0;
	p_mpv_get_property( get_mpv(), "time-pos", MPV_FORMAT_DOUBLE, &time_pos );
	p_mpv_get_property( get_mpv(), "duration", MPV_FORMAT_DOUBLE, &duration );

	float new_time_pos = time_pos + seconds;

	if ( new_time_pos < 0 )
	{
		// try to change to the previous video
		if ( clip_data::current_group_source[ clip_data::current_group ] > 0 )
		{
			clip_data::current_group_source[ clip_data::current_group ]--;
			replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, clip_data::current_group_source[ clip_data::current_group ] );

			double duration = 0;
			p_mpv_get_property( get_mpv(), "duration", MPV_FORMAT_DOUBLE, &duration );

			new_time_pos = duration + new_time_pos;
			timeline_set_seek_time_fast( new_time_pos );
		}
	}
	else if ( new_time_pos > duration )
	{
		// try to change to the next video
		clip_group_t* group = clip_get_group( clip_data::current_clip, clip_data::current_group );

		if ( group && group->sources.size() > clip_data::current_group_source[ clip_data::current_group ] + 1 )
		{
			clip_data::current_group_source[ clip_data::current_group ]++;
			replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, clip_data::current_group_source[ clip_data::current_group ] );

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
	g_selected_section            = UINT32_MAX;
	g_timeline_marker_source      = UINT32_MAX;

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
			if ( clip_data::get_current_group_source() != g_timeline_marker_source )
			{
				// remove the other marker since it was
				if ( marker == 1 )
				{
					g_timeline_marker_active[ 0 ] = false;
					g_timeline_marker_times[ 0 ]  = 0.f;
				}
				else
				{
					g_timeline_marker_active[ 1 ] = false;
					g_timeline_marker_times[ 1 ]  = 0.f;
				}

				g_timeline_marker_source = clip_data::get_current_group_source();
			}

			double time_pos = 0;
			p_mpv_get_property( get_mpv(), "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

			g_timeline_marker_active[ marker ] = true;
			g_timeline_marker_times[ marker ]  = time_pos;
		}
	}
}


void delete_current_video( clip_group_t* group )
{
	u32 group_source = clip_data::get_current_group_source();

	clip_group_remove_source( clip_data::current_clip, clip_data::current_group, group_source );
	timeline_reset();
	// replay_editor_reset();

	//if ( group )
	//	clip_data::current_group_source.remove( group_source );

	if ( group && clip_data::current_group_source[ clip_data::current_group ] > 0 && clip_data::current_group_source[ clip_data::current_group ] == group->sources.size() )
		clip_data::current_group_source[ clip_data::current_group ]--;

	if ( group->sources.size() )
	{
		// clip_data::current_source = group->sources[ clip_data::current_group_source ].source_index;
		replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, clip_data::get_current_group_source() );
	}
	else
	{
		replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, 0 );
		clip_data::current_source        = UINT32_MAX;
		clip_data::current_group_source = UINT32_MAX;
	}
}


#if 0
void draw_timeline_title_button()
{
	ImGuiIO&    io              = ImGui::GetIO();
	ImDrawList* draw_list       = ImGui::GetWindowDrawList();

	// draw delete button
	const float CLOSE_BTN_SIZE  = ImGui::GetTextLineHeight();
	const float close_btn_diff  = ( ImGui::GetFrameHeight() - CLOSE_BTN_SIZE ) * 0.5f;
	ImVec2      close_btn_min   = { vid_area_max.x - ( close_btn_diff + CLOSE_BTN_SIZE ), vid_area_min.y + close_btn_diff };
	ImVec2      close_btn_max   = { close_btn_min.x + CLOSE_BTN_SIZE, close_btn_min.y + CLOSE_BTN_SIZE };

	int         close_btn_state = 0;
	ImColor     close_btn_color = COLOR_BTN_RED;

	if ( mouse_in_rect( close_btn_min, close_btn_max ) )
	{
		if ( io.MouseDown[ 0 ] )
		{
			close_btn_state                     = 2;
			close_btn_color                     = COLOR_BTN_RED_ACTIVE;
			delete_current_vid_on_mouse_release = true;
		}
		else
		{
			close_btn_state = 1;
			close_btn_color = COLOR_BTN_RED_HOVER;
		}
	}
	else
	{
		// delete_current_vid_on_mouse_release = false;
	}

	draw_list->AddRectFilled( close_btn_min, close_btn_max, close_btn_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
}
#endif


#if 0
void draw_time_range_move_button()
{
	// MOVE RIGHT (+1)
	if ( mouse_in_rect( order_btn_min, order_btn_max ) )
	{
		ignore_seek_drag = true;

		if ( source_use.time_range.size() > 1 )
		{
			if ( io.MouseDown[ 0 ] )
			{
				//btn_color.x *= 1.5;
				//btn_color.y *= 1.5;
				//btn_color.z *= 1.5;
				btn_color = style.Colors[ ImGuiCol_ButtonActive ];
			}
			else if ( io.MouseReleased[ 0 ] )
			{
				// btn_color.x *= 1.5;
				// btn_color.y *= 1.5;
				// btn_color.z *= 1.5;
				btn_color = style.Colors[ ImGuiCol_ButtonActive ];
			}
			else
			{
				//btn_color.x *= 1.25;
				//btn_color.y *= 1.25;
				//btn_color.z *= 1.25;
				btn_color = style.Colors[ ImGuiCol_ButtonHovered ];
			}
		}
	}

	//btn_color.w = 1;

	draw_list->AddRectFilled( order_btn_min, order_btn_max, ImColor( style.Colors[ ImGuiCol_WindowBg ] ), style.FrameRounding, ImDrawFlags_RoundCornersAll );
	draw_list->AddRectFilled( order_btn_min, order_btn_max, ImColor( btn_color ), style.FrameRounding, ImDrawFlags_RoundCornersAll );
	draw_list->AddText( { order_btn_min.x + text_diff, order_btn_min.y }, ImColor( ImVec4( 255, 255, 255, 255 ) ), ">" );
}
#endif


void timeline_handle_scroll( ImVec2 base_timeline_pos )
{
	const bool show_timeline = get_mpv_index() != EXTRA_VID_ID;

	if ( !show_timeline )
		return;

	if ( app::mouse_scroll_int[ 1 ] == 0 )
		return;

	u32 zoom_step = timeline::zoom_step;

	if ( ( app::mouse_scroll_int[ 1 ] < 0 && timeline::zoom_step > 0 ) || app::mouse_scroll_int[ 1 ] > 0 )
	{
		timeline::zoom_step += app::mouse_scroll_int[ 1 ];
		timeline::zoom_step = CLAMP( timeline::zoom_step, 0, 20 );
	}

	double new_zoom = 1.0;

	if ( timeline::zoom_step > 0 )
	{
		for ( int step = 0; step < timeline::zoom_step; step++ )
		{
			new_zoom *= 1.0 + 0.2;
		}
	}

	//new_zoom            = CLAMP( new_zoom, 1.0, 5.0 );
	double factor       = new_zoom / timeline::zoom;

	// get base timeline pos, and apply scroll offset to it
	ImVec2 base_screen_pos = ImGui::GetCursorScreenPos();
	base_screen_pos.x -= timeline::scroll_x;

	ImVec2 timeline_content_size = g_timeline_size;
	timeline_content_size.x *= new_zoom;

	float scroll_size = MAX( 0.0f, timeline_content_size.x - g_timeline_size.x );

	float snap_offset   = 70;

	// put mouse in local space
	float mouse_x = app::mouse_pos[ 0 ] - base_screen_pos.x;

	// scale by top left of zoomed in content size
	float new_x_pos     = scale_point_from_origin( mouse_x, 0, factor );

	// snap to edges
	if ( mouse_x < (snap_offset/2) )
		timeline::scroll_x = 0;
	else if ( mouse_x > ( g_timeline_size.x * timeline::zoom ) - (snap_offset/2) )
		timeline::scroll_x = scroll_size;
	else
		timeline::scroll_x -= new_x_pos;

	timeline::zoom      = new_zoom;
	timeline::do_scroll = true;
}


void timeline_draw()
{
	p_mpv_set_option_string( get_mpv(), "start", "0%" );

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

	u32    change_to_source_i = UINT32_MAX;

	//u64    _time2              = sys_get_time_ms();

	// MPV SLOWDOWN ?
	//s32          paused             = 0;
	//p_mpv_get_property( get_mpv(), "pause", MPV_FORMAT_FLAG, &paused );

	//u64 _end_time2 = sys_get_time_ms();

	//if ( _end_time2 > _time2 )
	//	printf( "MPV PAUSED - %u\n", _end_time2 - _time2 );

	//u64    _time    = sys_get_time_ms();

	// MPV SLOWDOWN - try observe property instead?
	// or use get_property_async?
	s32    paused             = 0;
	double time_pos           = 0;
	double duration           = 0;

	if ( get_mpv_data() )
	{
		time_pos = get_mpv_data()->time_pos;
		duration = get_mpv_data()->duration;
		paused   = get_mpv_data()->pause;
	}

	//p_mpv_get_property( get_mpv(), "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

	//u64 _end_time = sys_get_time_ms();

	//if ( _end_time > _time )
	//	printf( "MPV TIME POS - %u\n", _end_time - _time );

	std::vector< duration_t > durations;

	// TODO: make sure no inputs get captured if focused in a drop down or typing in a text box

	bool                      capture_inputs         = !io.WantTextInput;

	// test WantCaptureMouseUnlessPopupClose?

	// ------------------------------------------------------------------------------------------

	clip_group_t*      group                  = clip_get_group( clip_data::current_clip, clip_data::current_group );

	bool                      video_path_matches     = false;

	if ( clip_data::current_clip && clip_data::current_clip->source_count > 0 && clip_data::current_source != UINT32_MAX )
	{
		video_path_matches = ( mpv_get_current_video() ? strcmp( clip_data::current_clip->source[ clip_data::current_source ].path, mpv_get_current_video() ) == 0 : false );
	}

	bool                      draw_tabs_and_sections = clip_data::current_clip && clip_data::get_current_group_source() != UINT32_MAX && video_path_matches;

	// ------------------------------------------------------------------------------------------
	// Draw tabs on top for which encode preset currently in use and current source video?
	float text_height = ImGui::GetFontSize();

	if ( ImGui::BeginTabBar( "##timeline_tabs" ) )
	{
		if ( clip_data::current_clip )
		{
			for ( u32 group_i = 0; group_i < clip_data::current_clip->groups.size(); group_i++ )
			{
				clip_group_t& group = clip_data::current_clip->groups[ group_i ];

				std::string          group_name = clip_group_get_name( group );

				bool selected_tab = clip_data::current_group == group_i;

				if ( selected_tab )
				{
					ImGui::PushStyleColor( ImGuiCol_Tab, style.Colors[ ImGuiCol_TabSelected ] );
				}

				bool group_invalid = ( group.presets.empty() || group.sources.empty() );

				if ( group_invalid )
				{
					ImGui::PushStyleColor( ImGuiCol_Tab, COLOR_BTN_RED );
					ImGui::PushStyleColor( ImGuiCol_TabHovered, COLOR_BTN_RED_HOVER );
					ImGui::PushStyleColor( ImGuiCol_TabSelected, COLOR_BTN_RED_ACTIVE );
				}

				ImGui::PushID( group_i + 1 );

				// if ( ImGui::TabItemButton( title, selected_tab ? ImGuiTabItemFlags_SetSelected : 0 ) )
				if ( ImGui::TabItemButton( group_name.c_str() ) )
				{
					if ( clip_data::current_group != group_i )
					{
						replay_editor_set_group( clip_data::current_clip_index, group_i, clip_data::current_group_source[ group_i ] );
					}
				}

				ImGui::PopID();

				if ( group_invalid )
				{
					ImGui::PopStyleColor( 3 );
				}

				if ( selected_tab )
				{
					ImGui::PopStyleColor();
				}
			}

			#if 0
			ImGui::PushStyleColor( ImGuiCol_Tab, COLOR_PURPLE );
			ImGui::PushStyleColor( ImGuiCol_TabHovered, COLOR_PURPLE_HOVER );
			ImGui::PushStyleColor( ImGuiCol_TabSelected, COLOR_PURPLE_ACTIVE );

			if ( ImGui::TabItemButton( "New Group" ) )
			{
				u32 new_group = clip_data::current_clip->groups.size();
				clip_data::current_clip->groups.emplace_back();

				replay_editor_set_group( clip_data::current_clip_index, new_group, 0 );
			}

			ImGui::PopStyleColor( 3 );
			#endif

			ImGui::SameLine();

			//ImVec2 text_size = ImGui::CalcTextSize( "Create Group with Encode Preset" );

			//ImGui::PushItemWidth( style.FramePadding.x * 2 + text_size.x + ImGui::GetFrameHeight() );

			if ( ImGui::BeginCombo( "##New Group Preset", "Create Group with Encode Preset", ImGuiComboFlags_WidthFitPreview ) )
			{
				for ( u32 preset_i = 0; preset_i < clip_data::preset_count; preset_i++ )
				{
					clip_encode_preset_t& preset        = clip_data::preset[ preset_i ];
					bool                  preset_in_use = false;

					for ( u32 group_i = 0; group_i < clip_data::current_clip->groups.size(); group_i++ )
					{
						clip_group_t& group = clip_data::current_clip->groups[ group_i ];

						for ( u32 group_preset_i = 0; group_preset_i < group.presets.size(); group_preset_i++ )
						{
							if ( group.presets[ group_preset_i ] != preset_i )
								continue;

							preset_in_use = true;
							break;
						}

						if ( preset_in_use )
							break;
					}

					if ( preset_in_use )
						continue;

					if ( ImGui::Selectable( preset.name ) )
					{
						u32                  new_group = clip_data::current_clip->groups.size();
						clip_group_t& group     = clip_data::current_clip->groups.emplace_back();

						clip_group_add_preset( *clip_data::current_clip, group, preset_i );
						replay_editor_set_group( clip_data::current_clip_index, new_group, 0 );
					}
				}

				ImGui::EndCombo();
			}
		}

		ImGui::EndTabBar();
	}

	// ------------------------------------------------------------------------------------------

	{
		// ImGui::BeginDisabled( !draw_tabs_and_sections );
		// ImGui::SameLine();
		ImGui::BeginDisabled( !clip_data::current_clip );

		// if ( ImGui::Button( "Add Loaded Video" ) )
		// {
		// 	u32 i = clip_group_add_source( clip_data::current_clip, clip_data::current_group, mpv_get_current_video() );
		// 	replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, i );
		// }
		// 
		// ImGui::SameLine();

		ImGui::PushStyleColor( ImGuiCol_ButtonActive, COLOR_BTN_RED_ACTIVE );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, COLOR_BTN_RED_HOVER );
		ImGui::PushStyleColor( ImGuiCol_Button, COLOR_BTN_RED );

		ImGui::EndDisabled();
		// ImGui::BeginDisabled( !( clip_data::current_clip && clip_data::current_group_source != UINT32_MAX ) );
		//ImGui::BeginDisabled( !clip_data::current_clip );
		//
		//if ( ImGui::Button( "Delete Video" ) )
		//{
		//	clip_group_remove_source( clip_data::current_clip, clip_data::current_group, clip_data::current_group_source );
		//	clip_remove_entry( clip_data::current_clip_index );
		//	timeline_reset();
		//	replay_editor_reset();
		//	mpv_cmd_close_video();
		//
		//	ImGui::EndDisabled();
		//
		//	ImGui::PopStyleColor( 3 );
		//
		//	return;
		//}

		//ImGui::SameLine();
		//
		//if ( ImGui::Button( "Delete Current Video" ) )
		//{
		//	delete_current_video( group );
		//
		//	ImGui::EndDisabled();
		//	ImGui::PopStyleColor( 3 );
		//
		//	return;
		//}

		//ImGui::EndDisabled();
		//
		//ImGui::SameLine();

		ImGui::BeginDisabled( !group );

		if ( ImGui::Button( "Delete Current Group" ) )
		{
			clip_data::current_clip->groups.remove( clip_data::current_group );

			if ( clip_data::current_group > 0 && clip_data::current_group == clip_data::current_clip->groups.size() )
				clip_data::current_group--;

			timeline_reset();

			ImGui::EndDisabled();

			ImGui::PopStyleColor( 3 );

			return;
		}

		ImGui::EndDisabled();

		ImGui::PopStyleColor( 3 );

		if ( group )
		{
			ImGui::SameLine();
			ImGui::SeparatorEx( ImGuiSeparatorFlags_Vertical );
			ImGui::SameLine();

			draw_preset_dropdown( *clip_data::current_clip, *group, true );
		}
	}

	// ------------------------------------------------------------------------------------------
	// Marker controls for creating new sections (Q and W, E to create as a section and clear markers)

	if ( group )
	{
		for ( clip_source_usage_t& source_use : group->sources )
		{
			clip_source_t& source = clip_data::current_clip->source[ source_use.source_index ];

			if ( !source.file_missing )
			{
				durations.emplace_back( source.metadata.duration, duration_total );
				duration_total += source.metadata.duration;
			}
			else
			{
				// find the last valid time range for now
				float temp_duration = 0.f;

				for ( u32 time_i = 0; time_i < source_use.time_range.size(); time_i++ )
				{
					temp_duration = std::max( temp_duration, source_use.time_range[ time_i ].end );
				}

				durations.emplace_back( temp_duration, duration_total );
				duration_total += temp_duration;
			}
		}
	}

	// Marker Controls
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

			clip_group_add_time_range( clip_data::current_clip, *group, g_timeline_marker_source, start_time, end_time );

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
		clip_source_usage_t& source = group->sources[ clip_data::get_current_group_source() ];

		if ( ImGui::IsKeyPressed( ImGuiKey_A, false ) )
		{
			// search for the first notable time to snap to
			float closest_time = 0.f;

			if ( time_pos < TIMELINE_SKIP_TIME && clip_data::get_current_group_source() > 0 )
			{
				clip_source_usage_t& prev_source = group->sources[ clip_data::get_current_group_source() - 1 ];
				float                duration    = durations[ clip_data::get_current_group_source() - 1 ].duration;

				for ( u32 time_i = 0; time_i < prev_source.time_range.size(); time_i++ )
				{
					clip_time_range_t& time_range = prev_source.time_range[ time_i ];

					if ( time_range.start < duration - TIMELINE_SKIP_TIME )
						closest_time = std::max( closest_time, time_range.start );

					if ( time_range.end < duration - TIMELINE_SKIP_TIME )
						closest_time = std::max( closest_time, time_range.end );
				}

				change_to_source_i = clip_data::get_current_group_source() - 1;
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
			// search for the first notable time to snap to
			float closest_time = durations[ focused_source ].duration;

			if ( time_pos > closest_time - TIMELINE_SKIP_TIME && clip_data::get_current_group_source() + 1 < group->sources.size() )
			{
				clip_source_usage_t& prev_source = group->sources[ clip_data::get_current_group_source() + 1 ];
				float                duration    = durations[ clip_data::get_current_group_source() + 1 ].duration;
				closest_time                     = duration;

				for ( u32 time_i = 0; time_i < prev_source.time_range.size(); time_i++ )
				{
					clip_time_range_t& time_range = prev_source.time_range[ time_i ];

					if ( time_range.start > TIMELINE_SKIP_TIME )
						closest_time = std::min( closest_time, time_range.start );

					if ( time_range.end > TIMELINE_SKIP_TIME )
						closest_time = std::min( closest_time, time_range.end );
				}

				change_to_source_i = clip_data::get_current_group_source() + 1;
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

	ImVec2 window_pos              = ImGui::GetWindowPos();
	ImVec2 cursor_pos              = ImGui::GetCursorPos();
	ImVec2 region_avail            = ImGui::GetContentRegionAvail();

	g_timeline_size.x              = region_avail.x;
	// g_timeline_size.y        = region_avail.y - cursor_pos.y;
	g_timeline_size.y              = region_avail.y - ( text_height + style.FramePadding.y * 2 + style.ItemSpacing.y );

	//static float timeline_scroll_x = 0.f;

 	ImVec2 base_timeline_pos       = ImVec2( window_pos.x + cursor_pos.x, window_pos.y + cursor_pos.y );
	if ( mouse_hovering_area( base_timeline_pos, { base_timeline_pos.x + g_timeline_size.x, base_timeline_pos.y + g_timeline_size.y } ) )
		timeline_handle_scroll( base_timeline_pos );

	ImVec2 timeline_content_size = g_timeline_size;
	timeline_content_size.x *= timeline::zoom;
	timeline_content_size.y -= style.ScrollbarSize;

	ImVec2 window_cursor_pos( ( window_pos.x + cursor_pos.x ) - timeline::scroll_x, window_pos.y + cursor_pos.y );
	ImVec2 timeline_size = ImVec2( window_cursor_pos.x + timeline_content_size.x, window_cursor_pos.y + timeline_content_size.y );

	ImVec2 window_area_min( window_cursor_pos.x + TIMELINE_BORDER_SIZE, window_cursor_pos.y + TIMELINE_BORDER_SIZE );
	ImVec2 window_area_max( timeline_size.x - TIMELINE_BORDER_SIZE, timeline_size.y - TIMELINE_BORDER_SIZE );

	ImVec2 mouse_pos     = ImGui::GetMousePos();

	// is mouse within the frame here
	bool   mouse_hovered = mouse_hovering_area( window_area_min, window_area_max );

	ImGui::SetNextWindowContentSize( timeline_content_size );

	if ( timeline::do_scroll )
		ImGui::SetNextWindowScroll( { timeline::scroll_x, 0 } );

	// ZOOMING !!!
	if ( !ImGui::BeginChild( "##timeline_area", g_timeline_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar ) )
	{
		ImGui::EndChild();
		return;
	}

	ImGui::SetScrollY( 0 );

	// ------------------------------------------------------------------------------------------
	// Draw Background

	// only draw if no group is selected
	if ( !group || group->sources.empty() )
	{
		ImColor main_border_color = style.Colors[ ImGuiCol_Border ];
		ImColor main_bg_color     = style.Colors[ ImGuiCol_WindowBg ];

		main_bg_color.Value.x *= style.DisabledAlpha;
		main_bg_color.Value.y *= style.DisabledAlpha;
		main_bg_color.Value.z *= style.DisabledAlpha;

		// draw border and background on top
		draw_list->AddRectFilled( window_area_min, window_area_max, main_bg_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
		draw_list->AddRect( window_cursor_pos, timeline_size, main_border_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
	}

	// ------------------------------------------------------------------------------------------
	// Draw Source videos on top of bg

	bool delete_current_vid = false;

	if ( group )
	{
		float last_percent = 0.f;

		for ( size_t source_use_i = 0; source_use_i < group->sources.size(); source_use_i++ )
		{
			clip_source_usage_t& source_use        = group->sources[ source_use_i ];
			clip_source_t&       source            = clip_data::current_clip->source[ source_use.source_index ];
			duration_t           duration          = durations[ source_use_i ];

			ImColor              main_border_color = style.Colors[ ImGuiCol_FrameBgActive ];
			ImColor              main_bg_color     = style.Colors[ ImGuiCol_FrameBg ];

			if ( source.file_missing )
			{
				main_border_color = COLOR_BTN_RED;
				main_bg_color     = COLOR_RED_FRAME;
			}

			if ( clip_data::get_current_group_source() != source_use_i )
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

			// draw titlebar section for time ranges
			ImVec2 time_title_bar_max = title_max;
			time_title_bar_max.y += title_size;

			// ImVec4 time_range_color = style.Colors[ ImGuiCol_FrameBg ];
			ImVec4 time_range_color = main_border_color;
			time_range_color.w      = 0.25;

			draw_list->AddRectFilled( vid_area_min, time_title_bar_max, ImColor( time_range_color ) );

			// draw titlebar
			draw_list->AddRectFilled( vid_area_min, title_max, main_border_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
			draw_list->AddText( title_pos, ImColor( 255, 255, 255 ), source.filename );

			// DO NOT DO THIS !!!!
			// this can clash with seeking slightly
			// 
			// since below in that area, seeking can be done, but then you can also resize the used sections,
			// so the area above is nice for seeking freely,
			// and i even added section start and end time seek time snapping on that top title bar
			// 
			// but then if the close button is there, you have to press below it
			// pressing the delete key on the keyboard is a better approach
			#if 0
			// draw delete button
			const float     CLOSE_BTN_SIZE  = ImGui::GetTextLineHeight();
			const float     close_btn_diff  = ( ImGui::GetFrameHeight() - CLOSE_BTN_SIZE ) * 0.5f;
			ImVec2          close_btn_min   = { vid_area_max.x - ( close_btn_diff + CLOSE_BTN_SIZE ), vid_area_min.y + close_btn_diff };
			ImVec2          close_btn_max   = { close_btn_min.x + CLOSE_BTN_SIZE, close_btn_min.y + CLOSE_BTN_SIZE };

			ImColor         close_btn_color = COLOR_BTN_RED;

			if ( mouse_in_rect( close_btn_min, close_btn_max ) )
			{
				if ( io.MouseDown[ 0 ] )
				{
					close_btn_color = COLOR_BTN_RED_ACTIVE;
				}
				else if ( io.MouseReleased[ 0 ] )
				{
					close_btn_color    = COLOR_BTN_RED_ACTIVE;
					delete_current_vid = true;
				}
				else
				{
					close_btn_color = COLOR_BTN_RED_HOVER;
				}
			}

			draw_list->AddRectFilled( close_btn_min, close_btn_max, close_btn_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
			#endif

			if ( clip_data::get_current_group_source() == source_use_i && capture_inputs && g_selected_section == UINT32_MAX && ImGui::IsKeyPressed( ImGuiKey_Delete, false ) )
			{
				delete_current_vid = true;
			}

			// TODO: draw sorting buttons for ordering clips

			// check if we want to select this one
			// if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) && point_in_rect( mouse_pos, vid_area_min, title_max ) )
			// {
			// 	change_to_source_i = source_use_i;
			// }
		}
	}

	// draw_list->AddText( window_cursor_pos, ImColor( 0, 255, 0 ), "TEST" );

	// int          seek_pos_start = window_cursor_pos.x + TIMELINE_BORDER_SIZE;
	// int          seek_pos_end   = timeline_size.x - TIMELINE_BORDER_SIZE;
	// int          seek_area      = seek_pos_end - seek_pos_start;

	// mouse position local to inside timeline window
	// ImVec2       mouse_pos_local( mouse_pos.x - seek_pos_start, mouse_pos.y - timeline_size.y );

	// ------------------------------------------------------------------------------------------
	// TODO: Draw Thumbnails and Current Audio Track Waveform

	// ------------------------------------------------------------------------------------------
	// Draw sections

	static bool       section_resize                = false;
	static bool       section_resize_mouse_wait     = false;
	static u32        section_resize_source         = 0;
	static bool       section_resize_left           = false;
	static u32        section_resize_index          = 0;
	static float      section_resize_seek_time      = 0.f;

	const float       section_snap_size_base        = 4 * app::dpi;

	static bool       just_selected_section         = false;

	static bool       seek_drag                     = false;
	static float      new_seek_percent              = 0.f;

	bool              seek_time_override            = false;
	bool              ignore_seek_drag              = false;

	// shift time range params
	static u32        shift_time_range_hovered_prev = UINT32_MAX;
	u32               shift_time_range_hovered      = UINT32_MAX;
	bool              shift_time_range              = false;
	bool              shift_time_range_dir          = false;
	u32               shift_time_range_vid          = 0;

	ChVector< float > area_percents;

	if ( draw_tabs_and_sections && group )
	{
		float last_percent = 0.f;

		for ( size_t video_i = 0; video_i < group->sources.size(); video_i++ )
		{
			duration_t duration        = durations[ video_i ];

			float      total_area      = window_area_max.x - window_area_min.x;

			float      percent_of_area = duration.duration / duration_total;
			float      offset          = timeline_size.x / duration_total;

			ImVec2     vid_area_min    = window_area_min;
			ImVec2     vid_area_max    = window_area_max;

			vid_area_min.x             = window_area_min.x + total_area * ( last_percent );
			vid_area_max.x             = window_area_min.x + total_area * ( percent_of_area + last_percent );

			int    seek_pos_start      = vid_area_min.x;
			int    seek_pos_end        = vid_area_max.x;
			int    seek_area           = seek_pos_end - seek_pos_start;

			// mouse position local to inside timeline window
			ImVec2 mouse_pos_local( mouse_pos.x - seek_pos_start, mouse_pos.y - timeline_size.y );

			area_percents.push_back( last_percent );
			last_percent += percent_of_area;

			bool mouse_hovered_area = mouse_hovering_area( vid_area_min, vid_area_max );

			if ( video_i < group->sources.size() )
			{
				clip_source_usage_t& source_use         = group->sources[ video_i ];
				clip_source_t&       source             = clip_data::current_clip->source[ source_use.source_index ];

				// ----------------------------------------------------------------------------------------------------------
				// Draw Each Time Range in Video Source
				for ( size_t time_i = 0; time_i < source_use.time_range.size(); time_i++ )
				{
					clip_time_range_t& time_range        = source_use.time_range[ time_i ];

					int                section_pos_left  = seek_pos_start + seek_area * ( time_range.start / source.metadata.duration );
					int                section_pos_right = seek_pos_start + seek_area * ( time_range.end / source.metadata.duration );

					bool               is_selected       = clip_data::get_current_group_source() == video_i && g_selected_section == time_i;
					ImColor            border_color      = is_selected ? SECTION_COLOR_SELECT_BORDER : SECTION_COLOR_BORDER;

					float              height_min        = window_area_min.y + ImGui::GetFrameHeight();

					draw_list->AddRectFilled( ImVec2( section_pos_left, height_min ), ImVec2( section_pos_right, window_area_max.y ), border_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );
					draw_list->AddRectFilled( ImVec2( section_pos_left + 1, height_min + 1 ), ImVec2( section_pos_right - 1, window_area_max.y - 1 ), SECTION_COLOR_BASE, style.FrameRounding, ImDrawFlags_RoundCornersAll );

					char title[ TIME_BUFFER ]{};
					// snprintf( title, 16, "%u", time_i );
					util_format_time( title, TIME_BUFFER, time_range.end - time_range.start, true );

					float  title_size = ImGui::GetFrameHeight();

					ImVec2 title_pos( section_pos_left + style.FramePadding.x, height_min + style.FramePadding.y );

					// draw titlebar
					draw_list->AddRectFilled( ImVec2( section_pos_left, height_min ), ImVec2( section_pos_right, height_min + title_size ), border_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );

					ImGui::PushClipRect( ImVec2( section_pos_left, height_min ), ImVec2( section_pos_right, height_min + title_size ), true );

					draw_list->AddText( title_pos, ImColor( 0, 0, 0 ), title );

					ImGui::PopClipRect();

#if 0
					// Draw order changing buttons on time range

					const float ORDER_BTN_SIZE        = ImGui::GetTextLineHeight();
					const float text_width            = ImGui::CalcTextSize( "<" ).x;
					const float ORDER_BTN_WIDTH       = text_width + style.FramePadding.x;
					const float order_btn_diff        = ( ImGui::GetFrameHeight() - ORDER_BTN_WIDTH ) * 0.5f;
					const float order_btn_diff_height = ( ImGui::GetFrameHeight() - ORDER_BTN_SIZE ) * 0.5f;

					ImVec2      order_btn_min         = { section_pos_right - ( order_btn_diff + text_width + 2 ), height_min + order_btn_diff_height };
					ImVec2      order_btn_max         = { order_btn_min.x + ORDER_BTN_WIDTH, order_btn_min.y + ORDER_BTN_SIZE };

					const float text_diff             = ( ORDER_BTN_WIDTH - text_width ) * 0.5f;

					// offset it back more for left button start pos
					order_btn_min.x -= ORDER_BTN_WIDTH + 2;
					order_btn_max.x -= ORDER_BTN_WIDTH + 2;

					// 0 == move left, 1 == move right
					for ( u32 m = 0; m < 2; m++ )
					{
						ImVec4 btn_color = style.Colors[ ImGuiCol_Button ];
						bool   disabled  = source_use.time_range.size() <= 1;

						if ( !disabled )
						{
							if ( m == 0 && time_i == 0 )
							{
								disabled = true;
							}
							else if ( m == 1 && time_i + 1 == source_use.time_range.size() )
							{
								disabled = true;
							}
						}

						// MOVE RIGHT (+1)
						if ( mouse_in_rect( order_btn_min, order_btn_max ) )
						{
							shift_time_range_hovered = time_i;

							ignore_seek_drag = true;

							if ( !disabled )
							{
								if ( io.MouseDown[ 0 ] )
								{
									//btn_color.x *= 1.5;
									//btn_color.y *= 1.5;
									//btn_color.z *= 1.5;
									btn_color = style.Colors[ ImGuiCol_ButtonActive ];
								}
								else if ( io.MouseReleased[ 0 ] )
								{
									// btn_color.x *= 1.5;
									// btn_color.y *= 1.5;
									// btn_color.z *= 1.5;
									btn_color = style.Colors[ ImGuiCol_ButtonActive ];

									shift_time_range     = true;
									shift_time_range_vid = video_i;
									shift_time_range_dir = m == 0 ? false : true; 

								}
								else
								{
									//btn_color.x *= 1.25;
									//btn_color.y *= 1.25;
									//btn_color.z *= 1.25;
									btn_color = style.Colors[ ImGuiCol_ButtonHovered ];
								}
							}
						}

						if ( disabled )
						{
							btn_color.w *= style.DisabledAlpha;
						}

						//btn_color.w = 1;

						// draw window bg to darken button first
						// draw_list->AddRectFilled( order_btn_min, order_btn_max, ImColor( style.Colors[ ImGuiCol_WindowBg ] ), style.FrameRounding, ImDrawFlags_RoundCornersAll );
						draw_list->AddRectFilled( order_btn_min, order_btn_max, ImColor( {64, 64, 64, 255} ), style.FrameRounding, ImDrawFlags_RoundCornersAll );
						draw_list->AddRectFilled( order_btn_min, order_btn_max, ImColor( btn_color ), style.FrameRounding, ImDrawFlags_RoundCornersAll );

						// TODO: USE AddTriangleFilled instead
						// draw_list->AddText( { order_btn_min.x + text_diff, order_btn_min.y }, ImColor( disabled ? style.Colors[ ImGuiCol_TextDisabled ] : style.Colors[ ImGuiCol_Text ] ), m == 0 ? ">" : "<" );
						draw_list->AddText( { order_btn_min.x + text_diff, order_btn_min.y }, ImColor( style.Colors[ ImGuiCol_Text ] ), m == 0 ? "<" : ">" );

						// offset next button
						order_btn_min.x += ORDER_BTN_WIDTH + 2;
						order_btn_max.x += ORDER_BTN_WIDTH + 2;
					}
					#endif

					// draw grab points
					//draw_list->AddRectFilled( ImVec2( section_pos_left, window_area_min.y ), ImVec2( section_pos_left + 16, window_area_max.y ), SECTION_COLOR_BORDER );
					//draw_list->AddRectFilled( ImVec2( section_pos_right - 16, window_area_min.y ), ImVec2( section_pos_right, window_area_max.y ), SECTION_COLOR_BORDER );

					// if ( !capture_inputs )
					// 	continue;

					// ------------------------------------------------------------------------------------------
					// check cursor snapping above time range start

					// TODO: separate this, build the boundaries for each time range in an earlier for loop
					// then make grab boundaries for each boundary
					// then, you can limit the size of these boundaries 

					int   snap_to_time_range = 0;
					float section_snap_size  = section_snap_size_base;

					// if the section is too small, shrink the snap size
					if ( section_snap_size * 2 > ( section_pos_right - section_pos_left ) )
					{
						section_snap_size = ( section_pos_right - section_pos_left ) * 0.5;
					}

					// clamp to timeline area
					// float section_pos_snap_start_l  = std::max( section_pos_left - section_snap_size, window_area_min.x );
					float section_pos_snap_start_l  = section_pos_left;
					float section_pos_snap_start_r  = std::min( section_pos_left + section_snap_size, window_area_max.x );
					// float section_pos_snap_start_r  = CLAMP( section_pos_left + section_snap_size, float(section_pos_right), window_area_max.x );

					float section_pos_snap_end_l    = std::max( section_pos_right - section_snap_size, window_area_min.x );
					//float section_pos_snap_end_l    = section_pos_right - section_snap_size;
					//float section_pos_snap_end_r    = std::max( section_pos_right + section_snap_size, window_area_max.x );
					float section_pos_snap_end_r    = section_pos_right;

					if ( mouse_hovered )
					{
						if ( mouse_in_rect( ImVec2( section_pos_snap_start_l, window_area_min.y ), ImVec2( section_pos_snap_start_r, height_min ) ) )
						{
							ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
							snap_to_time_range = 1;
						}

						// check to the left of the section in the timeline
						//else if( mouse_in_rect( ImVec2( section_pos_left - section_snap_size, height_min ), ImVec2( section_pos_left, window_area_max.y ) ) )
						//{
						//	//ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
						//	snap_to_time_range = 1;
						//}

						// check snap to end time
						else if ( mouse_in_rect( ImVec2( section_pos_snap_end_l, window_area_min.y ), ImVec2( section_pos_snap_end_r, height_min ) ) )
						{
							ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
							snap_to_time_range = 2;
						}

						// check to the right of the section in the timeline
						//else if ( mouse_in_rect( ImVec2( section_pos_right, height_min ), ImVec2( section_pos_right + section_snap_size, window_area_max.y ) ) )
						//{
						//	//ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
						//	snap_to_time_range = 2;
						//}
					}

					if ( snap_to_time_range > 0 && io.MouseClicked[ 0 ] )
					{
						seek_time_override = true;
						new_time_pos       = snap_to_time_range == 1 ? time_range.start : time_range.end;
						new_seek_percent   = new_time_pos / duration.duration;

						if ( mouse_hovered_area && clip_data::get_current_group_source() != video_i )
						{
							change_to_source_i = video_i;
						}
					}

					// check if we want to select this one
					if ( mouse_hovered && io.MouseClicked[ 0 ] && mouse_in_rect( ImVec2( section_pos_snap_start_l, height_min ), ImVec2( section_pos_snap_end_r, window_area_max.y ) ) )
					{
						g_selected_section    = time_i;
						just_selected_section = true;
					}

					// ------------------------------------------------------------------------------------------
					// Section Resize Start

					if ( mouse_hovered && !section_resize && !seek_time_override )
					{
						// if ( !io.MouseClicked[ 0 ] )
						// 	continue;

						// check left side for hit detection
						if ( mouse_in_rect( ImVec2( section_pos_snap_start_l, height_min ), ImVec2( section_pos_snap_start_r, window_area_max.y ) ) )
						{
							ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeEW );

							if ( io.MouseClicked[ 0 ] )
							{
								//seek_time_override       = true;
								section_resize_mouse_wait = true;
								section_resize_left       = true;
								section_resize_index      = time_i;
								section_resize_source     = video_i;
								section_resize_seek_time  = 0.f;
							}
						}

						// check right side for hit detection
						else if ( mouse_in_rect( ImVec2( section_pos_snap_end_l, height_min ), ImVec2( section_pos_snap_end_r, window_area_max.y ) ) )
						{
							ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeEW );

							if ( io.MouseClicked[ 0 ] )
							{
								//seek_time_override       = true;
								section_resize_mouse_wait = true;
								section_resize_source     = video_i;
								section_resize_index      = time_i;
								section_resize_seek_time  = 0.f;
							}
						}
					}
				}

				// ------------------------------------------------------------------------------------------
				// Draw 2 Marker positions

				if ( g_timeline_marker_source == video_i )
				{
					for ( int marker_i = 0; marker_i < 2; marker_i++ )
					{
						if ( !g_timeline_marker_active[ marker_i ] )
							continue;

						double  time_pos_seconds = duration.duration ? duration.duration / g_timeline_marker_times[ marker_i ] : 0.0;
						int     marker_pos_final = duration.duration ? ( seek_area / time_pos_seconds ) + seek_pos_start : seek_pos_start;

						ImColor marker_color     = marker_i == 0 ? TIMELINE_MARKER_COLOR_A : TIMELINE_MARKER_COLOR_B;

						draw_list->AddLine(
						  ImVec2( marker_pos_final, window_cursor_pos.y + ImGui::GetFrameHeight() + 1 ),
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
						  ImVec2( marker_pos_final - ( box_padding_x - 1 ), window_cursor_pos.y + ImGui::GetFrameHeight() + 1 ),
						  ImVec2( marker_pos_final + box_padding_x, window_cursor_pos.y + ImGui::GetFrameHeight() + 1 + box_padding_y ),
						  marker_color, style.FrameRounding, ImDrawFlags_RoundCornersAll );

						draw_list->AddText( ImVec2( marker_pos_final - ( style.FramePadding.x - 1 ), window_cursor_pos.y + ImGui::GetFrameHeight() + style.FramePadding.y ), ImColor( 0, 0, 0 ), marker_i == 0 ? "A" : "B" );
					}
				}

				// ------------------------------------------------------------------------------------------
				// Process section resizing

				if ( !seek_time_override && ( section_resize || section_resize_mouse_wait ) && section_resize_source == video_i )
				{
					static float min_start_time = 0.f;
					static float max_end_time   = duration.duration;
					static bool  calc_times     = true;

					ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeEW );

					if ( !io.MouseDown[ 0 ] )
					{
						section_resize            = false;
						section_resize_mouse_wait = false;
						section_resize_left       = false;

						min_start_time            = 0.f;
						max_end_time              = duration.duration;
						calc_times                = true;
						section_resize_seek_time  = 0.f;
					}
					else
					{
						clip_time_range_t& time_range = source_use.time_range[ section_resize_index ];

						if ( section_resize_mouse_wait )
						{
							if ( !just_selected_section && app::mouse_delta[ 0 ] != 0 )
							{
								section_resize            = true;
								section_resize_mouse_wait = false;
							}
							else if ( just_selected_section )
							{
								seek_time_override = true;
								new_time_pos     = section_resize_left ? time_range.start : time_range.end;
								new_seek_percent = new_time_pos / duration.duration;

								if ( mouse_hovered_area && clip_data::get_current_group_source() != video_i )
								{
									change_to_source_i = video_i;
								}
							}
						}

						if ( section_resize )
						{
							seek_time_override = true;
							new_seek_percent   = mouse_pos_local.x / seek_area;
							new_time_pos       = duration.duration * new_seek_percent;

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
				}
			}

			// ------------------------------------------------------------------------------------------
			// Seek Bar Hit Detection

			// if ( clip_data::current_group_source == source_use_i )
			{
				if ( mouse_hovered && capture_inputs && !section_resize && !seek_time_override && !ignore_seek_drag )
				{
					if ( mouse_hovered_area && io.MouseClicked[ 0 ] )
					{
						seek_drag = true;

						if ( !just_selected_section )
							g_selected_section = UINT32_MAX;
					}

					if ( seek_drag && !io.MouseDown[ 0 ] )
					{
						// timeline_set_seek_time( new_time_pos );
						seek_drag        = false;
						new_seek_percent = 0.f;
						new_time_pos     = 0.f;
					}

					if ( seek_drag && mouse_hovered_area )
					{
						if ( mouse_hovered_area && clip_data::get_current_group_source() != video_i )
						{
							change_to_source_i = video_i;
						}

						new_seek_percent  = mouse_pos_local.x / seek_area;
						new_time_pos      = section_resize ? section_resize_seek_time : duration.duration * new_seek_percent;

						bool mouse_moving = app::mouse_delta[ 0 ] != 0 || app::mouse_delta[ 1 ] != 0;

						if ( mouse_moving || io.MouseClicked[ 0 ] )
						{
							//if ( mouse_moving )
							//	printf( "MOUSE MOVE\n" );

							// ui actually feels worse with this lol
							// timeline_set_seek_time_fast( new_time_pos );
							// timeline_set_seek_time( new_time_pos );
						}
					}
				}
			}

			// ------------------------------------------------------------------------------------------
			// Draw Seek position on top of everything

			if ( clip_data::get_current_group_source() == video_i )
			{
				double time_pos_seconds = duration.duration ? duration.duration / time_pos : 0;
				int    seek_pos_final   = duration.duration ? ( seek_area / time_pos_seconds ) + seek_pos_start : seek_pos_start;

				// if ( section_resize )
				// {
				// 	seek_pos_final = seek_area / ( duration / section_resize_seek_time ) + seek_pos_start;
				// }
				// else if ( seek_drag )
				if ( ( seek_drag && !section_resize ) || seek_time_override )
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

	if ( !timeline::do_scroll )
		timeline::scroll_x = ImGui::GetScrollX();

	timeline::do_scroll    = false;
	timeline::scroll_max_x = ImGui::GetScrollMaxX();

	ImGui::EndChild();

	shift_time_range_hovered_prev = shift_time_range_hovered;
	just_selected_section         = false;

	// ------------------------------------------------------------------------------------------
	// Key binding controls

	if ( group && capture_inputs && g_selected_section != UINT32_MAX && ImGui::IsKeyPressed( ImGuiKey_Delete ) )
	{
		clip_group_remove_time_range( clip_data::current_clip, *group, clip_data::get_current_group_source(), g_selected_section );
		g_selected_section = UINT32_MAX;
	}

	// ------------------------------------------------------------------------------------------
	// Set new cursor pos for normal imgui widget drawing

	// ImGui::SetCursorPos( ImVec2( cursor_pos.x, cursor_pos.y + g_timeline_size.y + ( style.ItemSpacing.y * 1.5 ) ) );
	//ImGui::SetCursorPos( ImVec2( cursor_pos.x, cursor_pos.y + g_timeline_size.y + style.ItemSpacing.y ) );

	if ( !seek_drag && was_playing && paused && group && group->sources.size() )
	{
		if ( time_pos >= durations[ clip_data::get_current_group_source() ].duration - 0.5f )
		{
			if ( group->sources.size() > clip_data::get_current_group_source() + 1 )
			{
				change_to_source_i = ++clip_data::current_group_source[ clip_data::current_group ];
				new_time_pos       = 0.f;
			}
		}
	}

	if ( change_to_source_i != UINT32_MAX )
	{
		// char time_pos_str[ 16 ];
		// gcvt( new_time_pos, 4, time_pos_str );
		// 
		// int ret = p_mpv_set_option_string( get_mpv(), "start", time_pos_str );

		replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, change_to_source_i );
		// timeline_set_seek_time( new_time_pos );

		// HACK: despite trying to wait for mpv to load the video fully, or even starting at at the desired time
		// it still places the start time at 0 sometimes
		// so, do it the next time we draw the timeline if needed lol
		// it looks a bit odd, but works
		update_time_next_draw = true;

		mpv_data_t* mpv       = get_mpv_data( change_to_source_i );

		if ( mpv && !stay_paused && !seek_drag && was_playing && paused )
		{
			const char* cmd[]   = { "set", "pause", "no", NULL };
			int         cmd_ret = p_mpv_command_async( mpv->mpv, 0, cmd );
		}

		if ( stay_paused )
			stay_paused = false;
	}

	if ( seek_drag || seek_time_override )
	{
		bool mouse_moving = app::mouse_delta[ 0 ] != 0 || app::mouse_delta[ 1 ] != 0;

		if ( mouse_moving || io.MouseClicked[ 0 ] || seek_time_override )
		{
			// ui actually feels worse with this lol
			// timeline_set_seek_time_fast( new_time_pos );
			timeline_set_seek_time( new_time_pos );
		}
	}

	if ( group )
	{
		if ( shift_time_range )
		{
			clip_group_shift_time_range( clip_data::current_clip, *group, shift_time_range_vid, shift_time_range_hovered, shift_time_range_dir );
		}

		if ( delete_current_vid )
		{
			delete_current_video( group );
			group = nullptr;
		}
	}

	was_playing = !paused;
}

