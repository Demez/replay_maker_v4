#include "clip.h"
#include "logging.h"
#include "main.h"


std::unordered_map< std::string, video_metadata_t > g_video_metadata_map;


// --------------------------------------------------------------------------------------------------------


clip_data_t* clip_create()
{
	clip_data_t* data = ch_calloc< clip_data_t >( 1 );
	data->version     = CLIP_VIDEO_FORMAT_VER;

#if 0
	// for now, create a default encode preset and a default prefix preset
	clip_encode_preset_t* encode = clip_add_encode_preset( data, "test", "mkv" );

	encode->ffmpeg_cmd           = strdup( "-map 0 -c:a libopus -b:a 160k -c:v libx264 -crf 24 -preset veryfast -pix_fmt yuv444p -colorspace bt709 -color_primaries bt709 -color_trc bt709 -threads 16" );
	encode->out_folder_append    = strdup( "test" );
	encode->out_prefix           = strdup( "test_" );

	// for now, create a default encode preset and a default prefix preset
	clip_encode_preset_t* encode2 = clip_add_encode_preset( data, "test_webm", "webm" );

	encode2->ffmpeg_cmd           = strdup( "-map 0 -c:a libopus -b:a 160k -c:v libx264 -crf 24 -preset veryfast -pix_fmt yuv444p -colorspace bt709 -color_primaries bt709 -color_trc bt709 -threads 16" );
	encode2->out_folder_append    = strdup( "test2" );
	encode2->out_prefix           = strdup( "test2_" );

	// for now, create a default encode preset and a default prefix preset
	clip_encode_preset_t* encode3 = clip_add_encode_preset( data, "test3_webm", "webm" );

	encode3->ffmpeg_cmd           = strdup( "-map 0 -c:a libopus -b:a 160k -c:v libx264 -crf 24 -preset veryfast -pix_fmt yuv444p -colorspace bt709 -color_primaries bt709 -color_trc bt709 -threads 16" );
	encode3->out_folder_append    = strdup( "test2" );
	encode3->out_prefix           = strdup( "test2_" );

	clip_add_prefix( data, "General", "general_d_" );
	clip_add_prefix( data, "VRChat", "vrc_d_" );
	clip_add_prefix( data, "Left 4 Dead 2", "l4d2_d_" );
	clip_add_prefix( data, "Deep Rock Galactic", "drg_d_" );
#endif

	return data;
}


void clip_free( clip_data_t* data )
{
	if ( data == nullptr )
		return;
}


// ============================================================================================================================


static void clip_parse_encode_presets( clip_data_t* data, json_object_t& json )
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

		clip_encode_preset_t* encode = clip_create_encode_preset( data );

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


static void clip_parse_prefixes( clip_data_t* data, json_object_t& json )
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

		clip_prefix_t* prefix = clip_create_prefix( data );

		if ( !prefix )
		{
			log_printf( "failed to create prefix?\n" );
			return;
		}

		memcpy( prefix->name, field.aName.data, MIN( MAX_LEN_PRESET_NAME, field.aName.size ) * sizeof( char ) );
		prefix->prefix = strdup( field.aString.data );
	}
}


bool clip_parse_settings( clip_data_t* data, const char* path )
{
	if ( !data )
		return false;

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
			clip_parse_encode_presets( data, object );
		}
		else if ( util_strncmp( "prefixes", 8, object.aName.data, object.aName.size ) )
		{
			clip_parse_prefixes( data, object );
		}
	}

	json_free( root );
	free( file );

	return true;
}


// ============================================================================================================================


void clip_parse_encode_override( clip_data_t* data, clip_output_video_t& output, json_object_t& root, u32 source_i )
{
	if ( root.aType != e_json_type_object )
	{
		log_printf( "expected encode_override to be an object!\n" );
		return;
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
				for ( u32 preset_i = 0; preset_i < data->preset_count; preset_i++ )
				{
					if ( !util_strncmp( json_preset.aString.data, json_preset.aString.size, data->preset[ preset_i ].name, strlen( data->preset[ preset_i ].name ) ) )
						continue;

					// check if we have a preset with this already
					bool found_preset = false;
					for ( u32 preset_use_i = 0; preset_use_i < output.presets.size(); preset_use_i++ )
					{
						clip_preset_output_t& preset_use = output.presets[ preset_use_i ];
						if ( preset_use.preset == preset_i )
						{
							found_preset      = true;
							bool found_source = false;

							// look for this source
							for ( clip_source_usage_t& source_use : preset_use.sources )
							{
								if ( source_use.source_index == source_i )
								{
									found_source = true;
									break;
								}
							}

							if ( !found_source )
							{
								clip_source_usage_t& source_use = preset_use.sources.emplace_back();
								source_use.source_index         = source_i;
							}

							break;
						}
					}

					// create a new one and add it
					if ( !found_preset )
					{
						// if ( array_append( output.preset, output.preset_count ) )
						// {
						// }

						clip_preset_output_t& preset_use = output.presets.emplace_back();
						preset_use.preset                = preset_i;

						clip_source_usage_t& source_use  = preset_use.sources.emplace_back();
						source_use.source_index          = source_i;
					}

					break;
				}
			}
		}
	}
}


// version 3 parsing
bool clip_parse_input_v3( clip_data_t* data, clip_output_video_t& output, json_object_t& root, u32 input_i )
{
	if ( root.aType != e_json_type_object )
	{
		log_printf( "expected video source to be an object!\n" );
		return false;
	}

	clip_source_t& source = output.source[ input_i ];

	for ( size_t root_i = 0; root_i < root.aObjects.count; root_i++ )
	{
		json_object_t& object = root.aObjects.data[ root_i ];

		if ( util_strncmp( "path", 4, object.aName.data, object.aName.size ) )
		{
			source.path = strdup( object.aString.data );
		}
		else if ( util_strncmp( "encode_overrides", 16, object.aName.data, object.aName.size ) )
		{
			clip_parse_encode_override( data, output, object, input_i );
		}
		else if ( util_strncmp( "time_ranges", 11, object.aName.data, object.aName.size ) )
		{
			#if 0
			if ( object.aType != e_json_type_array )
			{
				log_printf( "expected time_ranges to be an array!\n" );
				continue;
			}

			clip_time_range_t* time_range_array = ch_calloc< clip_time_range_t >( object.aObjects.count );

			if ( !time_range_array )
				continue;

			source.time_range = time_range_array;

			for ( size_t range_i = 0; range_i < object.aObjects.count; range_i++ )
			{
				json_object_t& range_json = object.aObjects.data[ range_i ];
				
				if ( range_json.aType == e_json_type_object )
				{
					clip_time_range_t& time_range = source.time_range[ source.time_range_count++ ];

					for ( size_t time_i = 0; time_i < range_json.aObjects.count; time_i++ )
					{
						json_object_t& time_entry = range_json.aObjects.data[ time_i ];

						if ( util_strncmp( time_entry.aName.data, time_entry.aName.size, "encode_overrides", 16 ) )
						{
							log_printf( log_error, "ENCODE OVERRIDES IN TIME RANGE - OBSOLETE\n" );
							// clip_parse_encode_override( data, time_range.encode_settings, time_entry );
						}
						else if ( util_strncmp( time_entry.aName.data, time_entry.aName.size, "start", 5 ) )
						{
							time_range.start = time_entry.aDouble;
						}
						else if ( util_strncmp( time_entry.aName.data, time_entry.aName.size, "end", 3 ) )
						{
							time_range.end = time_entry.aDouble;
						}
					}
				}
				else if ( range_json.aType == e_json_type_array )
				{
					if ( range_json.aObjects.count != 2 )
					{
						log_printf( "time_ranges entry does not have only 2 entires\n" );
						continue;
					}

					clip_time_range_t& time_range = source.time_range[ source.time_range_count++ ];
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

	clip_get_video_metadata( source );

	return true;
}


bool clip_parse_output_preset( clip_data_t* data, clip_output_video_t& output, json_object_t& root, clip_preset_output_t& preset_use )
{
	if ( root.aType != e_json_type_object )
	{
		log_printf( "expected video source to be an object!\n" );
		return false;
	}

	for ( size_t root_i = 0; root_i < root.aObjects.count; root_i++ )
	{
		json_object_t& object = root.aObjects.data[ root_i ];

		if ( util_strncmp( "preset", 6, object.aName.data, object.aName.size ) )
		{
			// look for the preset
			bool found = false;
			for ( u32 preset_i = 0; preset_i < data->preset_count; preset_i++ )
			{
				if ( !util_strncmp( object.aString.data, object.aString.size, data->preset[ preset_i ].name, strlen( data->preset[ preset_i ].name ) ) )
					continue;

				preset_use.preset = preset_i;
				found             = true;
				break;
			}

			if ( !found )
			{
				printf( "Failed to find valid encode preset for video: %s\n", object.aString.data );
				return false;
			}
		}
		else if ( util_strncmp( "ffmpeg_cmd", 10, object.aName.data, object.aName.size ) )
		{
			// soon.....
		}
		else if ( util_strncmp( "sources", 7, object.aName.data, object.aName.size ) )
		{
			if ( object.aType != e_json_type_array )
			{
				log_printf( "expected presets/sources to be an array!\n" );
				continue;
			}

			preset_use.sources.resize( object.aObjects.count );

			for ( size_t source_use_i = 0; source_use_i < object.aObjects.count; source_use_i++ )
			{
				json_object_t&       obj_json = object.aObjects.data[ source_use_i ];
				clip_source_usage_t& source_use  = preset_use.sources[ source_use_i ];

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


void clip_parse_video( clip_data_t* data, json_object_t& root, u32 output_i )
{
	clip_output_video_t& output = data->output[ output_i ];

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
			for ( u32 prefix_i = 0; prefix_i < data->prefix_count; prefix_i++ )
			{
				if ( !util_strncmp( object.aString.data, object.aString.size, data->prefix[ prefix_i ].name, strlen( data->prefix[ prefix_i ].name ) ) )
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

			clip_source_t* video_array = ch_calloc< clip_source_t >( object.aObjects.count );

			if ( !video_array )
				continue;

			output.source = video_array;

			for ( size_t video_i = 0; video_i < object.aObjects.count; video_i++ )
			{
				json_object_t& json_video = object.aObjects.data[ video_i ];
				if ( clip_parse_input_v3( data, output, json_video, video_i ) )
					output.source_count++;
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
					output.source[ video_i ].path = strdup( json_video.aString.data );
				}

				clip_get_video_metadata( output.source[ video_i ] );
			}
		}
		else if ( util_strncmp( "presets", 7, object.aName.data, object.aName.size ) )
		{
			if ( object.aType != e_json_type_array )
			{
				log_printf( "Expected array of objects for presets\n" );
				continue;
			}

			output.presets.clear();
			output.presets.resize( object.aObjects.count );

			for ( size_t preset_use_i = 0; preset_use_i < object.aObjects.count; preset_use_i++ )
			{
				json_object_t& json_preset_use = object.aObjects.data[ preset_use_i ];

				if ( json_preset_use.aType != e_json_type_object )
				{
					log_printf( log_error, "Expected object for output preset, got \"%s\"\n", json_type_to_str( json_preset_use.aType ) );
					continue;
				}

				clip_parse_output_preset( data, output, json_preset_use, output.presets[ preset_use_i ] );
			}
		}
		else
		{
			log_printf( "unknown video property: \"%s\"\n", object.aName.data );
		}
	}

	clip_check_video( data, output );
}


bool clip_parse_videos( clip_data_t* data, const char* path )
{
	if ( !data )
		return false;

	if ( data->output_count )
	{
		log_printf( "TODO: CLEAR OLD OUTPUT VIDEOS\n" );
	}

	char* file = fs_read_file( path );

	if ( !file )
	{
		log_printf( "failed to read file for videos\n" );
		return false;
	}

	clip_output_video_t* new_data = nullptr;
	json_object_t*       videos_root = nullptr;

	json_object_t root{};
	EJsonError    err = json_parse( root, file );

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

		data->version = version.aInt;
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
	new_data    = ch_realloc< clip_output_video_t >( data->output, videos_root->aObjects.count );

	if ( !new_data )
		return false;

	memset( new_data, 0, sizeof( clip_output_video_t ) * videos_root->aObjects.count );

	data->output       = new_data;
	data->output_count = videos_root->aObjects.count;

	for ( size_t root_i = 0; root_i < videos_root->aObjects.count; root_i++ )
	{
		json_object_t& object = videos_root->aObjects.data[ root_i ];
		clip_parse_video( data, object, root_i );
	}

	json_free( root );
	free( file );

	// scan videos
	clip_check_videos( data );

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


void clip_check_video( clip_data_t* data, clip_output_video_t& output )
{
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
		clip_encode_preset_t& preset = g_clip_data->preset[ output.presets[ preset_i ] ];

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


void clip_check_videos( clip_data_t* data )
{

}


// ============================================================================================================================


void clip_save_settings( clip_data_t* data, const char* path )
{
	// build json5

	// write to file
}


// ============================================================================================================================


#if 0
static bool clip_save_encode_override( clip_data_t* data, clip_encode_override_t& encode_override, json_object_t& root )
{
	root.aName          = json_strn( "encode_overrides", 16 );
	root.aType          = e_json_type_array;
	root.aObjects.count = encode_override.presets_count;
	root.aObjects.data  = ch_malloc< json_object_t >( encode_override.presets_count );

	if ( !root.aObjects.data )
		return false;

	for ( u32 i = 0; i < encode_override.presets_count; i++ )
	{
		json_object_t& entry = root.aObjects.data[ i ];
		entry.aType          = e_json_type_string;
		entry.aString        = json_str( data->preset[ encode_override.presets[ i ] ].name );
	}

	return true;
}
#else
static bool clip_save_encode_override( clip_data_t* data, clip_encode_settings_t& encode_override, json_object_t& root )
{
	root.aName          = json_strn( "encode_overrides", 16 );
	root.aType          = e_json_type_object;
	root.aObjects.count = 2;
	root.aObjects.data  = ch_malloc< json_object_t >( 2 );

	if ( !root.aObjects.data )
		return false;

	root.aObjects.data[ 0 ].aName          = json_strn( "presets", 7 );
	root.aObjects.data[ 0 ].aType          = e_json_type_array;
	root.aObjects.data[ 0 ].aObjects.count = encode_override.presets_count;
	root.aObjects.data[ 0 ].aObjects.data  = ch_malloc< json_object_t >( encode_override.presets_count );

	if ( !root.aObjects.data[ 0 ].aObjects.data )
		return false;

	for ( u32 i = 0; i < encode_override.presets_count; i++ )
	{
		json_object_t& entry = root.aObjects.data[ 0 ].aObjects.data[ i ];
		entry.aType          = e_json_type_string;
		entry.aString        = json_str( data->preset[ encode_override.presets[ i ] ].name );
	}

	root.aObjects.data[ 1 ].aName   = json_strn( "ffmpeg_cmd", 10 );
	root.aObjects.data[ 1 ].aType   = e_json_type_string;
	root.aObjects.data[ 1 ].aString = {};

	return true;
}
#endif


bool clip_save_videos( clip_data_t* data, const char* path )
{
	return false;

#if 0
	if ( !data )
		return false;

	if ( !data->output_count )
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

	if ( !json_add_array( root.aObjects.data[ 1 ], data->output_count ) )
	{
		json_free( root );
		return false;
	}

	json_object_t& video_root = root.aObjects.data[ 1 ];
	video_root.aName          = json_strn( "videos", 6 );

	for ( size_t root_i = 0; root_i < video_root.aObjects.count; root_i++ )
	{
		json_object_t&       output_json = video_root.aObjects.data[ root_i ];
		clip_output_video_t& output      = data->output[ root_i ];
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
		output_json.aObjects.data[ 1 ].aString = json_str( data->prefix[ output.prefix ].name );

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


// ============================================================================================================================


u32 clip_add_prefix( clip_data_t* data, const char* name, const char* prefix )
{
	if ( !data )
		return UINT32_MAX;

	clip_prefix_t* new_data = ch_realloc< clip_prefix_t >( data->prefix, data->prefix_count + 1 );

	if ( !new_data )
		return UINT32_MAX;

	data->prefix = new_data;
	memset( &data->prefix[ data->prefix_count ], 0, sizeof( clip_prefix_t ) );

	size_t name_len = strlen( name );
	memcpy( data->prefix[ data->prefix_count ].name, name, std::min( name_len, MAX_LEN_PRESET_NAME ) );

	data->prefix[ data->prefix_count ].prefix = strdup( prefix );

	return data->prefix_count++;
}


clip_encode_preset_t* clip_add_encode_preset( clip_data_t* data, const char* name, const char* ext )
{
	if ( !data )
		return nullptr;

	if ( array_append( data->preset, data->preset_count ) )
		return nullptr;

	size_t name_len = strlen( name );
	memcpy( data->preset[ data->preset_count ].name, name, std::min( name_len, MAX_LEN_PRESET_NAME ) );

	size_t ext_len = strlen( name );
	memcpy( data->preset[ data->preset_count ].ext, ext, std::min( ext_len, MAX_LEN_EXT ) );

	return &data->preset[ data->preset_count++ ];
}


// ========================================================================================================


clip_prefix_t* clip_create_prefix( clip_data_t* data )
{
	if ( !data )
		return nullptr;

	if ( array_append( data->prefix, data->prefix_count ) )
		return nullptr;

	return &data->prefix[ data->prefix_count++ ];
}


clip_encode_preset_t* clip_create_encode_preset( clip_data_t* data )
{
	if ( !data )
		return nullptr;

	if ( array_append( data->preset, data->preset_count ) )
		return nullptr;

	return &data->preset[ data->preset_count++ ];
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


clip_output_video_t* clip_add_output( clip_data_t* data, const char* name )
{
	if ( !data )
		return nullptr;

	clip_output_video_t* new_data = ch_realloc< clip_output_video_t >( data->output, data->output_count + 1 );

	if ( !new_data )
		return nullptr;

	data->output = new_data;
	memset( &data->output[ data->output_count ], 0, sizeof( clip_output_video_t ) );

	clip_output_video_t* output = &data->output[ data->output_count ];
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

	data->output_count++;

	return output;
}


u32 clip_add_input( clip_output_video_t* output, const char* path )
{
	if ( !output )
		return UINT32_MAX;

	clip_source_t* new_data = ch_realloc< clip_source_t >( output->source, output->source_count + 1 );

	if ( !new_data )
		return UINT32_MAX;

	output->source = new_data;
	memset( &output->source[ output->source_count ], 0, sizeof( clip_source_t ) );

	clip_source_t* source = &output->source[ output->source_count ];
	source->path               = ch_malloc< char >( strlen( path ) + 1 );

	strcpy( source->path, path );
	source->path[ strlen( path ) ] = 0;

	clip_get_video_metadata( *source );

	return output->source_count++;
}


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


void clip_remove_output( clip_data_t* data, clip_output_video_t* output )
{
	if ( !data )
		return;

	// look for the pointer
	u32 output_i = 0;
	for ( ; output_i < data->output_count; output_i++ )
	{
		if ( &data->output[ output_i ] == output )
			break;
	}

	if ( output_i == data->output_count )
	{
		log_printf( "invalid output\n" );
		return;
	}

	clip_remove_output( data, output_i );
}


void clip_remove_output( clip_data_t* data, u32 output_i )
{
	if ( !data )
		return;

	if ( output_i > data->output_count )
	{
		log_printf( "invalid output index\n" );
		return;
	}

	clip_output_video_t& output = data->output[ output_i ];

	// remove source videos
	for ( u32 i = 0; i < output.source_count; i++ )
	{
		free( output.source[ i ].path );
	}

	free( output.source );

	util_array_remove_element( data->output, data->output_count, output_i );
}


void clip_remove_input( clip_output_video_t* output, u32 input_i )
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

	util_array_remove_element( output->source, output->source_count, input_i );
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


void clip_add_preset_to_encode_override( clip_data_t* data, clip_encode_settings_t& override, u32 preset_index )
{
	if ( !data )
		return;

	if ( array_append( override.presets, override.presets_count ) )
		return;

	override.presets[ override.presets_count++ ] = preset_index;
}


void clip_add_preset_to_encode_override( clip_data_t* data, clip_encode_settings_t& override, const char* preset_name )
{
	if ( !data )
		return;

	// look for a preset with this name
	for ( u32 i = 0; i < data->preset_count; i++ )
	{
		if ( strcmp( data->preset[ i ].name, preset_name ) == 0 )
		{
			clip_add_preset_to_encode_override( data, override, i );
			return;
		}
	}

	log_printf( "Failed to find preset: %s\n", preset_name );
}


void clip_add_preset( clip_data_t* data, clip_output_video_t& output, u32 preset_index )
{
	for ( clip_preset_output_t& preset_out : output.presets )
	{
		if ( preset_out.preset == preset_index )
			break;
	}

	clip_preset_output_t& preset_out = output.presets.emplace_back();
	preset_out.preset                = preset_index;
}


void clip_remove_preset( clip_data_t* data, clip_output_video_t& output, u32 preset_index )
{
	for ( size_t i = 0; i < output.presets.size(); i++ )
	{
		if ( output.presets[ i ].preset != preset_index )
			continue;

		output.presets.remove( i );
		break;
	}
}


clip_preset_output_t* clip_get_preset_output( clip_output_video_t* output, u32 preset_index )
{
	if ( !output )
		return nullptr;

	for ( size_t i = 0; i < output->presets.size(); i++ )
	{
		if ( output->presets[ i ].preset != preset_index )
			continue;

		return &output->presets[ i ];
	}

	return nullptr;
}


void clip_move_output( clip_data_t* data, u32 output_id, u32 insert_position )
{
	if ( !data )
		return;

	if ( output_id == insert_position )
		return;

	if ( output_id >= data->output_count )
		return;

	if ( insert_position >= data->output_count )
		return;

	clip_output_video_t* temp_data = ch_calloc< clip_output_video_t >( 1 );

	if ( !temp_data )
	{
		log_printf( "Failed to allocate temp data to reorder output video\n" );
		return;
	}

	// back up this data
	memcpy( temp_data, &data->output[ output_id ], sizeof( clip_output_video_t ) );

	if ( output_id > insert_position )
	{
		// we want to move this output to an earlier spot in memor
		// shift everything between the insert position and original output position forward by 1
		u32 move_count = output_id - insert_position;
		memmove( data->output + insert_position + 1, data->output + insert_position, sizeof( clip_output_video_t ) * move_count );

		// now copy back the data
		memcpy( &data->output[ insert_position ], temp_data, sizeof( clip_output_video_t ) );
	}
	else
	{
		// we want to move this output to a further away spot in memory

		// shift everything between the insert position and original output position back by 1
		u32 move_count = insert_position - output_id;
		memmove( data->output + output_id, data->output + output_id + 1, sizeof( clip_output_video_t ) * move_count );

		// now copy back the data
		memcpy( &data->output[ insert_position ], temp_data, sizeof( clip_output_video_t ) );
	}

	//u32 move_count = data->output_count - insert_position;
	//memmove( data->output, data->output + insert_position, sizeof( clip_output_video_t ) * move_count );

	free( temp_data );
}

