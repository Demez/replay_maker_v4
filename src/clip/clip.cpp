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

	// for now, create a default encode preset and a default prefix preset
	clip_encode_preset_t* encode = clip_add_encode_preset( data, "test", "mkv" );

	encode->ffmpeg_cmd           = strdup( "-map 0 -c:a libopus -b:a 160k -c:v libx264 -crf 24 -preset veryfast -pix_fmt yuv444p -colorspace bt709 -color_primaries bt709 -color_trc bt709 -threads 16" );
	encode->out_folder_append    = strdup( "test" );
	encode->out_prefix           = strdup( "test_" );

	clip_add_prefix( data, "cool", "coolgame_d_" );

	return data;
}


void clip_free( clip_data_t* data )
{
	if ( data == nullptr )
		return;
}


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


void clip_add_search_path( clip_data_t* data, const char* path )
{
	if ( !data )
		return;

	char** new_data = ch_realloc< char* >( data->search_path, data->search_path_count + 1 );

	if ( !new_data )
		return;

	data->search_path                            = new_data;
	data->search_path[ data->search_path_count ] = ch_malloc< char >( strlen( path ) + 1 );

	strcpy( data->search_path[ data->search_path_count ], path );
	data->search_path[ data->search_path_count ][ strlen( path ) ] = 0;

	data->search_path_count++;
}


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


clip_input_video_t* clip_add_input( clip_output_video_t* output, const char* path )
{
	if ( !output )
		return nullptr;

	clip_input_video_t* new_data = ch_realloc< clip_input_video_t >( output->input, output->input_count + 1 );

	if ( !new_data )
		return nullptr;

	output->input = new_data;
	memset( &output->input[ output->input_count ], 0, sizeof( clip_input_video_t ) );

	clip_input_video_t* input = &output->input[ output->input_count ];
	input->path               = ch_malloc< char >( strlen( path ) + 1 );

	strcpy( input->path, path );
	input->path[ strlen( path ) ] = 0;

	output->input_count++;

	return input;
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

