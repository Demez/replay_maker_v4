#include "clip.h"
#include "logging.h"
#include "main.h"


std::unordered_map< std::string, video_metadata_t > g_video_metadata_map;


namespace clip_data
{
	u32                   version              = CLIP_VIDEO_FORMAT_VER;

	clip_t*               clip                 = nullptr;
	u32                   clip_count           = 0;

	clip_encode_preset_t* preset               = nullptr;
	u32                   preset_count         = 0;

	clip_prefix_t*        prefix               = nullptr;
	u32                   prefix_count         = 0;

	// Replay Editor info, store in a different namespace maybe?
	clip_t*               current_clip         = nullptr;
	u32                   current_clip_index   = UINT32_MAX;

	u32                   current_group        = 0;
	ChVector< u32 >       current_group_source{};

	u32                   current_source = 0;
	
	u32                   get_current_group_source()
	{
		if ( current_group_source.empty() )
			return UINT32_MAX;

		if ( current_group > current_group_source.size() )
			return UINT32_MAX;

		return current_group_source[ current_group ];
	}
};

// --------------------------------------------------------------------------------------------------------


void clip_data_reset()
{
}


// ============================================================================================================================


u32 clip_add_prefix( const char* name, const char* prefix )
{
	clip_prefix_t* new_data = ch_realloc< clip_prefix_t >( clip_data::prefix, clip_data::prefix_count + 1 );

	if ( !new_data )
		return UINT32_MAX;

	clip_data::prefix = new_data;
	memset( &clip_data::prefix[ clip_data::prefix_count ], 0, sizeof( clip_prefix_t ) );

	size_t name_len = strlen( name );
	memcpy( clip_data::prefix[ clip_data::prefix_count ].name, name, std::min( name_len, MAX_LEN_PRESET_NAME ) );

	clip_data::prefix[ clip_data::prefix_count ].prefix = strdup( prefix );

	return clip_data::prefix_count++;
}


clip_encode_preset_t* clip_add_encode_preset( const char* name, const char* ext )
{
	if ( array_append( clip_data::preset, clip_data::preset_count ) )
		return nullptr;

	size_t name_len = strlen( name );
	memcpy( clip_data::preset[ clip_data::preset_count ].name, name, std::min( name_len, MAX_LEN_PRESET_NAME ) );

	size_t ext_len = strlen( name );
	memcpy( clip_data::preset[ clip_data::preset_count ].ext, ext, std::min( ext_len, MAX_LEN_EXT ) );

	return &clip_data::preset[ clip_data::preset_count++ ];
}


// ========================================================================================================


clip_prefix_t* clip_create_prefix()
{
	if ( array_append( clip_data::prefix, clip_data::prefix_count ) )
		return nullptr;

	return &clip_data::prefix[ clip_data::prefix_count++ ];
}


clip_encode_preset_t* clip_create_encode_preset()
{
	if ( array_append( clip_data::preset, clip_data::preset_count ) )
		return nullptr;

	return &clip_data::preset[ clip_data::preset_count++ ];
}


clip_encode_preset_t* clip_get_encode_preset( u32 preset_i )
{
	if ( preset_i >= clip_data::preset_count )
		return nullptr;

	return &clip_data::preset[ preset_i ];
}


// ========================================================================================================


char* clip_replay_name_trim( const char* name, u32 prefix_len )
{
	char* custom_name = util_strdup( name + prefix_len );

	if ( !custom_name )
		return nullptr;

	// replace the space with an underscore
	custom_name[ 10 ] = '_';

	//"2019-12-15";
	// "2019-12-15 23-55-40";
	// title from the end of the string (usually if more than 19 left characters)
// 	if ( strlen( custom_name ) > 19 )
// 	{
// 		char* title = strstr( custom_name, " - " );
// 
// 		if ( title )
// 		{
// 			*title = 0;
// 		}
// 	}

	return custom_name;
}


// ========================================================================================================
// Video Management


clip_t* clip_add_entry( const char* name )
{
	clip_t* new_data = ch_realloc< clip_t >( clip_data::clip, clip_data::clip_count + 1 );

	if ( !new_data )
		return nullptr;

	clip_data::clip = new_data;
	memset( &clip_data::clip[ clip_data::clip_count ], 0, sizeof( clip_t ) );

	clip_t* clip = &clip_data::clip[ clip_data::clip_count ];
	clip->name                = fs_get_filename_no_ext( name );
	clip->enabled             = true;

	size_t name_len             = strlen( name );

	// convenience, if it starts with "Replay ", or "Replay_" (2019 clips), remove it
	// then also check if it has a title appended to it
	// TODO: just check if it has Replay in it, because on older clips i tended to add a title to the start instead of the end
	if ( name_len > 7 && strncmp( clip->name, "Replay ", 7 ) == 0 )
	{
		char* custom_name = clip_replay_name_trim( clip->name, 7 );

		if ( custom_name )
		{
			free( clip->name );
			clip->name = custom_name;
		}
	}
	else if ( name_len > 9 && strncmp( clip->name, "Replay__ ", 9 ) == 0 )
	{
		char* custom_name = clip_replay_name_trim( clip->name, 9 );

		if ( custom_name )
		{
			free( clip->name );
			clip->name = custom_name;
		}
	}

	clip_data::clip_count++;

	// add a group to it

	return clip;
}


void clip_remove_entry( clip_t* clip )
{
	// look for the pointer
	u32 entry_i = 0;
	for ( ; entry_i < clip_data::clip_count; entry_i++ )
	{
		if ( &clip_data::clip[ entry_i ] == clip )
			break;
	}

	if ( entry_i == clip_data::clip_count )
	{
		log_printf( "invalid output\n" );
		return;
	}

	clip_remove_entry( entry_i );
}


void clip_remove_entry( u32 output_i )
{
	if ( output_i > clip_data::clip_count )
	{
		log_printf( "invalid output index\n" );
		return;
	}

	clip_t& clip = clip_data::clip[ output_i ];

	// remove source videos
	for ( u32 i = 0; i < clip.source_count; i++ )
	{
		free( clip.source[ i ].path );
	}

	free( clip.source );

	util_array_remove_element( clip_data::clip, clip_data::clip_count, output_i );
}


void clip_move_entry( u32 entry_id, u32 insert_position )
{
	if ( entry_id == insert_position )
		return;

	if ( entry_id >= clip_data::clip_count )
		return;

	if ( insert_position >= clip_data::clip_count )
		return;

	clip_t* temp_data = ch_calloc< clip_t >( 1 );

	if ( !temp_data )
	{
		log_printf( "Failed to allocate temp data to reorder output video\n" );
		return;
	}

	// back up this data
	memcpy( temp_data, &clip_data::clip[ entry_id ], sizeof( clip_t ) );

	if ( entry_id > insert_position )
	{
		// we want to move this output to an earlier spot in memor
		// shift everything between the insert position and original output position forward by 1
		u32 move_count = entry_id - insert_position;
		memmove( clip_data::clip + insert_position + 1, clip_data::clip + insert_position, sizeof( clip_t ) * move_count );

		// now copy back the data
		memcpy( &clip_data::clip[ insert_position ], temp_data, sizeof( clip_t ) );
	}
	else
	{
		// we want to move this output to a further away spot in memory

		// shift everything between the insert position and original output position back by 1
		u32 move_count = insert_position - entry_id;
		memmove( clip_data::clip + entry_id, clip_data::clip + entry_id + 1, sizeof( clip_t ) * move_count );

		// now copy back the data
		memcpy( &clip_data::clip[ insert_position ], temp_data, sizeof( clip_t ) );
	}

	//u32 move_count = clip_data::clip_count - insert_position;
	//memmove( clip_data::output, clip_data::output + insert_position, sizeof( clip_output_video_t ) * move_count );

	free( temp_data );
}


// ========================================================================================================


u32 clip_add_source( clip_t* clip, const char* path )
{
	if ( !clip )
		return UINT32_MAX;
	
	// Search if this exists already
	for ( u32 i = 0; i < clip->source_count; i++ )
	{
		if ( strcmp( clip->source[ i ].path, path ) == 0 )
		{
			return i;
		}
	}

	if ( clip->source_count > 0 )
		printf( "WOW2\n" );

	// Not found, add it
	clip_source_t* new_data = ch_realloc< clip_source_t >( clip->source, clip->source_count + 1 );

	if ( !new_data )
		return UINT32_MAX;

	clip->source = new_data;
	memset( &clip->source[ clip->source_count ], 0, sizeof( clip_source_t ) );

	clip_source_t* source = &clip->source[ clip->source_count ];
	source->path          = util_strdup( path );
	source->filename      = fs_get_filename( source->path );

	clip_get_video_metadata( *source );

	return clip->source_count++;
}


void clip_remove_source( clip_t* clip, u32 source_i )
{
	if ( !clip )
		return;

	if ( source_i > clip->source_count )
	{
		log_printf( "invalid source index\n" );
		return;
	}

	// Remove usages from groups
	for ( u32 g = 0; clip_group_t& group : clip->groups )
	{
		for ( u32 i = 0; i < group.sources.size(); )
		{
			clip_source_usage_t& source_use = group.sources[ i ];

			if ( source_use.source_index == source_i )
			{
				clip_group_remove_source( clip, g, i );
				continue;
			}

			if ( source_use.source_index > source_i )
				source_use.source_index--;

			i++;
		}

		g++;
	}

	clip_source_t& source = clip->source[ source_i ];

	free( source.path );
	free( source.filename );

	util_array_remove_element( clip->source, clip->source_count, source_i );
}


std::string clip_group_get_name( clip_t* clip, clip_group_t& group, bool error_names )
{
	if ( !clip )
		return {};

	// collect presets used
	char title[ 128 ]{};
	snprintf( title, 128, "Group: " );

	if ( group.presets.size() )
	{
		for ( u32 preset_i = 0; preset_i < group.presets.size(); preset_i++ )
		{
			clip_encode_preset_t& preset = clip_data::preset[ group.presets[ preset_i ] ];
			strcat( title, preset.name );

			if ( preset_i + 1 < group.presets.size() )
				strcat( title, ", " );
		}
	}
	else if ( error_names )
	{
		strcat( title, "[NO PRESETS]" );
	}

	if ( error_names )
	{
		if ( group.sources.empty() )
			strcat( title, " [NO SOURCES]" );

		for ( u32 source_i = 0; source_i < group.sources.size(); source_i++ )
		{
			clip_source_usage_t& source_use = group.sources[ source_i ];

			if ( source_use.time_range.empty() )
			{
				strcat( title, " [NO SECTIONS]" );
				break;
			}

			if ( source_use.source_index > clip->source_count )
			{
				strcat( title, " [INVALID SOURCE]" );
				break;
			}

			clip_source_t& source = clip->source[ source_use.source_index ];

			for ( u32 time_i = 0; time_i < source_use.time_range.size(); time_i++ )
			{
				if ( !valid_time_range( source_use.time_range[ time_i ], source.metadata ) )
				{
					strcat( title, " [INVALID SECTIONS]" );
					return title;
				}
			}
		}
	}

	return title;
}


u32  clip_group_add_source( clip_t* clip, u32 group_index, const char* path )
{
	if ( !clip )
		return UINT32_MAX;

	u32 source_i = clip_add_source( clip, path );

	if ( source_i == UINT32_MAX )
		return UINT32_MAX;

	if ( group_index >= clip->groups.size() )
		return UINT32_MAX;

	clip_group_t& group = clip->groups[ group_index ];

	clip_source_usage_t& source = group.sources.emplace_back();
	source.source_index         = source_i;

	clip_check_video( *clip, true );

	return group.sources.size() - 1;
}


void clip_group_remove_source( clip_t* clip, u32 group_index, u32 group_src_i )
{
	if ( !clip )
		return;

	if ( group_index >= clip->groups.size() )
		return;

	clip_group_t& group = clip->groups[ group_index ];

	if ( group_src_i >= group.sources.size() )
		return;

	group.sources.remove( group_src_i );

	if ( clip == clip_data::current_clip )
	{
		if ( clip_data::current_group_source[ group_index ] > 0 && clip_data::current_group_source[ group_index ] == group.sources.size() )
			clip_data::current_group_source[ group_index ]--;

		if ( group.sources.size() )
		{
			// clip_data::current_source = group->sources[ clip_data::current_group_source ].source_index;
			//replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, clip_data::get_current_group_source() );
		}
		else
		{
			//replay_editor_set_group( clip_data::current_clip_index, clip_data::current_group, 0 );
			clip_data::current_source       = UINT32_MAX;
			//clip_data::current_group_source = UINT32_MAX;
		}
	}

	clip_check_video( *clip, true );
}


void clip_group_remove_source( clip_t* clip, u32 group_index, const char* path )
{
	if ( !clip )
		return;

	for ( u32 i = 0; i < clip->source_count; i++ )
	{
		if ( strcmp( clip->source[ i ].path, path ) == 0 )
		{
			return clip_group_remove_source( clip, group_index, i );
		}
	}
}


void clip_group_add_preset( clip_t& clip, clip_group_t& group, u32 preset_i )
{
	if ( group.presets.index( preset_i ) != UINT32_MAX )
		return;

	group.presets.push_back( preset_i );
	clip_check_video( clip );
}


void clip_group_remove_preset( clip_t& clip, clip_group_t& group, u32 preset_i )
{
	if ( preset_i > group.presets.size() )
		return;

	group.presets.remove( preset_i );
	clip_check_video( clip );

	// for ( u32 i = 0; i < group.presets.size(); i++ )
	// {
	// 	if ( group.presets[ i ] == preset_i )
	// 	{
	// 		group.presets.remove( i );
	// 		clip_check_video( clip );
	// 		break;
	// 	}
	// }
}


void clip_group_remove_preset( clip_t& clip, u32 group_index, u32 preset_i )
{
	if ( group_index >= clip.groups.size() )
		return;

	clip_group_t& group = clip.groups[ group_index ];
	clip_group_remove_preset( clip, group, preset_i );
}


void clip_group_add_time_range( clip_t* clip, clip_group_t& group, u32 source_i, float start_time, float end_time )
{
	if ( !clip )
		return;

	if ( source_i > group.sources.size() )
	{
		log_printf( "invalid source index\n" );
		return;
	}

	clip_source_usage_t& source   = group.sources[ source_i ];

	// What if you made time ranges linked lists? maybe the same with source videos? would allow for easy re-ordering
	clip_time_range_t&   new_data = source.time_range.emplace_back();

	new_data.start                = start_time;
	new_data.end                  = end_time;
}


void clip_group_remove_time_range( clip_t* clip, clip_group_t& group, u32 source_i, u32 time_range )
{
	if ( !clip )
		return;

	if ( source_i > clip->source_count )
	{
		log_printf( "invalid source index\n" );
		return;
	}

	clip_source_usage_t& source = group.sources[ source_i ];

	if ( time_range > source.time_range.size() )
	{
		log_printf( "invalid time range index\n" );
		return;
	}

	source.time_range.remove( time_range );
}


// direction = false for back, true for forward
void clip_group_shift_time_range( clip_t* clip, clip_group_t& group, u32 source_i, u32 time_range_i, bool direction )
{
	if ( !clip )
		return;

	if ( source_i > clip->source_count )
	{
		log_printf( "invalid source index\n" );
		return;
	}

	clip_source_usage_t& source = group.sources[ source_i ];

	if ( time_range_i > source.time_range.size() )
	{
		log_printf( "invalid time range index\n" );
		return;
	}

	if ( source.time_range.size() == 1 )
		return;

	clip_time_range_t time_range = source.time_range[ time_range_i ];

	if ( direction )
	{
		// check if valid move
		if ( time_range_i + 1 < source.time_range.size() )
		{
			source.time_range.insert( time_range, time_range_i + 2 );
			source.time_range.remove( time_range_i );
		}
	}
	else
	{
		// check if valid move
		if ( time_range_i > 0 )
		{
			source.time_range.insert( time_range, time_range_i - 1 );
			source.time_range.remove( time_range_i + 1 );
		}
	}
}


void clip_duplicate_time_range( clip_t* clip, u32 input_i, u32 src_time_range_i )
{
#if 0
	if ( !clip )
		return;

	if ( input_i > clip->source_count )
	{
		log_printf( "invalid source index\n" );
		return;
	}

	clip_source_t& source = clip->source[ input_i ];

	if ( src_time_range_i > source.time_range_count )
	{
		log_printf( "invalid time range to duplicate\n" );
		return;
	}

	if ( array_append( source.time_range, source.time_range_count ) )
		return;

	clip_time_range_t& src_time = source.time_range[ src_time_range_i ];
	clip_time_range_t& dst_time = source.time_range[ source.time_range_count++ ];

	dst_time.start              = src_time.start;
	dst_time.end                = src_time.end;
#endif
}


clip_group_t* clip_get_group( clip_t* clip, u32 group_index )
{
	if ( !clip )
		return nullptr;

	if ( group_index >= clip->groups.size() )
		return nullptr;

	return &clip->groups[ group_index ];
}


