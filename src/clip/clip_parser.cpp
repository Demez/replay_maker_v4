#include "clip.h"
#include "logging.h"
#include "main.h"


extern std::unordered_map< std::string, video_metadata_t > g_video_metadata_map;


// ============================================================================================================================


static void clip_parse_encode_presets( json_object_t& json )
{
	if ( json.aType != e_json_type_array )
	{
		log_printf( "encode_presets is not an array!\n" );
		return;
	}

	for ( size_t root_i = 0; root_i < json.aObjects.count; root_i++ )
	{
		json_object_t& object = json.aObjects.data[ root_i ];

		if ( object.aType != e_json_type_object )
		{
			log_printf( "encode preset entry is not an object!\n" );
			continue;
		}

		clip_encode_preset_t* encode = clip_create_encode_preset();

		if ( !encode )
		{
			log_printf( "failed to create encode preset?\n" );
			return;
		}

		for ( size_t object_i = 0; object_i < object.aObjects.count; object_i++ )
		{
			json_object_t& field = object.aObjects.data[ object_i ];

			if ( field.aType == e_json_type_string )
			{
				if ( util_strncmp( "name", 4, field.aName.data, field.aName.size ) )
				{
					memcpy( encode->name, field.aString.data, MIN( MAX_LEN_PRESET_NAME, field.aString.size ) * sizeof( char ) );
				}
				else if ( util_strncmp( "ext", 3, field.aName.data, field.aName.size ) )
				{
					memcpy( encode->ext, field.aString.data, MIN( MAX_LEN_EXT, field.aString.size ) * sizeof( char ) );
				}
				else if ( util_strncmp( "ffmpeg_cmd", 10, field.aName.data, field.aName.size ) )
				{
					encode->ffmpeg_cmd = strdup( field.aString.data );
				}
				else if ( util_strncmp( "out_folder_append", 17, field.aName.data, field.aName.size ) )
				{
					encode->out_folder_append = strdup( field.aString.data );
				}
				else if ( util_strncmp( "out_prefix", 10, field.aName.data, field.aName.size ) )
				{
					encode->out_prefix = strdup( field.aString.data );
				}
				else
				{
					log_printf( "unknown encode preset string field: \"%s\"\n", field.aName.data );
				}
			}
			else if ( field.aType == e_json_type_int )
			{
				if ( util_strncmp( "target_size", 11, field.aName.data, field.aName.size ) )
				{
					encode->target_size = field.aInt;
				}
				else if ( util_strncmp( "target_size_max", 15, field.aName.data, field.aName.size ) )
				{
					encode->target_size_max = field.aInt;
				}
				else if ( util_strncmp( "target_size_min", 15, field.aName.data, field.aName.size ) )
				{
					encode->target_size_min = field.aInt;
				}
				else if ( util_strncmp( "audio_bitrate", 13, field.aName.data, field.aName.size ) )
				{
					encode->audio_bitrate = field.aInt;
				}
				else
				{
					log_printf( "unknown encode preset int field: \"%s\"\n", field.aName.data );
				}
			}
			else
			{
				log_printf( "unknown encode preset field: \"%s\"\n", field.aName.data );
			}
		}
	}
}


static void clip_parse_prefixes( json_object_t& json )
{
	if ( json.aType != e_json_type_array )
	{
		log_printf( "encode_presets is not an array!\n" );
		return;
	}

	for ( size_t root_i = 0; root_i < json.aObjects.count; root_i++ )
	{
		json_object_t& object = json.aObjects.data[ root_i ];

		if ( object.aType != e_json_type_object )
		{
			log_printf( "encode preset entry is not an object!\n" );
			continue;
		}

		if ( object.aObjects.count > 1 )
		{
			log_printf( "extra data in preset entry?\n" );
			continue;
		}

		json_object_t& field = object.aObjects.data[ 0 ];

		if ( field.aType != e_json_type_string )
		{
			log_printf( "invalid encode preset field type - expected string: \"%s\"\n", field.aName.data );
			continue;
		}

		clip_prefix_t* prefix = clip_create_prefix();

		if ( !prefix )
		{
			log_printf( "failed to create prefix?\n" );
			return;
		}

		memcpy( prefix->name, field.aName.data, MIN( MAX_LEN_PRESET_NAME, field.aName.size ) * sizeof( char ) );
		prefix->prefix = strdup( field.aString.data );
	}
}


bool clip_parse_settings( const char* path )
{
	char* file = fs_read_file( path );

	if ( !file )
	{
		log_printf( "failed to read file for settings\n" );
		return false;
	}

	json_object_t root{};
	EJsonError    err = json_parse( root, file );

	if ( err != EJsonError_None )
	{
		log_printf( "Error Parsing Json - %s\n", json_error_to_str( err ) );
		return false;
	}

	if ( root.aType != e_json_type_object )
	{
		log_printf( "%s - Root Type is not an Object\n", path );
		return false;
	}

	for ( size_t root_i = 0; root_i < root.aObjects.count; root_i++ )
	{
		json_object_t& object = root.aObjects.data[ root_i ];

		if ( util_strncmp( "encode_presets", 14, object.aName.data, object.aName.size ) )
		{
			clip_parse_encode_presets( object );
		}
		else if ( util_strncmp( "prefixes", 8, object.aName.data, object.aName.size ) )
		{
			clip_parse_prefixes( object );
		}
	}

	json_free( root );
	free( file );

	return true;
}


// ============================================================================================================================


ChVector< u32 > clip_parse_encode_override( clip_output_video_t& output, json_object_t& root )
{
	ChVector< u32 > presets_used{};

	if ( root.aType != e_json_type_object )
	{
		log_printf( "expected encode_override to be an object!\n" );
		return presets_used;
	}

	for ( size_t root_i = 0; root_i < root.aObjects.count; root_i++ )
	{
		json_object_t& object = root.aObjects.data[ root_i ];

		if ( util_strncmp( "presets", 7, object.aName.data, object.aName.size ) )
		{
			if ( object.aType != e_json_type_array )
			{
				log_printf( "Expected array of strings for presets value in encode_overrides\n" );
				continue;
			}

			for ( size_t preset_name_i = 0; preset_name_i < object.aObjects.count; preset_name_i++ )
			{
				json_object_t& json_preset = object.aObjects.data[ preset_name_i ];

				// look for the preset
				for ( u32 preset_i = 0; preset_i < clip_data::preset_count; preset_i++ )
				{
					if ( !util_strncmp( json_preset.aString.data, json_preset.aString.size, clip_data::preset[ preset_i ].name, strlen( clip_data::preset[ preset_i ].name ) ) )
						continue;

					presets_used.push_back( preset_i );
					break;
				}
			}
		}
	}

	return presets_used;
}


clip_source_usage_t* clip_get_source_use_v3()
{
	return nullptr;
}


u32  clip_add_source( clip_output_video_t* output, const char* path );


// version 3 parsing
bool clip_parse_input_v3( clip_output_video_t& output, json_object_t& root )
{
	if ( root.aType != e_json_type_object )
	{
		log_printf( "expected video source to be an object!\n" );
		return false;
	}

	clip_source_t*       source   = nullptr;
	u32                  source_i = UINT32_MAX;
	clip_output_group_t* group    = nullptr;

	for ( size_t root_i = 0; root_i < root.aObjects.count; root_i++ )
	{
		json_object_t& object = root.aObjects.data[ root_i ];

		if ( util_strncmp( "path", 4, object.aName.data, object.aName.size ) )
		{
			source_i = clip_add_source( &output, object.aString.data );

			if ( source_i == UINT32_MAX )
				return false;

			source = &output.source[ source_i ];
		}
		else if ( util_strncmp( "encode_overrides", 16, object.aName.data, object.aName.size ) )
		{
			// get the encode presets used first
			ChVector< u32 > encode_presets = clip_parse_encode_override( output, object );

			// then look for a group with a matching set of encode presets used
			for ( u32 group_i = 0; group_i < output.groups.size(); group_i++ )
			{
				clip_output_group_t& _group = output.groups[ group_i ];

				if ( _group.presets.size() != encode_presets.size() )
					continue;

				bool all_valid = true;
				u32  src_preset_i = 0;
				for ( u32 preset_i : _group.presets )
				{
					if ( preset_i != encode_presets[ src_preset_i++ ] )
					{
						all_valid = false;
						break;
					}
				}

				if ( all_valid )
				{
					group = &_group;
					break;
				}
			}

			if ( !group )
			{
				group          = &output.groups.emplace_back();
				group->presets = encode_presets;
			}
		}
		else if ( util_strncmp( "time_ranges", 11, object.aName.data, object.aName.size ) )
		{
#if 01
			if ( !group )
			{
				// INCORRECT ORDERING !!!!
				log_printf( "no group created before time_ranges parsed !!\n" );
				continue;
			}

			if ( object.aType != e_json_type_array )
			{
				log_printf( "expected time_ranges to be an array!\n" );
				continue;
			}

			clip_source_usage_t& source_use = group->sources.emplace_back();
			source_use.source_index         = source_i;

			for ( size_t range_i = 0; range_i < object.aObjects.count; range_i++ )
			{
				json_object_t& range_json = object.aObjects.data[ range_i ];
				
				if ( range_json.aType == e_json_type_object )
				{
					clip_time_range_t& time_range = source_use.time_range.emplace_back();

					for ( size_t time_i = 0; time_i < range_json.aObjects.count; time_i++ )
					{
						json_object_t& time_entry = range_json.aObjects.data[ time_i ];

						if ( util_strncmp( time_entry.aName.data, time_entry.aName.size, "start", 5 ) )
							time_range.start = time_entry.aDouble;

						else if ( util_strncmp( time_entry.aName.data, time_entry.aName.size, "end", 3 ) )
							time_range.end = time_entry.aDouble;
					}
				}
				else if ( range_json.aType == e_json_type_array )
				{
					if ( range_json.aObjects.count != 2 )
					{
						log_printf( "time_ranges entry does not have only 2 entires\n" );
						continue;
					}

					clip_time_range_t& time_range = source_use.time_range.emplace_back();
					time_range.start              = range_json.aObjects.data[ 0 ].aDouble;
					time_range.end                = range_json.aObjects.data[ 1 ].aDouble;
				}
			}
#endif
		}
		else
		{
			log_printf( "unknown source video property: \"%s\"\n", object.aName.data );
		}
	}

	// clip_get_video_metadata( source );

	return true;
}


bool clip_parse_output_group( clip_output_video_t& output, json_object_t& root, clip_output_group_t& group )
{
	if ( root.aType != e_json_type_object )
	{
		log_printf( "expected video source to be an object!\n" );
		return false;
	}

	for ( size_t root_i = 0; root_i < root.aObjects.count; root_i++ )
	{
		json_object_t& object = root.aObjects.data[ root_i ];

		if ( util_strncmp( "presets", 7, object.aName.data, object.aName.size ) )
		{
			if ( object.aType != e_json_type_array )
			{
				log_printf( "expected presets/sources to be an array!\n" );
				continue;
			}

			for ( size_t source_use_i = 0; source_use_i < object.aObjects.count; source_use_i++ )
			{
				json_object_t& obj_json = object.aObjects.data[ source_use_i ];

				// look for the preset
				for ( u32 preset_i = 0; preset_i < clip_data::preset_count; preset_i++ )
				{
					if ( !util_strncmp( obj_json.aString.data, obj_json.aString.size, clip_data::preset[ preset_i ].name, strlen( clip_data::preset[ preset_i ].name ) ) )
						continue;

					group.presets.push_back( preset_i );
					break;
				}
			}

			if ( group.presets.empty() )
			{
				printf( "Failed to find valid encode preset for video\n" );
				return false;
			}
		}
		else if ( util_strncmp( "sources", 7, object.aName.data, object.aName.size ) )
		{
			if ( object.aType != e_json_type_array )
			{
				log_printf( "expected presets/sources to be an array!\n" );
				continue;
			}

			group.sources.resize( object.aObjects.count );

			for ( size_t source_use_i = 0; source_use_i < object.aObjects.count; source_use_i++ )
			{
				json_object_t&       obj_json   = object.aObjects.data[ source_use_i ];
				clip_source_usage_t& source_use = group.sources[ source_use_i ];

				for ( size_t j = 0; j < obj_json.aObjects.count; j++ )
				{
					json_object_t& source_json = obj_json.aObjects.data[ j ];

					if ( util_strncmp( "source", 6, source_json.aName.data, source_json.aName.size ) )
					{
						if ( source_json.aType == e_json_type_int )
						{
							int source_i = source_json.aInt;

							// make sure this is valid
							if ( source_i < 0 || source_i > output.source_count )
							{
								log_printf( log_error, "source index out of bounds, got %d, only have %d sources\n", source_i, output.source_count );
								break;
							}
							else
							{
								source_use.source_index = source_i;
							}
						}
						else
						{
							log_printf( log_error, "expected source index to be an int, got %s\n", json_type_to_str( source_json.aType ) );
							break;
						}
					}
					else if ( util_strncmp( "time_ranges", 11, source_json.aName.data, source_json.aName.size ) )
					{
						source_use.time_range.resize( source_json.aObjects.count );

						for ( size_t range_i = 0; range_i < source_json.aObjects.count; range_i++ )
						{
							json_object_t& range_json = source_json.aObjects.data[ range_i ];

							if ( range_json.aType == e_json_type_array )
							{
								if ( range_json.aObjects.count != 2 )
								{
									log_printf( log_error, "time_ranges entry does not have only 2 entires\n" );
									continue;
								}

								clip_time_range_t& time_range = source_use.time_range[ range_i ];
								time_range.start              = range_json.aObjects.data[ 0 ].aDouble;
								time_range.end                = range_json.aObjects.data[ 1 ].aDouble;
							}
						}
					}
				}
			}
		}
		else
		{
			log_printf( "unknown source video property: \"%s\"\n", object.aName.data );
		}
	}

	return true;
}


void clip_parse_video( json_object_t& root, u32 output_i )
{
	clip_output_video_t& output = clip_data::output[ output_i ];
	output.enabled              = true;

	for ( size_t root_i = 0; root_i < root.aObjects.count; root_i++ )
	{
		json_object_t& object = root.aObjects.data[ root_i ];

		if ( util_strncmp( "name", 4, object.aName.data, object.aName.size ) )
		{
			output.name = strdup( object.aString.data );
		}
		else if ( util_strncmp( "prefix", 6, object.aName.data, object.aName.size ) )
		{
			// look for the prefix
			for ( u32 prefix_i = 0; prefix_i < clip_data::prefix_count; prefix_i++ )
			{
				if ( !util_strncmp( object.aString.data, object.aString.size, clip_data::prefix[ prefix_i ].name, strlen( clip_data::prefix[ prefix_i ].name ) ) )
					continue;

				output.prefix = prefix_i;
				break;
			}
		}
		// version 3
		else if ( util_strncmp( "inputs", 6, object.aName.data, object.aName.size ) )
		{
			if ( object.aType != e_json_type_array )
			{
				log_printf( "Expected array of objects for source videos\n" );
				continue;
			}

			for ( size_t video_i = 0; video_i < object.aObjects.count; video_i++ )
			{
				json_object_t& json_video = object.aObjects.data[ video_i ];
				clip_parse_input_v3( output, json_video );
			}
		}
		else if ( util_strncmp( "sources", 7, object.aName.data, object.aName.size ) )
		{
			if ( object.aType != e_json_type_array )
			{
				log_printf( "Expected array of strings for source videos\n" );
				continue;
			}

			output.source = ch_calloc< clip_source_t >( object.aObjects.count );

			if ( !output.source )
				continue;

			output.source_count = object.aObjects.count;

			for ( size_t video_i = 0; video_i < object.aObjects.count; video_i++ )
			{
				json_object_t& json_video = object.aObjects.data[ video_i ];

				if ( json_video.aType != e_json_type_string )
				{
					log_printf( log_error, "Expected string for source video path, got \"%s\"\n", json_type_to_str( json_video.aType ) );
				}
				else
				{
					output.source[ video_i ].path     = strdup( json_video.aString.data );
					output.source[ video_i ].filename = fs_get_filename( json_video.aString.data );
				}

				clip_get_video_metadata( output.source[ video_i ] );
			}
		}
		else if ( util_strncmp( "groups", 6, object.aName.data, object.aName.size ) )
		{
			if ( object.aType != e_json_type_array )
			{
				log_printf( "Expected array of objects for groups\n" );
				continue;
			}

			output.groups.clear();
			output.groups.resize( object.aObjects.count );

			for ( size_t preset_use_i = 0; preset_use_i < object.aObjects.count; preset_use_i++ )
			{
				json_object_t& json_preset_use = object.aObjects.data[ preset_use_i ];

				if ( json_preset_use.aType != e_json_type_object )
				{
					log_printf( log_error, "Expected object for output groups, got \"%s\"\n", json_type_to_str( json_preset_use.aType ) );
					continue;
				}

				clip_parse_output_group( output, json_preset_use, output.groups[ preset_use_i ] );
			}
		}
		else
		{
			log_printf( "unknown video property: \"%s\"\n", object.aName.data );
		}
	}

	clip_check_video( output );
}


bool clip_parse_videos( const char* path )
{
	if ( clip_data::output_count )
	{
		log_printf( "TODO: CLEAR OLD OUTPUT VIDEOS\n" );
	}

	char* file = fs_read_file( path );

	if ( !file )
	{
		log_printf( "failed to read file for videos\n" );
		return false;
	}

	clip_output_video_t* new_data    = nullptr;
	json_object_t*       videos_root = nullptr;

	json_object_t        root{};
	EJsonError           err = json_parse( root, file );

	if ( err != EJsonError_None )
	{
		log_printf( "Error Parsing Json - %s\n", json_error_to_str( err ) );
		goto fail;
	}

	if ( root.aType != e_json_type_object )
	{
		log_printf( "%s - Root Type is not an Object\n", path );
		goto fail;
	}

	if ( root.aObjects.count != 2 )
	{
		log_printf( "Root object count is not 2 entries (version, videos)\n" );
		goto fail;
	}

	if ( !root.aObjects.data[ 0 ].aName.size )
	{
		log_printf( "First object name is empty?\n" );
		goto fail;
	}

	if ( util_strncmp( "version", 7, root.aObjects.data[ 0 ].aName.data, root.aObjects.data[ 0 ].aName.size ) )
	{
		json_object_t& version = root.aObjects.data[ 0 ];
		if ( version.aType != e_json_type_int )
		{
			log_printf( "Version entry is not an integer type!\n" );
			goto fail;
		}
		else if ( version.aInt < CLIP_VIDEO_FORMAT_VER_MIN )
		{
			log_printf( "File is older video format version (got version %d, expected min version %d)\n", version.aInt, CLIP_VIDEO_FORMAT_VER_MIN );
			goto fail;
		}
		else if ( version.aInt > CLIP_VIDEO_FORMAT_VER )
		{
			log_printf( "File is too new of a video format version (got version %d, expected version %d)\n", version.aInt, CLIP_VIDEO_FORMAT_VER );
			goto fail;
		}

		clip_data::version = version.aInt;
	}
	else
	{
		log_printf( "First entry is not \"version\"\n" );
		goto fail;
	}

	if ( !util_strncmp( "videos", 6, root.aObjects.data[ 1 ].aName.data, root.aObjects.data[ 1 ].aName.size ) )
	{
		log_printf( "Second entry is not \"videos\"\n" );
		goto fail;
	}

	videos_root = &root.aObjects.data[ 1 ];

	// allocate all the output videos now
	new_data    = ch_realloc< clip_output_video_t >( clip_data::output, videos_root->aObjects.count );

	if ( !new_data )
		return false;

	memset( new_data, 0, sizeof( clip_output_video_t ) * videos_root->aObjects.count );

	clip_data::output       = new_data;
	clip_data::output_count = videos_root->aObjects.count;

	for ( size_t root_i = 0; root_i < videos_root->aObjects.count; root_i++ )
	{
		json_object_t& object = videos_root->aObjects.data[ root_i ];
		clip_parse_video( object, root_i );
	}

	json_free( root );
	free( file );

	// scan videos
	clip_check_videos();

	return true;

fail:
	json_free( root );
	free( file );
	return false;
}


void clip_get_video_metadata( clip_source_t& source )
{
	std::string path = source.path;
	auto        it   = g_video_metadata_map.find( path );

	if ( it != g_video_metadata_map.end() )
	{
		source.metadata = it->second;
	}

	get_video_metadata( source.path, source.metadata );
	g_video_metadata_map[ path ] = source.metadata;
}


void clip_check_video( clip_output_video_t& output )
{
	if ( output.name == nullptr )
	{
		output.state = e_output_state_invalid;
		return;
	}

	if ( output.source_count == 0 || output.source == nullptr )
	{
		output.state = e_output_state_invalid;
		return;
	}

	if ( output.groups.empty() )
	{
		output.state = e_output_state_invalid;
		return;
	}


#if 0
	// ----------------------------------------------------------------------------------------
	// determine encode presets for this output video

	extern bool used_in_preset( clip_encode_settings_t & override, u32 preset_i );

	// figure out what encode presets this runs on
	for ( u32 in_i = 0; in_i < output.source_count; in_i++ )
	{
		clip_source_t& source = output.source[ in_i ];

		for ( u32 preset_i = 0; preset_i < source.encode_settings.presets_count; preset_i++ )
		{
			bool preset_already_added = false;
			for ( u32 search_i = 0; search_i < output.presets_count; search_i++ )
			{
				if ( output.presets[ search_i ] == source.encode_settings.presets[ preset_i ] )
				{
					preset_already_added = true;
					break;
				}
			}

			if ( preset_already_added )
				continue;

			// add it to this list
			u32* new_data = ch_realloc< u32 >( output.presets, output.presets_count + 1 );

			if ( !new_data )
			{
				log_printf( "failed to allocate data for storing output video presets\n" );
				return;
			}

			output.presets                           = new_data;
			output.presets[ output.presets_count++ ] = source.encode_settings.presets[ preset_i ];
		}
	}

	// ----------------------------------------------------------------------------------------
	// get source video metadata

	bool all_valid = true;

	// find all unique source videos, there will be duplicates for different encode presets
	// this way we don't need get metadata for the same video multiple times

	for ( u32 in_i = 0; in_i < output.source_count; in_i++ )
	{
		clip_source_t& source = output.source[ in_i ];

		if ( source.file_missing || !fs_exists( source.path ) )
		{
			all_valid          = false;
			source.file_missing = true;
			break;
		}
	}

	// ----------------------------------------------------------------------------------------
	// print data for each encode preset

	for ( u32 preset_i = 0; preset_i < output.presets_count; preset_i++ )
	{
		clip_encode_preset_t& preset = g_clip_clip_data::preset[ output.presets[ preset_i ] ];

		float duration         = 0.f;
		bool  duration_invalid = false;
		for ( u32 in_i = 0; in_i < output.source_count; in_i++ )
		{
			clip_source_t& source = output.source[ in_i ];

			if ( !used_in_preset( source.encode_settings, preset_i ) )
				continue;

			for ( u32 time_i = 0; time_i < source.time_range_count; time_i++ )
			{
				if ( !valid_time_range( source.time_range[ time_i ], source.metadata ) )
					duration_invalid = true;

				duration += source.time_range[ time_i ].end - source.time_range[ time_i ].start;
			}
		}

		log_printf( "    Duration: %.4f%s\n\n", duration, duration_invalid ? " [INVALID]" : "" );

		// print source videos for this preset and their time ranges
		for ( u32 in_i = 0; in_i < output.source_count; in_i++ )
		{
			clip_source_t& source = output.source[ in_i ];

			// validate preset
			if ( !used_in_preset( source.encode_settings, preset_i ) )
				continue;

			log_printf( "    %s%s\n", source.path, source.file_missing ? " [INVALID]" : "" );

			if ( source.file_missing )
				continue;

			for ( u32 time_i = 0; time_i < source.time_range_count; time_i++ )
			{
				bool  valid          = valid_time_range( source.time_range[ time_i ], source.metadata );
				float range_duration = source.time_range[ time_i ].end - source.time_range[ time_i ].start;

				log_printf( "        %.4f - %.4f (%.4f)%s\n", source.time_range[ time_i ].start, source.time_range[ time_i ].end, range_duration, valid ? "" : " [INVALID]" );
			}
		}
	}

	if ( !all_valid )
		return;
#endif

	output.state = e_output_state_wait;
}


void clip_check_videos()
{
}


// ============================================================================================================================


void clip_save_settings( const char* path )
{
	// build json5

	// write to file
}


// ============================================================================================================================


bool clip_save_videos( const char* path )
{
	return false;

#if 0
	if ( !data )
		return false;

	if ( !clip_data::output_count )
		return false;

	if ( !path )
		return false;

	// build json5
	json_object_t root{};
	root.aType = e_json_type_object;

	if ( !json_add_objects( root, 2 ) )
	{
		json_free( root );
		return false;
	}

	root.aObjects.data[ 0 ].aName = json_strn( "version", 7 );
	root.aObjects.data[ 0 ].aType = e_json_type_int;
	root.aObjects.data[ 0 ].aInt  = CLIP_VIDEO_FORMAT_VER;

	if ( !json_add_array( root.aObjects.data[ 1 ], clip_data::output_count ) )
	{
		json_free( root );
		return false;
	}

	json_object_t& video_root = root.aObjects.data[ 1 ];
	video_root.aName          = json_strn( "videos", 6 );

	for ( size_t root_i = 0; root_i < video_root.aObjects.count; root_i++ )
	{
		json_object_t&       output_json = video_root.aObjects.data[ root_i ];
		clip_output_video_t& output      = clip_data::output[ root_i ];
		output_json.aType                = e_json_type_object;

		// output video has 4 objects
		output_json.aObjects.count       = 4;
		output_json.aObjects.data        = ch_malloc< json_object_t >( 4 );

		if ( !output_json.aObjects.data )
		{
			json_free( root );
			return false;
		}

		// name
		output_json.aObjects.data[ 0 ].aName   = json_strn( "name", 4 );
		output_json.aObjects.data[ 0 ].aType   = e_json_type_string;
		output_json.aObjects.data[ 0 ].aString = json_str( output.name );

		// prefix
		output_json.aObjects.data[ 1 ].aName   = json_strn( "prefix", 6 );
		output_json.aObjects.data[ 1 ].aType   = e_json_type_string;
		output_json.aObjects.data[ 1 ].aString = json_str( clip_data::prefix[ output.prefix ].name );

		// encode_overrides
	//	if ( !clip_save_encode_override( data, output.encode_overrides, output_json.aObjects.data[ 2 ] ) )
	//	{
	//		json_free( root );
	//		return;
	//	}

		// inputs
		output_json.aObjects.data[ 3 ].aName          = json_strn( "inputs", 6 );
		output_json.aObjects.data[ 3 ].aType          = e_json_type_array;
		output_json.aObjects.data[ 3 ].aObjects.count = output.source_count;
		output_json.aObjects.data[ 3 ].aObjects.data  = ch_malloc< json_object_t >( output.source_count );

		if ( output.source_count )
		{
			if ( !output_json.aObjects.data[ 3 ].aObjects.data )
			{
				json_free( root );
				return false;
			}

			for ( u32 input_i = 0; input_i < output.source_count; input_i++ )
			{
				json_object_t&      input_json = output_json.aObjects.data[ 3 ].aObjects.data[ input_i ];
				clip_source_t& source      = output.source[ input_i ];

				if ( !json_add_objects( input_json, 3 ) )
				{
					json_free( root );
					return false;
				}

				// path
				input_json.aObjects.data[ 0 ].aName   = json_strn( "path", 4 );
				input_json.aObjects.data[ 0 ].aType   = e_json_type_string;
				input_json.aObjects.data[ 0 ].aString = json_str( source.path );

				// encode_overrides
				if ( !clip_save_encode_override( data, source.encode_settings, input_json.aObjects.data[ 1 ] ) )
				{
					json_free( root );
					return false;
				}

				// time_ranges
				input_json.aObjects.data[ 2 ].aName = json_strn( "time_ranges", 11 );

				if ( !json_add_array( input_json.aObjects.data[ 2 ], source.time_range_count ) )
				{
					json_free( root );
					return false;
				}

				for ( u32 time_i = 0; time_i < source.time_range_count; time_i++ )
				{
					json_object_t& json_time = input_json.aObjects.data[ 2 ].aObjects.data[ time_i ];

					if ( !json_add_array( json_time, 2 ) )
					{
						json_free( root );
						return false;
					}

					// json_time.aObjects.data[ 0 ].aName   = json_strn( "start", 5 );
					json_time.aObjects.data[ 0 ].aType   = e_json_type_double;
					json_time.aObjects.data[ 0 ].aDouble = source.time_range[ time_i ].start;

					// json_time.aObjects.data[ 1 ].aName   = json_strn( "end", 3 );
					json_time.aObjects.data[ 1 ].aType   = e_json_type_double;
					json_time.aObjects.data[ 1 ].aDouble = source.time_range[ time_i ].end;
				}
			}
		}
	}

	// write to file
	json_str_t out_str = json_to_str( root );

	json_free( root );

	if ( !out_str.data )
	{
		log_printf( "failed to convert json to string to write to file!\n" );
		return false;
	}

	bool write_ret = fs_save_file( path, out_str.data, out_str.size - 1 );

	free( out_str.data );

	if ( !write_ret )
	{
		log_printf( "Failed to save videos to \"%s\"\n", path );
		return false;
	}

	log_printf( "Saved videos to \"%s\"\n", path );
	return true;
#endif
}

