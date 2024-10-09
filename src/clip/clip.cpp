#include "clip.h"


// --------------------------------------------------------------------------------------------------------


template< typename T >
static bool array_append( T*& data, u32 count )
{
	T* new_data = ch_realloc< T >( data, count + 1 );

	if ( !new_data )
		return true;

	data = new_data;
	memset( &data[ count ], 0, sizeof( T ) );

	return false;
}


// --------------------------------------------------------------------------------------------------------


clip_data_t* clip_create()
{
	clip_data_t* data = ch_calloc< clip_data_t >( 1 );

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
		printf( "encode_presets is not an array!\n" );
		return;
	}

	for ( size_t root_i = 0; root_i < json.aObjects.count; root_i++ )
	{
		json_object_t& object = json.aObjects.data[ root_i ];

		if ( object.aType != e_json_type_object )
		{
			printf( "encode preset entry is not an object!\n" );
			continue;
		}

		clip_encode_preset_t* encode = clip_create_encode_preset( data );

		if ( !encode )
		{
			printf( "failed to create encode preset?\n" );
			return;
		}

		for ( size_t object_i = 0; object_i < object.aObjects.count; object_i++ )
		{
			json_object_t& field = object.aObjects.data[ object_i ];

			if ( field.aType == e_json_type_string )
			{
				if ( util_strncmp( "name", 4, field.aName.data, field.aName.size ) )
				{
					memcpy( encode->name, field.aString.data, MAX( MAX_LEN_PRESET_NAME, field.aString.size ) * sizeof( char ) );
				}
				else if ( util_strncmp( "ext", 3, field.aName.data, field.aName.size ) )
				{
					memcpy( encode->ext, field.aString.data, MAX( MAX_LEN_EXT, field.aString.size ) * sizeof( char ) );
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
					printf( "unknown encode preset string field: \"%s\"\n", field.aName.data );
				}
			}
			else if ( field.aType == e_json_type_int )
			{
				if ( util_strncmp( "target_size", 11, field.aName.data, field.aName.size ) )
				{
					encode->target_size = field.aInt;
				}
				else if ( util_strncmp( "audio_bitrate", 13, field.aName.data, field.aName.size ) )
				{
					encode->audio_bitrate = field.aInt;
				}
				else
				{
					printf( "unknown encode preset int field: \"%s\"\n", field.aName.data );
				}
			}
			else
			{
				printf( "unknown encode preset field: \"%s\"\n", field.aName.data );
			}
		}
	}

	printf( "cool\n" );
}


static void clip_parse_prefixes( clip_data_t* data, json_object_t& json )
{
	if ( json.aType != e_json_type_array )
	{
		printf( "encode_presets is not an array!\n" );
		return;
	}

	for ( size_t root_i = 0; root_i < json.aObjects.count; root_i++ )
	{
		json_object_t& object = json.aObjects.data[ root_i ];

		if ( object.aType != e_json_type_object )
		{
			printf( "encode preset entry is not an object!\n" );
			continue;
		}

		if ( object.aObjects.count > 1 )
		{
			printf( "extra data in preset entry?\n" );
			continue;
		}

		json_object_t& field = object.aObjects.data[ 0 ];

		if ( field.aType != e_json_type_string )
		{
			printf( "invalid encode preset field type - expected string: \"%s\"\n", field.aName.data );
			continue;
		}

		clip_prefix_t* prefix = clip_create_prefix( data );

		if ( !prefix )
		{
			printf( "failed to create prefix?\n" );
			return;
		}

		memcpy( prefix->name, field.aName.data, MAX( MAX_LEN_PRESET_NAME, field.aName.size ) * sizeof( char ) );
		prefix->prefix = strdup( field.aString.data );
	}
}


void clip_parse_settings( clip_data_t* data, const char* path )
{
	if ( !data )
		return;

	char* file = fs_readfile( path );

	if ( !file )
	{
		printf( "failed to read file for settings\n" );
		return;
	}

	json_object_t root{};
	EJsonError    err = json_parse( &root, file );

	if ( err != EJsonError_None )
	{
		printf( "Error Parsing Json - %s\n", json_error_to_str( err ) );
		return;
	}

	if ( root.aType != e_json_type_object )
	{
		printf( "%s - Root Type is not an Object\n", path );
		return;
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

	json_free( &root );
	free( file );

	printf( "parsed settings\n" );
}


// ============================================================================================================================


void clip_parse_encode_override( clip_data_t* data, clip_encode_override_t& override, json_object_t& root )
{
	if ( root.aType != e_json_type_object )
	{
		printf( "expected encode_override to be an object!\n" );
		return;
	}

	for ( size_t root_i = 0; root_i < root.aObjects.count; root_i++ )
	{
		json_object_t& object = root.aObjects.data[ root_i ];

		if ( util_strncmp( "presets", 7, object.aName.data, object.aName.size ) )
		{
			if ( object.aType != e_json_type_array )
			{
				printf( "Expected array of strings for presets value in encode_overrides\n" );
				continue;
			}

			u32* preset_array = ch_calloc< u32 >( object.aObjects.count );

			if ( !preset_array )
				continue;

			override.presets = preset_array;

			for ( size_t preset_name_i = 0; preset_name_i < object.aObjects.count; preset_name_i++ )
			{
				json_object_t& json_preset = object.aObjects.data[ preset_name_i ];

				// look for the preset
				for ( u32 preset_i = 0; preset_i < data->preset_count; preset_i++ )
				{
					if ( !util_strncmp( json_preset.aString.data, json_preset.aString.size, data->preset[ preset_i ].name, strlen( data->preset[ preset_i ].name ) ) )
						continue;

					override.presets[ override.presets_count++ ] = preset_i;
					break;
				}
			}
		}
		else if ( util_strncmp( "exclude_mode", 7, object.aName.data, object.aName.size ) )
		{
			override.preset_exclude = object.aType == e_json_type_true ? true : false;
		}
	}
}


bool clip_parse_input( clip_data_t* data, clip_output_video_t& output, json_object_t& root, u32 input_i )
{
	if ( root.aType != e_json_type_object )
	{
		printf( "expected encode_override to be an object!\n" );
		return false;
	}

	clip_input_video_t& input = output.input[ input_i ];

	for ( size_t root_i = 0; root_i < root.aObjects.count; root_i++ )
	{
		json_object_t& object = root.aObjects.data[ root_i ];

		if ( util_strncmp( "path", 4, object.aName.data, object.aName.size ) )
		{
			input.path = strdup( object.aString.data );
		}
		else if ( util_strncmp( "encode_overrides", 16, object.aName.data, object.aName.size ) )
		{
			clip_parse_encode_override( data, input.encode_overrides, object );
		}
		else if ( util_strncmp( "time_ranges", 11, object.aName.data, object.aName.size ) )
		{
			if ( object.aType != e_json_type_array )
			{
				printf( "expected time_ranges to be an array!\n" );
				continue;
			}

			clip_time_range_t* time_range_array = ch_calloc< clip_time_range_t >( object.aObjects.count );

			if ( !time_range_array )
				continue;

			input.time_range = time_range_array;

			for ( size_t range_i = 0; range_i < object.aObjects.count; range_i++ )
			{
				json_object_t& range_json = object.aObjects.data[ range_i ];

				if ( range_json.aObjects.count != 2 )
				{
					printf( "time_ranges entry does not have only 2 entires\n" );
					continue;
				}

				clip_time_range_t& time_range = input.time_range[ input.time_range_count++ ];
				time_range.start              = range_json.aObjects.data[ 0 ].aDouble;
				time_range.end                = range_json.aObjects.data[ 1 ].aDouble;
			}
		}
		else
		{
			printf( "unknown input video property: \"%s\"\n", object.aName.data );
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
		else if ( util_strncmp( "encode_overrides", 16, object.aName.data, object.aName.size ) )
		{
			clip_parse_encode_override( data, output.encode_overrides, object );
		}
		else if ( util_strncmp( "inputs", 6, object.aName.data, object.aName.size ) )
		{
			if ( object.aType != e_json_type_array )
			{
				printf( "Expected array of objects for input videos\n" );
				continue;
			}

			clip_input_video_t* video_array = ch_calloc< clip_input_video_t >( object.aObjects.count );

			if ( !video_array )
				continue;

			output.input = video_array;

			for ( size_t video_i = 0; video_i < object.aObjects.count; video_i++ )
			{
				json_object_t& json_video = object.aObjects.data[ video_i ];
				if ( clip_parse_input( data, output, json_video, video_i ) )
					output.input_count++;
			}
		}
		else
		{
			printf( "unknown video property: \"%s\"\n", object.aName.data );
		}
	}
}


void clip_parse_videos( clip_data_t* data, const char* path )
{
	if ( !data )
		return;

	char* file = fs_readfile( path );

	if ( !file )
	{
		printf( "failed to read file for videos\n" );
		return;
	}

	json_object_t root{};
	EJsonError    err = json_parse( &root, file );

	if ( err != EJsonError_None )
	{
		printf( "Error Parsing Json - %s\n", json_error_to_str( err ) );
		return;
	}

	if ( root.aType != e_json_type_array )
	{
		printf( "%s - Root Type is not an Array\n", path );
		return;
	}

	// allocate all the output videos now
	clip_output_video_t* new_data = ch_realloc< clip_output_video_t >( data->output, root.aObjects.count );

	if ( !new_data )
		return;

	memset( new_data, 0, sizeof( clip_output_video_t ) * root.aObjects.count );

	data->output       = new_data;
	data->output_count = root.aObjects.count;

	for ( size_t root_i = 0; root_i < root.aObjects.count; root_i++ )
	{
		json_object_t& object = root.aObjects.data[ root_i ];
		clip_parse_video( data, object, root_i );
	}

	json_free( &root );
	free( file );

	printf( "parsed settings\n" );
}


void clip_save_videos( clip_data_t* data, const char* path )
{
	// build json5

	// write to file
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
	memcpy( data->prefix[ data->prefix_count ].name, name, std::max( name_len, MAX_LEN_PRESET_NAME ) );

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
	memcpy( data->preset[ data->preset_count ].name, name, std::max( name_len, MAX_LEN_PRESET_NAME ) );

	size_t ext_len = strlen( name );
	memcpy( data->preset[ data->preset_count ].ext, ext, std::max( ext_len, MAX_LEN_EXT ) );

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


clip_output_video_t* clip_create_output( clip_data_t* data )
{
	if ( !data )
		return nullptr;

	if ( array_append( data->output, data->output_count ) )
		return nullptr;
}


clip_input_video_t*  clip_create_input( clip_output_video_t* output )
{
	if ( !output )
		return nullptr;

	return nullptr;
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

	clip_input_video_t* new_data = ch_realloc< clip_input_video_t >( output->input, output->input_count + 1 );

	if ( !new_data )
		return UINT32_MAX;

	output->input = new_data;
	memset( &output->input[ output->input_count ], 0, sizeof( clip_input_video_t ) );

	clip_input_video_t* input = &output->input[ output->input_count ];
	input->path               = ch_malloc< char >( strlen( path ) + 1 );

	strcpy( input->path, path );
	input->path[ strlen( path ) ] = 0;

	return output->input_count++;
}


u32 clip_duplicate_input( clip_output_video_t* output, u32 input_i )
{
	if ( !output )
		return UINT32_MAX;

	if ( input_i >= output->input_count )
	{
		printf( "invalid input index\n" );
		return UINT32_MAX;
	}

	clip_input_video_t& input_src   = output->input[ input_i ];

	u32 input_dst_i = clip_add_input( output, input_src.path );

	if ( input_dst_i == UINT32_MAX )
		return UINT32_MAX;

	clip_input_video_t& input_dst = output->input[ input_dst_i ];

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

	if ( input_src.encode_overrides.presets )
	{
		input_dst.encode_overrides.presets_count = input_src.encode_overrides.presets_count;
		input_dst.encode_overrides.presets       = ch_malloc< u32 >( input_src.encode_overrides.presets_count );

		memcpy( input_dst.encode_overrides.presets, input_src.encode_overrides.presets, sizeof( u32 ) * input_src.encode_overrides.presets_count );
	}

	input_dst.encode_overrides.preset_exclude = input_src.encode_overrides.preset_exclude;

	return input_dst_i;
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
		printf( "invalid output\n" );
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
		printf( "invalid output index\n" );
		return;
	}

	clip_output_video_t& output = data->output[ output_i ];

	// remove input videos
	for ( u32 i = 0; i < output.input_count; i++ )
	{
		free( output.input[ i ].path );
		free( output.input[ i ].time_range );
		free( output.input[ i ].encode_overrides.presets );
	}

	free( output.input );
	free( output.encode_overrides.presets );

	util_array_remove_element( data->output, data->output_count, output_i );
}


void clip_remove_input( clip_output_video_t* output, u32 input_i )
{
	if ( !output )
		return;

	if ( input_i > output->input_count )
	{
		printf( "invalid input index\n" );
		return;
	}

	clip_input_video_t& input = output->input[ input_i ];
	
	free( input.path );
	free( input.time_range );
	free( input.encode_overrides.presets );

	util_array_remove_element( output->input, output->input_count, input_i );
}


void clip_add_time_range( clip_output_video_t* output, u32 input_i, float start_time, float end_time )
{
	if ( !output )
		return;

	if ( input_i > output->input_count )
	{
		printf( "invalid input index\n" );
		return;
	}

	clip_input_video_t& input = output->input[ input_i ];

	// What if you made time ranges linked lists? maybe the same with input videos? would allow for easy re-ordering
	clip_time_range_t* new_data = ch_realloc< clip_time_range_t >( input.time_range, input.time_range_count + 1 );

	if ( !new_data )
		return;

	input.time_range = new_data;
	memset( &input.time_range[ input.time_range_count ], 0, sizeof( clip_time_range_t ) );

	input.time_range[ input.time_range_count ].start = start_time;
	input.time_range[ input.time_range_count ].end   = end_time;

	input.time_range_count++;
}


void clip_remove_time_range( clip_output_video_t* output, u32 input_i, u32 time_range )
{
	if ( !output )
		return;

	if ( input_i > output->input_count )
	{
		printf( "invalid input index\n" );
		return;
	}

	clip_input_video_t& input = output->input[ input_i ];
	util_array_remove_element( input.time_range, input.time_range_count, time_range );
}


void clip_add_preset_to_encode_override( clip_data_t* data, clip_encode_override_t& override, u32 preset_index )
{
	if ( !data )
		return;

	if ( array_append( override.presets, override.presets_count ) )
		return;

	override.presets[ override.presets_count++ ] = preset_index;
}


void clip_add_preset_to_encode_override( clip_data_t* data, clip_encode_override_t& override, const char* preset_name )
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

	printf( "Failed to find preset: %s\n", preset_name );
}

