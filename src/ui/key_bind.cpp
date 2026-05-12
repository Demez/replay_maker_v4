#include "main.h"
#include "imgui.h"

#include <unordered_map>


extern int  g_grabbed_divider_idx;
extern bool g_hovered_divider;


enum e_key_state : s8
{
	e_key_state_just_released = -1,
	e_key_state_released      = 0,
	e_key_state_pressed       = 1,
	e_key_state_just_pressed  = 2,
};


struct button_list_t
{
	e_key_state state;
	u8          count;
	int         mod_mask;
	ImGuiKey*   button;

	inline bool operator==( const button_list_t& srOther ) const
	{
		// Guard self assignment
		if ( this == &srOther )
			return true;

		if ( mod_mask != srOther.mod_mask )
			return false;

		if ( count != srOther.count )
			return false;

		return std::memcmp( &button, &srOther.button, sizeof( ImGuiKey ) * count ) == 0;
	}
};


struct key_cmd_t
{
	e_cmd type;
	u8    cmd_index;  // points to alternate data in an array here if needed, like the mpv command index
};


struct mpv_cmd_t
{
	e_mpv_cmd type;

	union
	{
		float seek;

		struct
		{
			// how many commands are in the command list
			u8 cmd_count;

			// each entry is an array of strings
			char** cmd;

		} cmd;
	};
};



// Hashing Support
namespace std
{
template<>
struct hash< button_list_t >
{
	size_t operator()( button_list_t const& list ) const
	{
		size_t value = 0;

		value ^= ( hash< int >()( list.mod_mask ) );
		value ^= ( hash< u8 >()( list.count ) );
		value ^= ( hash< ImGuiKey* >()( list.button ) );

		return value;
	}
};
}


static std::unordered_map< button_list_t, key_cmd_t > g_key_binds;

// mpv command list
mpv_cmd_t*                                            g_mpv_cmd;


#define MPV_CMD( ... )                                              \
	{                                                               \
		const char* cmd[]   = { __VA_ARGS__, NULL };                \
		int         cmd_ret = p_mpv_command_async( g_mpv, 0, cmd ); \
		if ( cmd_ret != 0 )                                         \
			printf( "MPV CMD Error: %d", cmd_ret );                 \
	}


void input_init()
{
	// input_bind_key( ImGuiKey_Tab, e_cmd_toggle_sidebar );
	// input_bind_keys( { ImGuiKey_LeftCtrl, ImGuiKey_S }, e_cmd_save_file );
}


void handle_keybinds()
{
	// make sure imgui isn't focused in any text box first
	if ( ImGui::GetIO().WantTextInput )
		return;

	if ( g_encode_running )
		return;

	if ( ImGui::IsKeyPressed( ImGuiKey_Tab, false ) )
	{
		enable_sidebar( !g_show_sidebar );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_F, false ) )
	{
		sys_mpv_full_window_toggle();
	}
	else if ( ImGui::IsKeyDown( ImGuiKey_LeftCtrl ) && ImGui::IsKeyPressed( ImGuiKey_S, false ) )
	{
		save_videos();
	}

	// ================================================================
	// MPV Keybinds

	// Pause
	else if ( ImGui::IsKeyPressed( ImGuiKey_Space, false ) )
	{
		double duration = 0;
		double time_pos = 0;
		p_mpv_get_property( g_mpv, "duration", MPV_FORMAT_DOUBLE, &duration );
		p_mpv_get_property( g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos );

		if ( duration - 0.15 < time_pos )
		{
			// try to seek to next vid
			timeline_advance();
		}

		mpv_cmd_toggle_playback();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_MouseLeft, false ) )
	{
		if ( !g_preset_combo_open && !g_hovered_divider && mouse_in_rect( { 0.f, 0.f }, { (float)g_mpv_size[ 0 ], (float)g_mpv_size[ 1 ] } ) )
			mpv_cmd_toggle_playback();
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_LeftArrow, true ) )
	{
		timeline_seek( -5.0 );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_RightArrow, true ) )
	{
		timeline_seek( 5.0 );
	}

	// Video Cropping/Panning Adjustments

	// Reset All
	else if ( ImGui::IsKeyPressed( ImGuiKey_Keypad0, false ) )
	{
		MPV_CMD( "set", "video-zoom", "0" );
		MPV_CMD( "set", "video-pan-x", "0" );
		MPV_CMD( "set", "video-pan-y", "0" );
		MPV_CMD( "set", "vf", "crop" );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_Keypad8, false ) )
	{
		MPV_CMD( "set", "vf", "crop=1920:1080:0:360" );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_Keypad9, false ) )
	{
		MPV_CMD( "set", "vf", "crop=2560:1440:1920:0" );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_Keypad2, false ) )
	{
		MPV_CMD( "set", "vf", "crop=1920:1080:0:0" );
	}
	else if ( ImGui::IsKeyPressed( ImGuiKey_Keypad3, false ) )
	{
		MPV_CMD( "set", "vf", "crop=1920:1080:1920:0" );
	}
}

