#include "clip.h"
#include "logging.h"
#include "main.h"


std::unordered_map< std::string, video_metadata_t > g_video_metadata_map;


namespace clip_data
{
	u32                   version              = CLIP_VIDEO_FORMAT_VER;

	clip_output_video_t*  output               = nullptr;
	u32                   output_count         = 0;

	clip_encode_preset_t* preset               = nullptr;
	u32                   preset_count         = 0;

	clip_prefix_t*        prefix               = nullptr;
	u32                   prefix_count         = 0;

	clip_output_video_t*  current_output       = nullptr;
	u32                   current_output_index = UINT32_MAX;
	u32                   current_source       = 0;
	u32                   current_group_source = 0;
	u32                   current_group        = 0;
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


clip_output_video_t* clip_add_output( const char* name )
{
	clip_output_video_t* new_data = ch_realloc< clip_output_video_t >( clip_data::output, clip_data::output_count + 1 );

	if ( !new_data )
		return nullptr;

	clip_data::output = new_data;
	memset( &clip_data::output[ clip_data::output_count ], 0, sizeof( clip_output_video_t ) );

	clip_output_video_t* output = &clip_data::output[ clip_data::output_count ];
	output->name                = fs_get_filename_no_ext( name );
	output->enabled             = true;

	// convenience, if it starts with "Replay ", or "Replay_" (2019 clips), remove it
	// then also check if it has a title appended to it
	// TODO: just check if it has Replay in it, because on older clips i tended to add a title to the start instead of the end
	if ( strncmp( output->name, "Replay ", 7 ) == 0 )
	{
		char* custom_name = clip_replay_name_trim( output->name, 7 );

		if ( custom_name )
		{
			free( output->name );
			output->name = custom_name;
		}
	}
	else if ( strncmp( output->name, "Replay__ ", 9 ) == 0 )
	{
		char* custom_name = clip_replay_name_trim( output->name, 9 );

		if ( custom_name )
		{
			free( output->name );
			output->name = custom_name;
		}
	}

	clip_data::output_count++;

	// add a group to it

	return output;
}


void clip_remove_output( clip_output_video_t* output )
{
	// look for the pointer
	u32 output_i = 0;
	for ( ; output_i < clip_data::output_count; output_i++ )
	{
		if ( &clip_data::output[ output_i ] == output )
			break;
	}

	if ( output_i == clip_data::output_count )
	{
		log_printf( "invalid output\n" );
		return;
	}

	clip_remove_output( output_i );
}


void clip_remove_output( u32 output_i )
{
	if ( output_i > clip_data::output_count )
	{
		log_printf( "invalid output index\n" );
		return;
	}

	clip_output_video_t& output = clip_data::output[ output_i ];

	// remove source videos
	for ( u32 i = 0; i < output.source_count; i++ )
	{
		free( output.source[ i ].path );
	}

	free( output.source );

	util_array_remove_element( clip_data::output, clip_data::output_count, output_i );
}


void clip_move_output( u32 output_id, u32 insert_position )
{
	if ( output_id == insert_position )
		return;

	if ( output_id >= clip_data::output_count )
		return;

	if ( insert_position >= clip_data::output_count )
		return;

	clip_output_video_t* temp_data = ch_calloc< clip_output_video_t >( 1 );

	if ( !temp_data )
	{
		log_printf( "Failed to allocate temp data to reorder output video\n" );
		return;
	}

	// back up this data
	memcpy( temp_data, &clip_data::output[ output_id ], sizeof( clip_output_video_t ) );

	if ( output_id > insert_position )
	{
		// we want to move this output to an earlier spot in memor
		// shift everything between the insert position and original output position forward by 1
		u32 move_count = output_id - insert_position;
		memmove( clip_data::output + insert_position + 1, clip_data::output + insert_position, sizeof( clip_output_video_t ) * move_count );

		// now copy back the data
		memcpy( &clip_data::output[ insert_position ], temp_data, sizeof( clip_output_video_t ) );
	}
	else
	{
		// we want to move this output to a further away spot in memory

		// shift everything between the insert position and original output position back by 1
		u32 move_count = insert_position - output_id;
		memmove( clip_data::output + output_id, clip_data::output + output_id + 1, sizeof( clip_output_video_t ) * move_count );

		// now copy back the data
		memcpy( &clip_data::output[ insert_position ], temp_data, sizeof( clip_output_video_t ) );
	}

	//u32 move_count = clip_data::output_count - insert_position;
	//memmove( clip_data::output, clip_data::output + insert_position, sizeof( clip_output_video_t ) * move_count );

	free( temp_data );
}


// ========================================================================================================


u32 clip_add_source( clip_output_video_t* output, const char* path )
{
	if ( !output )
		return UINT32_MAX;
	
	// Search if this exists already
	for ( u32 i = 0; i < output->source_count; i++ )
	{
		if ( strcmp( output->source[ i ].path, path ) == 0 )
		{
			return i;
		}
	}

	// Not found, add it
	clip_source_t* new_data = ch_realloc< clip_source_t >( output->source, output->source_count + 1 );

	if ( !new_data )
		return UINT32_MAX;

	output->source = new_data;
	memset( &output->source[ output->source_count ], 0, sizeof( clip_source_t ) );

	clip_source_t* source = &output->source[ output->source_count ];
	source->path          = ch_malloc< char >( strlen( path ) + 1 );

	strcpy( source->path, path );
	source->path[ strlen( path ) ] = 0;
	source->filename                = fs_get_filename( source->path );

	clip_get_video_metadata( *source );

	return output->source_count++;
}


void clip_remove_source( clip_output_video_t* output, u32 input_i )
{
	if ( !output )
		return;

	if ( input_i > output->source_count )
	{
		log_printf( "invalid source index\n" );
		return;
	}

	clip_source_t& source = output->source[ input_i ];

	free( source.path );
	free( source.filename );

	util_array_remove_element( output->source, output->source_count, input_i );
}


#if 0


u32 clip_add_source_to_preset( clip_output_video_t* output, u32 preset_index, const char* path )
{
	if ( !output )
		return UINT32_MAX;

	u32 source_i = clip_add_input( output, path );

	if ( source_i == UINT32_MAX )
		return UINT32_MAX;

	for ( clip_output_group_t& preset_out : output->groups )
	{
		if ( preset_out.presets.index( preset_index ) != UINT32_MAX )
		{
			clip_source_usage_t& source = preset_out.sources.emplace_back();
			source.source_index         = source_i;
			return preset_out.sources.size() - 1;
		}
	}

	return UINT32_MAX;
}


void clip_remove_source_from_preset( clip_output_video_t* output, u32 preset_index, u32 preset_src_i )
{
	if ( !output )
		return;

	if ( preset_index >= output->groups.size() )
		return;

	clip_output_group_t& preset_out = output->groups[ preset_index ];

	if ( preset_src_i >= preset_out.sources.size() )
		return;

	preset_out.sources.remove( preset_src_i );
}


void clip_remove_source_from_preset( clip_output_video_t* output, u32 preset_index, const char* path )
{
	if ( !output )
		return;

	for ( u32 i = 0; i < output->source_count; i++ )
	{
		if ( strcmp( output->source[ i ].path, path ) == 0 )
		{
			return clip_remove_source_from_preset( output, preset_index, i );
		}
	}
}


#endif


std::string clip_group_get_name( clip_output_group_t& group )
{
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
	else
	{
		strcat( title, "[NO PRESETS]" );

		ImGui::PushStyleColor( ImGuiCol_Tab, COLOR_BTN_RED );
		ImGui::PushStyleColor( ImGuiCol_TabHovered, COLOR_BTN_RED_HOVER );
		ImGui::PushStyleColor( ImGuiCol_TabSelected, COLOR_BTN_RED_ACTIVE );
	}

	return title;
}


u32  clip_group_add_source( clip_output_video_t* output, u32 group_index, const char* path )
{
	if ( !output )
		return UINT32_MAX;

	u32 source_i = clip_add_source( output, path );

	if ( source_i == UINT32_MAX )
		return UINT32_MAX;

	if ( group_index >= output->groups.size() )
		return UINT32_MAX;

	clip_output_group_t& group = output->groups[ group_index ];

	clip_source_usage_t& source = group.sources.emplace_back();
	source.source_index         = source_i;

	clip_check_video( *output );

	return group.sources.size() - 1;
}


void clip_group_remove_source( clip_output_video_t* output, u32 group_index, u32 group_src_i )
{
	if ( !output )
		return;

	if ( group_index >= output->groups.size() )
		return;

	clip_output_group_t& group = output->groups[ group_index ];

	if ( group_src_i >= group.sources.size() )
		return;

	group.sources.remove( group_src_i );

	clip_check_video( *output );
}


void clip_group_remove_source( clip_output_video_t* output, u32 group_index, const char* path )
{
	if ( !output )
		return;

	for ( u32 i = 0; i < output->source_count; i++ )
	{
		if ( strcmp( output->source[ i ].path, path ) == 0 )
		{
			return clip_group_remove_source( output, group_index, i );
		}
	}
}


void clip_group_add_preset( clip_output_video_t& output, clip_output_group_t& group, u32 preset_i )
{
	if ( group.presets.index( preset_i ) != UINT32_MAX )
		return;

	group.presets.push_back( preset_i );
	clip_check_video( output );
}


void clip_group_remove_preset( clip_output_video_t& output, clip_output_group_t& group, u32 preset_i )
{
	for ( u32 i = 0; i < group.presets.size(); i++ )
	{
		if ( group.presets[ i ] == preset_i )
		{
			group.presets.remove( i );
			clip_check_video( output );
			break;
		}
	}
}


void clip_group_remove_preset( clip_output_video_t& output, u32 group_index, u32 preset_i )
{
	if ( group_index >= output.groups.size() )
		return;

	clip_output_group_t& group = output.groups[ group_index ];
	clip_group_remove_preset( output, group, preset_i );
}


// REMOVE ME
void duplicate_encode_overrides( clip_encode_settings_t& src, clip_encode_settings_t& dst )
{
	if ( src.presets )
	{
		dst.presets_count = src.presets_count;
		dst.presets       = ch_malloc< u32 >( src.presets_count );

		memcpy( dst.presets, src.presets, sizeof( u32 ) * src.presets_count );
	}
}


u32 clip_duplicate_input( clip_output_video_t* output, u32 input_i )
{
	return UINT32_MAX;

#if 0
	if ( !output )
		return UINT32_MAX;

	if ( input_i >= output->source_count )
	{
		log_printf( "invalid source index\n" );
		return UINT32_MAX;
	}

	u32 input_dst_i = clip_add_input( output, output->source[ input_i ].path );

	if ( input_dst_i == UINT32_MAX )
		return UINT32_MAX;

	clip_source_t& input_src = output->source[ input_i ];
	clip_source_t& input_dst = output->source[ input_dst_i ];
	input_dst.metadata            = input_src.metadata;

	if ( input_src.time_range_count )
	{
		input_dst.time_range_count = input_src.time_range_count;
		input_dst.time_range       = ch_malloc< clip_time_range_t >( input_src.time_range_count );

		for ( u32 i = 0; i < input_src.time_range_count; i++ )
		{
			input_dst.time_range[ i ].start = input_src.time_range[ i ].start;
			input_dst.time_range[ i ].end   = input_src.time_range[ i ].end;
		}
	}

	duplicate_encode_overrides( input_src.encode_settings, input_dst.encode_settings );

	return input_dst_i;
#endif
}


void clip_add_time_range( clip_output_video_t* output, u32 input_i, float start_time, float end_time )
{
#if 0
	if ( !output )
		return;

	if ( input_i > output->source_count )
	{
		log_printf( "invalid source index\n" );
		return;
	}

	clip_source_t& source = output->source[ input_i ];

	// What if you made time ranges linked lists? maybe the same with source videos? would allow for easy re-ordering
	clip_time_range_t* new_data = ch_realloc< clip_time_range_t >( source.time_range, source.time_range_count + 1 );

	if ( !new_data )
		return;

	source.time_range = new_data;
	memset( &source.time_range[ source.time_range_count ], 0, sizeof( clip_time_range_t ) );

	source.time_range[ source.time_range_count ].start = start_time;
	source.time_range[ source.time_range_count ].end   = end_time;

	source.time_range_count++;
#endif
}


void clip_remove_time_range( clip_output_video_t* output, u32 input_i, u32 time_range )
{
#if 0
	if ( !output )
		return;

	if ( input_i > output->source_count )
	{
		log_printf( "invalid source index\n" );
		return;
	}

	clip_source_t& source = output->source[ input_i ];
	util_array_remove_element( source.time_range, source.time_range_count, time_range );
#endif
}


void clip_duplicate_time_range( clip_output_video_t* output, u32 input_i, u32 src_time_range_i )
{
#if 0
	if ( !output )
		return;

	if ( input_i > output->source_count )
	{
		log_printf( "invalid source index\n" );
		return;
	}

	clip_source_t& source = output->source[ input_i ];

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


// REMOVE ME
void clip_add_preset_to_encode_override( clip_encode_settings_t& override, u32 preset_index )
{
	if ( array_append( override.presets, override.presets_count ) )
		return;

	override.presets[ override.presets_count++ ] = preset_index;
}


// REMOVE ME
void clip_add_preset_to_encode_override( clip_encode_settings_t& override, const char* preset_name )
{
	// look for a preset with this name
	for ( u32 i = 0; i < clip_data::preset_count; i++ )
	{
		if ( strcmp( clip_data::preset[ i ].name, preset_name ) == 0 )
		{
			clip_add_preset_to_encode_override( override, i );
			return;
		}
	}

	log_printf( "Failed to find preset: %s\n", preset_name );
}


// REMOVE ME
void clip_add_preset( clip_output_video_t& output, u32 preset_index )
{
	// for ( clip_output_group_t& preset_out : output.groups )
	// {
	// 	if ( preset_out.preset == preset_index )
	// 		break;
	// }
	// 
	// clip_output_group_t& preset_out = output.groups.emplace_back();
	// preset_out.preset                = preset_index;
}


// REMOVE ME
void clip_remove_preset( clip_output_video_t& output, u32 preset_index )
{
	// for ( size_t i = 0; i < output.groups.size(); i++ )
	// {
	// 	if ( output.groups[ i ].preset != preset_index )
	// 		continue;
	// 
	// 	output.groups.remove( i );
	// 	break;
	// }
}


clip_output_group_t* clip_get_group( clip_output_video_t* output, u32 group_index )
{
	if ( !output )
		return nullptr;

	if ( group_index >= output->groups.size() )
		return nullptr;

	return &output->groups[ group_index ];
}


