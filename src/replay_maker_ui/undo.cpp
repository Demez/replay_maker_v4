#include "main.h"

#include <vector>


constexpr size_t      UNDO_HISTORY_MAX = 64;


static undo_action_t* g_undo_actions;
static size_t         g_undo_action_pos = 0;


void undo_history_free_action_data( undo_action_t& action )
{
	switch ( action.type )
	{
		// no allocated data to free for these
		case e_action_timeline_marker_modify:
		default:
			break;

		case e_action_timeline_section_modify:
		{
			free( action.section_modify.time_ranges );
			break;
		}
		case e_action_clip_output_rename:
		{
			free( action.output_rename.name );
			break;
		}
	}
}


void undo_history_free_action( size_t index )
{
	// Remove an action, and shift everything back
	if ( index > UNDO_HISTORY_MAX )
		return;

	undo_history_free_action_data( g_undo_actions[ index ] );

	// Move everything back
	if ( index < UNDO_HISTORY_MAX )
	{
		util_array_remove_element( g_undo_actions, g_undo_action_pos, index );

		// memmove( &g_undo_actions[ index ], &g_undo_actions[ index + 1 ], (UNDO_HISTORY_MAX - index) * sizeof( );
	}

	memset( &g_undo_actions[ UNDO_HISTORY_MAX - 1 ], 0, sizeof( undo_action_t ) );
}


bool undo_history_init()
{
	g_undo_actions = ch_calloc< undo_action_t >( UNDO_HISTORY_MAX );
	return g_undo_actions != nullptr;
}


void undo_history_shutdown()
{
	for ( size_t i = 0; i < UNDO_HISTORY_MAX; i++ )
	{
		undo_history_free_action_data( g_undo_actions[ i ] );
	}

	free( g_undo_actions );
	g_undo_action_pos = 0;
}


// Returns a new undo_action_t pointer, where you can fill in your data for it there
undo_action_t* undo_history_add_action( e_action type )
{
	if ( g_undo_action_pos == UNDO_HISTORY_MAX )
	{
		undo_history_free_action( 0 );
		g_undo_action_pos--;
	}

	memset( &g_undo_actions[ g_undo_action_pos ], 0, sizeof( undo_action_t ) );
	undo_action_t* action = &g_undo_actions[ g_undo_action_pos ];
	action->type          = type;
	g_undo_action_pos++;

	return action;
}


void undo_history_apply_action( size_t index )
{
}


void undo_history_do_undo()
{
}


void undo_history_do_redo()
{
}

