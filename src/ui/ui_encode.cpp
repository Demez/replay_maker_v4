#include "main.h"
#include "encoder/encoder.h"

#include "imgui_internal.h"

bool used_in_preset( clip_encode_settings_t& override, u32 preset_i );

// makes sure the time range desired is valid for this input video
bool valid_time_range( clip_time_range_t& range, video_metadata_t& metadata );


static int preset_select = -1;
static int output_select = -1;


void encode_draw_sidebar()
{
	ImGuiIO&    io        = ImGui::GetIO();
	ImGuiStyle& style     = ImGui::GetStyle();

	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	if ( ImGui::BeginChild( "##encode_sidebar", {}, ImGuiChildFlags_ResizeX, ImGuiWindowFlags_None ) )
	{
		ImGui::TextUnformatted( "Encode Presets" );

		if ( ImGui::BeginChild( "##preset_list", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_None ) )
		{
			for ( size_t preset_i = 0; preset_i < g_clip_data->preset_count; preset_i++ )
			{
				clip_encode_preset_t& preset = g_clip_data->preset[ preset_i ];

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

		ImGui::TextUnformatted( "Video List" );

		ImGuiStyle& style = ImGui::GetStyle();

		if ( ImGui::BeginChild( "##video_list", {}, ImGuiChildFlags_Borders /*| ImGuiChildFlags_ResizeY*/ ) )
		{
			for ( size_t vid_i = 0; vid_i < g_clip_data->output_count; vid_i++ )
			{
				clip_output_video_t& output     = g_clip_data->output[ vid_i ];
				enc_output_video_t&  enc_output = g_output_videos[ vid_i ];

				u32 preset_idx = g_encoder_data.encode_preset;

				if ( preset_select > -1 )
					preset_idx = preset_select;

				clip_encode_preset_t& preset            = g_clip_data->preset[ preset_idx ];

				bool                  is_used_in_preset = false;
				float                 duration          = 0.f;
				bool                  duration_invalid  = false;

				for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
				{
					clip_input_video_t& input = output.input[ in_i ];

					if ( used_in_preset( input.encode_settings, preset_idx ) )
					{
						is_used_in_preset = true;
					}
					else
					{
						continue;
					}

					for ( u32 time_i = 0; time_i < input.time_range_count; time_i++ )
					{
						if ( !valid_time_range( input.time_range[ time_i ], input.metadata ) )
							duration_invalid = true;

						duration += input.time_range[ time_i ].end - input.time_range[ time_i ].start;
					}
				}

				if ( !is_used_in_preset )
					continue;

				char name_buf[ 512 ]{};
				snprintf( name_buf, 512, " %zu - %s", vid_i, output.name );

				bool draw_bg_color = output.state != e_output_state_wait;

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

					switch ( output.state )
					{
						default:
						case e_output_state_running:
						case e_output_state_count:
							break;

						case e_output_state_user_skipped:
							color = COLOR_PURPLE;
							break;

						case e_output_state_failed:
						case e_output_state_invalid:
							color = COLOR_BTN_RED;
							break;

						case e_output_state_finished:
						case e_output_state_already_finished:
							color = COLOR_GREEN;
							break;
					}

					draw_list->AddRectFilled( cursor_scr_pos, highlight_size, color );
				}

				if ( ImGui::Selectable( name_buf, output_select == vid_i ) )
				{
					if ( output_select == vid_i )
						output_select = -1;
					else
						output_select = vid_i;
				}

				bool focused = g_encoder_data.output_index == vid_i;

				if ( output_select > -1 )
				{
					focused = output_select == vid_i;
				}

				if ( focused )
				{
				}
			}
		}

		ImGui::EndChild();

		//if ( ImGui::BeginChild( "##style_edit", {}, ImGuiChildFlags_Borders, ImGuiWindowFlags_None ) )
		//	ImGui::ShowStyleEditor();
		//
		//ImGui::EndChild();
	}

	ImGui::EndChild();
}


void encode_draw_ffmpeg()
{
	ImGui::TextUnformatted( "FFMpeg Output" );

	// ImGui::PushStyleColor( ImGuiCol_ChildBg,)

	float       ffmpeg_output_size = ImGui::GetContentRegionAvail().y;
	ImGuiStyle& style              = ImGui::GetStyle();

	float       basic_text_height  = ImGui::GetFontSize() + style.ItemSpacing.y * 2;
	float       separator_height   = 1.f;

	ffmpeg_output_size -= basic_text_height * 5;
	ffmpeg_output_size -= ImGui::GetFrameHeight() * 1;
	ffmpeg_output_size -= separator_height * 1;  // ???
	ffmpeg_output_size -= style.WindowPadding.y * 2;

	if ( !ImGui::BeginChild( "##ffmpeg_output", { -1, ffmpeg_output_size }, ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar ) )
	{
		ImGui::EndChild();
		return;
	}

	static float scroll_max = ImGui::GetScrollMaxY();

	ImGui::PushTextWrapPos();
	ImGui::PushFont( font::console );

	u32 output_idx = g_encoder_data.output_index;

	if ( output_select > -1 )
		output_idx = output_select;

	clip_output_video_t& output     = g_clip_data->output[ output_idx ];
	enc_output_video_t&  enc_output = g_output_videos[ output_idx ];

	enc_output.ffmpeg_output_lock.lock();
	ImGui::TextUnformatted( enc_output.ffmpeg_output );
	enc_output.ffmpeg_output_lock.unlock();

	ImGui::PopFont();
	ImGui::PopTextWrapPos();

	// if we were scrolled all the way down before, make sure we stay scrolled down all the way
	if ( scroll_max == ImGui::GetScrollY() )
		ImGui::SetScrollY( ImGui::GetScrollMaxY() );

	scroll_max = ImGui::GetScrollMaxY();

	ImGui::EndChild();
}


void encode_draw_output_info()
{
	if ( !ImGui::BeginChild( "##output_info", {}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse ) )
	{
		ImGui::EndChild();
		return;
	}

	u32 output_idx = g_encoder_data.output_index;
	u32 preset_idx = g_encoder_data.encode_preset;

	if ( output_select > -1 )
		output_idx = output_select;

	if ( preset_select > -1 )
		preset_idx = preset_select;

	clip_output_video_t&  output     = g_clip_data->output[ output_idx ];
	enc_output_video_t&   enc_output = g_output_videos[ output_idx ];
	clip_encode_preset_t& preset     = g_clip_data->preset[ preset_idx ];

	std::string           filename   = get_video_output_name( output, preset );

	ImGui::Text( "Output Path: %s", g_encoder_data.output_dir.c_str() );

	ImGui::Separator();

	ImGui::Text( "File: %s", filename.c_str() );

	float duration         = 0.f;
	bool  duration_invalid = false;

	ImGui::Separator();

	for ( u32 in_i = 0; in_i < output.input_count; in_i++ )
	{
		clip_input_video_t& input = output.input[ in_i ];

		if ( !used_in_preset( input.encode_settings, preset_idx ) )
			continue;

		ImGui::Text( "Source %u: %s", in_i, input.path );

		for ( u32 time_i = 0; time_i < input.time_range_count; time_i++ )
		{
			if ( !valid_time_range( input.time_range[ time_i ], input.metadata ) )
			{
				duration_invalid = true;
				continue;
			}

			float range_duration = input.time_range[ time_i ].end - input.time_range[ time_i ].start;
			ImGui::Text( "    %.4f - %.4f (%.4f)", input.time_range[ time_i ].start, input.time_range[ time_i ].end, range_duration );

			duration += input.time_range[ time_i ].end - input.time_range[ time_i ].start;
		}
	}

	ImGui::Separator();

	ImGui::Text( "Duration: %.4f%s", duration, duration_invalid ? " [INVALID]" : "" );

	// Output Path
	// Source Videos
	// Duration
	// Current Video working on

	ImGui::Separator();

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

	ImGui::EndChild();
}


void encode_draw()
{
	if ( !g_clip_data )
		return;

	ImGuiStyle& style = ImGui::GetStyle();

	int window_width, window_height;
	SDL_GetWindowSizeInPixels( g_main_window_sdl, &window_width, &window_height );

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

			ImGui::Text( "%d / %d Videos Scanned", g_encoder_data.scan_index, g_clip_data->output_count );
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

