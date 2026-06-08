#include "main.h"
#include "encoder/encoder.h"
#include "logging.h"

#include "imgui_internal.h"

// makes sure the time range desired is valid for this source video
bool valid_time_range( clip_time_range_t& range, video_metadata_t& metadata );


static int   preset_select         = -1;
static int   output_select         = -1;

static float clip_info_area_height = 0.f;

#if 0
clip_group_t* video_get_group_from_preset( enc_clip_t& enc_clip, u32 preset_idx )
{
	bool is_used_in_preset = false;

	// check if this video is used on this preset
	for ( u32 i = 0; i < enc_clip.presets.size(); i++ )
	{
		if ( enc_clip.presets[ i ].preset != preset_idx )
			continue;

		is_used_in_preset = true;
		break;
	}

	if ( !is_used_in_preset )
		return false;

	// get the current group we want
	u32 group_i = 0;
	for ( ; group_i < enc_clip.clip->groups.size(); group_i++ )
	{
		clip_group_t& group = enc_clip.clip->groups[ group_i ];

		for ( u32 i = 0; i < enc_clip.presets.size(); i++ )
		{
			if ( group.presets[ i ] == preset_idx )
				return &group;
		}
	}

	return nullptr;
}
#endif


#if 0
bool video_get_use_info( clip_t& clip, enc_clip_t& enc_clip, u32 preset_idx )
{
	bool is_used_in_preset = false;

	// check if this video is used on this preset
	for ( u32 i = 0; i < enc_clip.presets.size(); i++ )
	{
		if ( enc_clip.presets[ i ] != preset_idx )
			continue;

		is_used_in_preset = true;
		break;
	}

	if ( !is_used_in_preset )
		return false;

	// get the current group we want
	u32 group_i = 0;
	for ( ; group_i < clip.groups.size(); group_i++ )
	{
		clip_group_t& _group = clip.groups[ group_i ];

		for ( u32 i = 0; i < enc_clip.presets.size(); i++ )
		{
			if ( _group.presets[ i ] == preset_idx )
				goto group_found;
		}
	}

	if ( group_i == clip.groups.size() )
	{
		// how would we even get here
		clip.state = e_enc_state_failed;
		return false;
	}

group_found:
	clip_group_t& group = clip.groups[ group_i ];
	bool          duration_invalid = false;
	//float         duration         = 0.f;

	for ( u32 src_i = 0; src_i < group.sources.size(); src_i++ )
	{
		clip_source_usage_t& source_use = group.sources[ src_i ];
		clip_source_t&       source     = clip.source[ source_use.source_index ];

		for ( u32 time_i = 0; time_i < source_use.time_range.size(); time_i++ )
		{
			if ( !valid_time_range( source_use.time_range[ time_i ], source.metadata ) )
				duration_invalid = true;

			//duration += source_use.time_range[ time_i ].end - source_use.time_range[ time_i ].start;
		}
	}

	return is_used_in_preset;
}
#endif


void encode_draw_sidebar()
{
	ImGuiIO&    io                 = ImGui::GetIO();
	ImGuiStyle& style              = ImGui::GetStyle();

	ImDrawList* draw_list          = ImGui::GetWindowDrawList();

	static u32  last_clip_i        = 0;
	bool        just_switched_clip = g_encoder_data.clip_index_prev != last_clip_i;

	last_clip_i                    = g_encoder_data.clip_index_prev;

	if ( ImGui::BeginChild( "##encode_sidebar", {}, ImGuiChildFlags_ResizeX, ImGuiWindowFlags_None ) )
	{
		ImGui::TextUnformatted( "Clip Filtering" );

		if ( ImGui::BeginChild( "##clip_filtering", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
		{
			clip_filtering_draw();

			//ImGui::Separator();
			//
			//if ( ImGui::Button( sort_newest_top ? "Sort: Newest First" : "Sort: Oldest First" ) )
			//	sort_newest_top = !sort_newest_top;
			//
			//ImGui::SameLine();
		}

		ImGui::EndChild();

		//ImGui::PushFont( font::normal, font::size + 4 );
		//
		//if ( g_encode_finished )
		//{
		//	ImGui::TextUnformatted( "Export Finished" );
		//}
		//else
		//{
		//	ImGui::TextUnformatted( "Export Running" );
		//}
		//
		//ImGui::PopFont();
		//
		//ImGui::Separator();

		#if 0
		ImGui::TextUnformatted( "Encode Presets" );

		if ( ImGui::BeginChild( "##preset_list", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_None ) )
		{
			for ( size_t preset_i = 0; preset_i < clip_data::preset_count; preset_i++ )
			{
				clip_encode_preset_t& preset = clip_data::preset[ preset_i ];

				if ( g_encoder_data.encode_preset == preset_i )
				{
					ImVec2 region_avail   = ImGui::GetContentRegionAvail();
					ImVec2 cursor_scr_pos = ImGui::GetCursorScreenPos();

					ImVec2 highlight_size{
						cursor_scr_pos.x + region_avail.x + style.ItemSpacing.x * 0.5f,
						cursor_scr_pos.y + ImGui::GetFontSize() + style.ItemSpacing.y * 0.5f
					};

					// something with centering? idk tbh
					cursor_scr_pos.x -= style.ItemSpacing.x * 0.5f;
					cursor_scr_pos.y -= style.ItemSpacing.y * 0.5f;

					ImColor color = style.Colors[ ImGuiCol_Button ];

					draw_list->AddRectFilled( cursor_scr_pos, highlight_size, color );
				}

				if ( ImGui::Selectable( preset.name, preset_select == preset_i ) )
				{
					if ( preset_select == preset_i )
						preset_select = -1;
					else
						preset_select = preset_i;

					if ( output_select > -1 )
						output_select = -1;
				}
			}
		}

		ImGui::EndChild();
#endif

		u32 result_count = 0;
		for ( u32 out_i = 0; out_i < clip_data::clip_count; out_i++ )
		{
			if ( clip_filtering_visible( clip_data::clip[ out_i ] ) )
				result_count++;
		}

		ImGui::Text( "Video List - %u/%u Complete", g_encoder_data.clip_index, clip_data::clip_count );

		if ( result_count < clip_data::clip_count )
		{
			ImGui::SameLine();
			ImGui::Text( "%u Shown", result_count );
		}

		ImGuiStyle& style = ImGui::GetStyle();

		if ( ImGui::BeginChild( "##video_list", {}, ImGuiChildFlags_Borders /*| ImGuiChildFlags_ResizeY*/ ) )
		{
			for ( size_t vid_i = 0; vid_i < clip_data::clip_count; vid_i++ )
			{
				clip_t&     clip       = clip_data::clip[ vid_i ];
				enc_clip_t& enc_clip   = g_encoder_clips[ vid_i ];

				if ( !clip_filtering_visible( clip ) )
					continue;

				if ( !clip.enabled )
					continue;

				u32 preset_idx = g_encoder_data.encode_preset;

				if ( preset_select > -1 )
					preset_idx = preset_select;

				clip_encode_preset_t& preset = clip_data::preset[ preset_idx ];
				clip_prefix_t&        prefix = clip_data::prefix[ clip.prefix ];

				//enc_clip_preset_t* enc_preset = encode_get_enc_preset( enc_clip, preset_idx );
				//
				//if ( !enc_preset )
				//	continue;
				//
				//clip_group_t& group = clip.groups[ enc_preset->group ];

				#if 0
				clip_group_t& group = clip.groups[ g_encoder_data.clip_group_i ];


				float                 duration         = 0.f;
				bool                  duration_invalid = false;

				for ( u32 src_i = 0; src_i < group.sources.size(); src_i++ )
				{
					clip_source_usage_t& source_use = group.sources[ src_i ];
					clip_source_t&       source     = clip.source[ source_use.source_index ];

					for ( u32 time_i = 0; time_i < source_use.time_range.size(); time_i++ )
					{
						if ( !valid_time_range( source_use.time_range[ time_i ], source.metadata ) )
							duration_invalid = true;
			
						duration += source_use.time_range[ time_i ].end - source_use.time_range[ time_i ].start;
					}
				}
				#endif

				char name_buf[ 512 ]{};
				snprintf( name_buf, 512, "%s - %s", prefix.name, clip.name );

				// snprintf( name_buf, 512, " %zu - %s", vid_i, clip.name );

				#if 0
				bool draw_bg_color = clip.state != e_enc_state_valid;

				if ( draw_bg_color )
				{
					ImVec2 region_avail   = ImGui::GetContentRegionAvail();
					ImVec2 cursor_scr_pos = ImGui::GetCursorScreenPos();

					ImVec2 highlight_size{
						cursor_scr_pos.x + region_avail.x + style.ItemSpacing.x * 0.5f,
						cursor_scr_pos.y + ImGui::GetFontSize() + style.ItemSpacing.y * 0.5f
					};

					// something with centering? idk tbh
					cursor_scr_pos.x -= style.ItemSpacing.x * 0.5f;
					cursor_scr_pos.y -= style.ItemSpacing.y * 0.5f;

					ImColor color = style.Colors[ ImGuiCol_Button ];

					switch ( clip.state )
					{
						default:
						case e_enc_state_running:
						case e_enc_state_count:
							break;

						case e_enc_state_user_skipped:
							color = COLOR_PURPLE;
							break;

						case e_enc_state_failed:
						case e_enc_state_invalid:
							color = COLOR_BTN_RED;
							break;

						case e_enc_state_finished:
						case e_enc_state_already_finished:
							color = COLOR_GREEN;
							break;
					}

					//draw_list->AddRectFilled( cursor_scr_pos, highlight_size, color );
				}
				#endif

				//if ( ImGui::Selectable( name_buf, output_select == vid_i ) )
				//{
				//	if ( output_select == vid_i )
				//		output_select = -1;
				//	else
				//		output_select = vid_i;
				//}

				bool focused = g_encoder_data.clip_index == vid_i;

				if ( output_select > -1 )
				{
					focused = output_select == vid_i;
				}

				//ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { 2, 2 } );

				//if ( focused )
				{
					ImGui::PushID( vid_i + 1 );

					// ImGuiChildFlags child_flags = ImGuiChildFlags_Border | ImGuiChildFlags_FrameStyle;
					ImGuiChildFlags child_flags   = ImGuiChildFlags_Border;

					//bool            draw_bg_color = enc_clip.state != e_enc_state_valid;
					bool            current       = g_encoder_data.clip_index == vid_i;

					ImVec4          color_bg      = style.Colors[ ImGuiCol_FrameBg ];

					ImVec4          header_bg     = style.Colors[ ImGuiCol_Header ];
					ImVec4          header_active = style.Colors[ ImGuiCol_HeaderActive ];
					ImVec4          header_hover  = style.Colors[ ImGuiCol_HeaderHovered ];

					switch ( enc_clip.state )
					{
						default:
						case e_enc_state_wait:
							//color_bg.w = 0.f;
							color_bg = style.Colors[ ImGuiCol_ChildBg ];
							color_bg.w = 0.5;
							break;

						case e_enc_state_running:
							break;

						case e_enc_state_skipped:
							color_bg      = COLOR_PURPLE_FRAME;
							header_active = COLOR_PURPLE_ACTIVE;
							header_hover  = COLOR_PURPLE_HOVER;
							break;

						case e_enc_state_failed:
							color_bg      = COLOR_RED_FRAME;
							header_active = COLOR_BTN_RED_ACTIVE;
							header_hover  = COLOR_BTN_RED_HOVER;
							break;

						case e_enc_state_finished:
							color_bg      = COLOR_GREEN_FRAME;
							header_active = COLOR_GREEN_ACTIVE;
							header_hover  = COLOR_GREEN_HOVER;
							break;
					}

					if ( current )
						color_bg = style.Colors[ ImGuiCol_FrameBg ];

					ImGui::PushStyleColor( ImGuiCol_ChildBg, color_bg );

					//draw_list->AddRectFilled( cursor_scr_pos, highlight_size, color );
					

					if ( ImGui::BeginChild( "##clip_encode_progress", {}, child_flags | ImGuiChildFlags_AutoResizeY ) )
					{
						// ImGui::TextUnformatted( name_buf );
						// ImGui::Separator();

						header_bg.w          = 0.f;

						ImGui::PushStyleColor( ImGuiCol_Header, header_bg );
						ImGui::PushStyleColor( ImGuiCol_HeaderActive, header_active );
						ImGui::PushStyleColor( ImGuiCol_HeaderHovered, header_hover );

						if ( just_switched_clip )
						{
							if ( g_encoder_data.clip_index == vid_i )
								ImGui::SetNextItemOpen( true );
							else if ( last_clip_i == vid_i )
								ImGui::SetNextItemOpen( false );
						}

						if ( ImGui::CollapsingHeader( name_buf, ImGuiTreeNodeFlags_None ) )
						{
							ImGui::Separator();
							ImGui::PushStyleVar( ImGuiStyleVar_ButtonTextAlign, ImVec2( 0, 0.5 ) );

							u32 group_export_i = 0;
							for ( u32 group_i = 0; group_i < clip.groups.size(); group_i++ )
							{
								for ( u32 preset_i : clip.groups[ group_i ].presets )
								{
									bool        current        = focused && g_encoder_data.encode_preset == preset_i;

									e_enc_state preset_state   = enc_clip.group_state[ group_export_i++ ];

									ImVec2 region_avail   = ImGui::GetContentRegionAvail();
									ImVec2 cursor_scr_pos = ImGui::GetCursorScreenPos();

									ImVec2 highlight_size{
										cursor_scr_pos.x + region_avail.x + style.ItemSpacing.x * 0.5f,
										cursor_scr_pos.y + ImGui::GetFontSize() + style.ItemSpacing.y * 0.5f
									};

									ImVec2 button_size{ region_avail.x, ImGui::GetFrameHeight() };

									// something with centering? idk tbh
									cursor_scr_pos.x -= style.ItemSpacing.x * 0.5f;
									cursor_scr_pos.y -= style.ItemSpacing.y * 0.5f;

									ImVec4 color_bg     = style.Colors[ ImGuiCol_Button ];
									ImVec4 color_active = style.Colors[ ImGuiCol_ButtonActive ];
									ImVec4 color_hover  = style.Colors[ ImGuiCol_ButtonHovered ];

									//if ( current )
									//{
									//}

									bool    draw_bg = true;

									switch ( preset_state )
									{
										default:
										case e_enc_state_wait:
											color_bg.w     = 0.f;
											//color_active.w = 0.f;
											//color_hover.w  = 0.f;
											draw_bg        = true;
											break;

										case e_enc_state_running:
											draw_bg = false;
											break;
								
										case e_enc_state_skipped:
											color_bg     = COLOR_PURPLE;
											color_active = COLOR_PURPLE_ACTIVE;
											color_hover  = COLOR_PURPLE_HOVER;
											draw_bg      = true;
											break;
								
										case e_enc_state_failed:
											color_bg     = COLOR_BTN_RED;
											color_active = COLOR_BTN_RED_ACTIVE;
											color_hover  = COLOR_BTN_RED_HOVER;
											draw_bg      = true;
											break;
								
										case e_enc_state_finished:
											color_bg     = COLOR_GREEN;
											color_active = COLOR_GREEN_ACTIVE;
											color_hover  = COLOR_GREEN_HOVER;
											draw_bg      = true;
											break;
									}

									if ( current )
									{

									}

									//if ( draw_bg )
									//	draw_list->AddRectFilled( cursor_scr_pos, highlight_size, color );
								
									if ( draw_bg )
									{
										ImGui::PushStyleColor( ImGuiCol_Button, color_bg );
										ImGui::PushStyleColor( ImGuiCol_ButtonActive, color_active );
										ImGui::PushStyleColor( ImGuiCol_ButtonHovered, color_hover );
									}

									//clip_encode_preset_t* preset = clip_get_encode_preset( preset_i );
									//ImGui::Text( "%sEncode Preset: %s", current ? "[CURRENT] " : "", preset->name );

									//ImGui::SetNextItemWidth( -FLT_MIN );

									std::string filename = get_video_output_name( clip, clip_data::preset[ preset_i ] );
									if ( ImGui::Button( filename.c_str(), button_size ) )
									{
										// TODO: focus output info on this entry
									}

									if ( draw_bg )
										ImGui::PopStyleColor( 3 );

								}
							}

							ImGui::PopStyleVar();
						}

						// Header Colors
						ImGui::PopStyleColor( 3 );
					}

					ImGui::EndChild();

					//if ( draw_bg_color )
					{
						ImGui::PopStyleColor();
					}

					ImGui::PopID();
				}
			
				//ImGui::PopStyleVar();

			}
		}

		ImGui::EndChild();

		//if ( ImGui::BeginChild( "##style_edit", {}, ImGuiChildFlags_Borders, ImGuiWindowFlags_None ) )
		//	ImGui::ShowStyleEditor();
		//
		//ImGui::EndChild();
	}

	ImGui::EndChild();

	//last_clip_i = g_encoder_data.clip_index;
}


void encode_draw_ffmpeg()
{
	ImGui::TextUnformatted( "FFMpeg Output" );

	// ImGui::PushStyleColor( ImGuiCol_ChildBg,)

	float       ffmpeg_output_size = ImGui::GetContentRegionAvail().y;
	ImGuiStyle& style              = ImGui::GetStyle();

	float       basic_text_height  = ImGui::GetFontSize() + style.ItemSpacing.y * 2;
	float       separator_height   = 1.f;

	// ffmpeg_output_size -= basic_text_height * 5;
	// ffmpeg_output_size -= ImGui::GetFrameHeight() * 1;
	// ffmpeg_output_size -= separator_height * 1;  // ???
	// ffmpeg_output_size -= style.WindowPadding.y * 2;

	ffmpeg_output_size -= clip_info_area_height;
	ffmpeg_output_size -= style.ItemSpacing.y;

	if ( !ImGui::BeginChild( "##ffmpeg_output", { -1, ffmpeg_output_size }, ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar ) )
	{
		ImGui::EndChild();
		return;
	}

	static float scroll_max = ImGui::GetScrollMaxY();
	u32          output_idx = g_encoder_data.clip_index;

	if ( output_select > -1 )
		output_idx = output_select;

	if ( output_idx < clip_data::clip_count )
	{
		clip_t&     clip     = clip_data::clip[ output_idx ];
		enc_clip_t& enc_clip = g_encoder_clips[ output_idx ];

		ImGui::PushTextWrapPos();
		ImGui::PushFont( font::console );

		enc_clip.ffmpeg_output_lock.lock();
		ImGui::TextUnformatted( enc_clip.ffmpeg_output );
		enc_clip.ffmpeg_output_lock.unlock();

		ImGui::PopFont();
		ImGui::PopTextWrapPos();

		// if we were scrolled all the way down before, make sure we stay scrolled down all the way
		if ( scroll_max == ImGui::GetScrollY() )
			ImGui::SetScrollY( ImGui::GetScrollMaxY() );

		scroll_max = ImGui::GetScrollMaxY();
	}

	ImGui::EndChild();
}


void encode_draw_output_info()
{
	u32         output_idx = g_encoder_data.clip_index;
	u32         preset_idx = g_encoder_data.encode_preset;

	ImGuiStyle& style      = ImGui::GetStyle();

	if ( output_select > -1 )
		output_idx = output_select;

	if ( preset_select > -1 )
		preset_idx = preset_select;

	clip_info_area_height = 0;

	if ( !ImGui::BeginChild( "##clip_status_info", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse ) )
	{
		goto clip_info_draw;
	}

	//clip_info_area_height += style.WindowPadding.y;

	if ( g_encode_finished )
	{
		ImGui::PushFont( font::normal, font::size + 4 );
		ImGui::TextUnformatted( "Export Finished" );
		clip_info_area_height += ImGui::GetTextLineHeight();
		ImGui::PopFont();
		
		ImGui::Separator();
	}

	if ( output_idx < clip_data::clip_count && preset_idx != UINT32_MAX )
	{
		// Draw current video info
		clip_t&               clip     = clip_data::clip[ output_idx ];
		enc_clip_t&           enc_clip = g_encoder_clips[ output_idx ];
		clip_encode_preset_t& preset   = clip_data::preset[ preset_idx ];

		std::string           filename = get_video_output_name( clip, preset );

		ImGui::Text( "Output Path: %s", g_encoder_data.output_dir.c_str() );

		ImGui::Separator();

		ImGui::Text( "File: %s", filename.c_str() );

		clip_info_area_height += ImGui::GetTextLineHeightWithSpacing() * 2;

		// enc_clip_preset_t* enc_preset = encode_get_enc_preset( enc_clip, preset_idx );
		// 
		// if ( !enc_preset )
		// {
		// 	// WE SHOULD NOT BE HERE
		// 	goto clip_info_draw;
		// }

		clip_group_t& group            = clip.groups[ g_encoder_data.clip_group_i ];

		float         duration         = 0.f;
		bool          duration_invalid = false;

		ImGui::Separator();

		for ( u32 src_i = 0; src_i < group.sources.size(); src_i++ )
		{
			clip_source_usage_t& source_use = group.sources[ src_i ];
			clip_source_t&       source     = clip.source[ source_use.source_index ];

			for ( u32 time_i = 0; time_i < source_use.time_range.size(); time_i++ )
			{
				if ( !valid_time_range( source_use.time_range[ time_i ], source.metadata ) )
				{
					duration_invalid = true;
					continue;
				}

				float range_duration = source_use.time_range[ time_i ].end - source_use.time_range[ time_i ].start;
				ImGui::Text( "    %.4f - %.4f (%.4f)", source_use.time_range[ time_i ].start, source_use.time_range[ time_i ].end, range_duration );
				clip_info_area_height += ImGui::GetTextLineHeightWithSpacing();

				duration += source_use.time_range[ time_i ].end - source_use.time_range[ time_i ].start;
			}
		}

		ImGui::Separator();

		ImGui::Text( "Duration: %.4f%s", duration, duration_invalid ? " [INVALID]" : "" );
		clip_info_area_height += ImGui::GetTextLineHeightWithSpacing();

		// Output Path
		// Source Videos
		// Duration
		// Current Video working on

		ImGui::Separator();
	}

	if ( g_encode_finished )
	{
		if ( ImGui::Button( "Return to Editor" ) )
		{
			encode_thread_stop();
		}
		
		clip_info_area_height += ImGui::GetFrameHeight();
	}
	else
	{
		if ( ImGui::Button( g_encode_pause ? "Resume" : "Pause" ) )
		{
			g_encode_pause = !g_encode_pause;
			SDL_Delay( 50 );
		}

		ImGui::SameLine();
		ImGui::SeparatorEx( ImGuiSeparatorFlags_Vertical );
		ImGui::SameLine();

		if ( ImGui::Button( "Skip" ) )
		{
		}

		ImGui::SameLine();

		if ( ImGui::Button( "Cancel" ) )
		{
			encode_thread_stop();
		}
		
		clip_info_area_height += ImGui::GetFrameHeightWithSpacing() * 3;
	}

	ImGui::SameLine();
	ImGui::SeparatorEx( ImGuiSeparatorFlags_Vertical );
	ImGui::SameLine();

	if ( ImGui::Button( "Open Output Folder" ) )
	{
		sys_open_folder( g_output_dir );
	}
	ImGui::SameLine();

	if ( ImGui::Button( "Open Log Folder" ) )
	{
		sys_open_folder( g_log_dir.c_str() );
	}

	clip_info_area_height += ImGui::GetFrameHeight() * 2;

clip_info_draw:
	clip_info_area_height = ImGui::GetWindowHeight();
	ImGui::EndChild();
}


void encode_draw()
{
	ImGuiStyle& style = ImGui::GetStyle();

	int window_width, window_height;
	SDL_GetWindowSizeInPixels( app::window, &window_width, &window_height );

	// ==========================================================================================================
	// wait for it to finish lol

	bool _started = g_encode_started.load();

	if ( !_started )
	{
		ImGui::SetNextWindowPos( { window_width * 0.5f, window_height * 0.5f }, ImGuiCond_Always, { 0.5, 0.5 } );
		// ImGui::SetNextWindowSize( { float( window_width ), float( window_height ) } );

		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { style.WindowPadding.x * 2, style.WindowPadding.y * 2 } );

		if ( ImGui::Begin( "##encode_waiting", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize ) )
		{
			ImGui::PushFont( font::normal, font::size * 2 );

			ImGui::TextUnformatted( "Collecting Video Info...\n" );

			ImGui::PopFont();

			ImGui::Separator();

			ImGui::Text( "%d / %d Videos Scanned", g_encoder_data.scan_index, clip_data::clip_count );
		}

		ImGui::PopStyleVar();

		ImGui::End();
		return;
	}

	// ==========================================================================================================

	ImGui::SetNextWindowPos( {0, 0} );
	ImGui::SetNextWindowSize( { float( window_width ), float( window_height ) } );

	ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.f );

	if ( ImGui::Begin( "##encode_status", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration ) )
	{
		encode_draw_sidebar();

		ImGui::SameLine();

		if ( ImGui::BeginChild( "##output", {}, ImGuiChildFlags_None, ImGuiWindowFlags_None ) )
		{
			encode_draw_ffmpeg();
			encode_draw_output_info();
		}

		ImGui::EndChild();
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

