#include <string.h>
#include <vector>
#include <forward_list>
#include <stdlib.h>
#include <cstdarg>

#include "clip/json5.h"


// TODO: be able to have verbose json error printing with this
// CONVAR( json_verbose, 0 );


constexpr int STRING_MALLOC_SIZE = 256;


struct JsonParseState_t
{
};


static bool Json_IsWhitespace( char c )
{
	return c < '!' || c > '~';
}


// Ends on the first character after whitespace
static EJsonError Json_SkipWhitespace( const char*& str )
{
StartOfSkip:
	while ( *str == ' ' || *str == '\t' || *str == '\n' || *str == '\r' )
	{
		str++;

		if ( *str == '\0' )
			return EJsonError_None;
	}

	// check for comment character
	if ( *str == '/' )
	{
		str++;

		// is this a multiline comment?
		if ( *str == '*' )
		{
			// read until */ character (probably slow)
			while ( strncmp( str, "*/", 2 ) != 0 )
				str++;

			if ( *str == '\0' )
				return EJsonError_None;

			// advance past */ character
			str++;

			if ( *str == '\0' )
				return EJsonError_None;

			str++;

			if ( *str == '\0' )
				return EJsonError_None;

			// continue checking whitespace
			goto StartOfSkip;
		}
		// is this a line comment?
		else if ( *str == '/' )
		{
			// read until end of line
			while ( *str != '\n' && *str != '\r' )
				str++;

			if ( *str == '\0' )
				return EJsonError_None;

			// continue checking whitespace
			goto StartOfSkip;
		}

		// must be some invalid character
		return EJsonError_InvalidCharacter;
	}

	return EJsonError_None;
}


static bool Json_IsControlCharacter( char c )
{
	// return c == '"' || c == '\'' || c == '[' || c == ']' || c == '{' || c == '}' || c == ':' || c == ',';
	return c == ':' || c == ',';
}


static bool Json_IsNumber( char c )
{
	return c >= '0' && c <= '9';
}


static bool Json_ValidQuotelessKey( char c )
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '$';
}


static EJsonError json_parse_quote( const char*& str, char endChar, json_str_t& output )
{
	str++;
	output.data = (char*)malloc( STRING_MALLOC_SIZE ); // preallocate chars
	output.size = 0;

	if ( output.data == nullptr )
		return EJsonError_OutOfMemory;

	memset( output.data, 0, STRING_MALLOC_SIZE );

	// const char* base = str;
	int i = 0;
	for ( int j = 1 ; *str != endChar; i++, str++ )
	{
		// can't have new line in it
		if ( *str == '\n' || *str == '\r' )
			return EJsonError_NewLineInQuote;

		if ( i % STRING_MALLOC_SIZE == 0 )
		{
			auto* tmp = (char*)realloc( output.data, STRING_MALLOC_SIZE * ( j + 1 ) );
			if ( tmp == nullptr )
			{
				free( output.data );
				return EJsonError_OutOfMemory;
			}

			memset( tmp, 0, ( STRING_MALLOC_SIZE * ( j + 1 ) ) - ( STRING_MALLOC_SIZE * j ) );

			j++;
			output.data = tmp;
		}

		if ( *str == '\\' )
		{
			str++;

			if ( *str == 'n' )
				output.data[ i ] = '\n';
			else if ( *str == 't' )
				output.data[ i ] = '\t';
			else
				output.data[ i ] = *str;
		}
		else
		{
			output.data[ i ] = *str;
		}
	}

	str++;
	auto* tmp = (char*)realloc( output.data, i + 1 );

	if ( tmp == nullptr )
	{
		free( output.data );
		return EJsonError_OutOfMemory;
	}

	output.data = tmp;
	output.size = i;
	output.data[ i ] = '\0';

	return EJsonError_None;
}


static EJsonError json_parse_string( const char*& str, json_str_t& output )
{
	const char* base = str;

	int i = 0;
	for ( ; !Json_IsWhitespace( *str ) && *str != ':'; i++, str++ )
	{
		if ( !Json_ValidQuotelessKey( *str ) )
			return EJsonError_InvalidQuotelessKeyCharacter;
	}

	output.data = (char*)malloc( i + 1 );

	if ( output.data == nullptr )
		return EJsonError_OutOfMemory;

	memset( output.data, 0, i + 1 );

	// output = const_cast< char* >( base );
	strncpy( output.data, base, i );
	output.data[ i ] = '\0';
	output.size      = i;

	return EJsonError_None;
}


static EJsonError json_parse_stringNoCheck( const char*& str, json_str_t& output )
{
	const char* base = str;

	int i = 0;
	for ( ; !Json_IsWhitespace( *str ) && *str != ':' && *str != ',' && *str != ']' && *str != '}'; i++, str++ );

	output.data = (char*)malloc( i + 1 );

	if ( output.data == nullptr )
		return EJsonError_OutOfMemory;

	memset( output.data, 0, i + 1 );

	// output = const_cast< char* >( base );
	strncpy( output.data, base, i );
	output.data[ i ] = '\0';
	output.size      = i;

	return EJsonError_None;
}


// TODO: add hex support, and other types
static EJsonError json_parse_number( const char*& str, json_object_t& srObj )
{
	const char* base = str;

	srObj.aType = e_json_type_int;

	int i = 0;
	for ( ; !Json_IsWhitespace( *str ) && *str != ':' && *str != ',' && *str != ']'; i++ )
	{
		if ( *str == '.' )
			srObj.aType = e_json_type_double;

		if ( *str == '+' )
			base = ++str;
		else
			str++;
	}

	char* output = (char*)malloc( i + 1 );

	if ( output == nullptr )
		return EJsonError_OutOfMemory;

	memset( output, 0, i + 1 );

	// output = const_cast< char* >( base );
	strncpy( output, base, i );
	output[ i ] = '\0';

	if ( srObj.aType == e_json_type_double )
	{
		char *end;
		srObj.aDouble = strtod( output, &end );

		if ( end == output )
		{
			free( output );
			return EJsonError_InvalidDouble;
		}
	}
	else  // int
	{
		char *end;
		srObj.aInt = strtoll( output, &end, 0 );

		if ( end == output )
		{
			free( output );
			return EJsonError_InvalidInt;
		}
	}

	free( output );
	return EJsonError_None;
}


static json_object_t* Json_IncrementObjectList( json_array_t& data )
{
	json_object_t* tmp = (json_object_t*)realloc( data.data, sizeof( json_object_t ) * ( data.count + 1 ) );

	if ( tmp == nullptr )
	{
		free( data.data );
		return nullptr;
	}

	memset( tmp + data.count, 0, sizeof( json_object_t ) * 1 );

	u64 index = data.count;

	data.data = tmp;
	data.count++;

	return &data.data[ index ];
}


// --------------------------------------------------------------------------------------
// Public Functions


EJsonError json_parse( json_object_t* spRoot, const char* spStr )
{
	if ( spRoot == nullptr )
		return EJsonError_RootIsNullptr;

	if ( spStr == nullptr )
		return EJsonError_DataIsNullptr;

	// stack for types
	// e_json_type typeStack[128] = {};
	// int typeIndex = 0;

	// std::vector< json_object_t* > objStack;
	// objStack.push_back( spRoot );

	std::forward_list< json_object_t* > objStack;
	objStack.push_front( spRoot );

	memset( spRoot, 0, sizeof( json_object_t ) );

	// int stackCount = 1;
	// json_object_t* objStack = (json_object_t*)malloc( sizeof( json_object_t ) );
	// objStack[0] = *spRoot;
	// json_object_t* objStack = spRoot;
	// json_object_t* cur = nullptr;

	char c;
	for (; *spStr;)
	{
		if ( EJsonError err = Json_SkipWhitespace( spStr ) )
			return err;

		c = *spStr;

		// ------------------------------------------------------
		// Find Root if needed
		if ( spRoot->aType == e_json_type_invalid )
		{
			switch ( c )
			{
				case '{':
				{
					spStr++;
					spRoot->aType = e_json_type_object;

					break;
				}
				case '[':
				{
					spStr++;
					spRoot->aType = e_json_type_array;

					break;
				}
				default:
				{
					return EJsonError_UnexpectedRootType;
				}
			}

			if ( EJsonError err = Json_SkipWhitespace( spStr ) )
				return err;

			c = *spStr;
		}
		else if ( objStack.empty() )
		{
			break;
		}

		// ------------------------------------------------------
		// First, check the key if in an object

		// also add a new json_object_t
		// json_object_t* next = (json_object_t*)malloc( cur->apObjects);
		// cur->apObjects = 

		// parent->apObjects = ( json_object_t* )malloc( sizeof( json_object_t ) );
		// 
		// if ( parent->apObjects == nullptr )
		// 	return EJsonError_OutOfMemory;

		// json_object_t cur{};

		// if ( objStack[ objStack.front() - 1 ]->aType == e_json_type_object )
		if ( objStack.front()->aType == e_json_type_object )
		{
			if ( c == '}' )
			{
				spStr++;
				
				if ( EJsonError err = Json_SkipWhitespace( spStr ) )
					return err;

				// ------------------------------------------------------
				// Check for the ',' character

				// TODO: this doesn't require a comma
				if ( *spStr == ',' )
					spStr++;

				objStack.pop_front();

				continue;
			}
		}
		else if ( objStack.front()->aType == e_json_type_array )
		{
			if ( c == ']' )
			{
				spStr++;

				if ( EJsonError err = Json_SkipWhitespace( spStr ) )
					return err;

				// ------------------------------------------------------
				// Check for the ',' character

				// TODO: this doesn't require a comma
				if ( *spStr == ',' )
					spStr++;

				objStack.pop_front();

				continue;
			}
		}


		json_object_t* cur = Json_IncrementObjectList( objStack.front()->aObjects );

		if ( objStack.front()->aType == e_json_type_object )
		{
			switch ( c )
			{
				case '"':
				case '\'':
				{
					if ( EJsonError err = json_parse_quote( spStr, c, cur->aName ) )
						return err;
					break;
				}
				// Has to be a quoteless string
				default:
				{
					if ( !Json_ValidQuotelessKey( c ) )
					{
						// if it's a number, warn about that
						if ( Json_IsNumber( c ) )
							return EJsonError_KeyStartsWithNumber;

						return EJsonError_InvalidQuotelessKeyCharacter;
					}

					if ( EJsonError err = json_parse_string( spStr, cur->aName ) )
						return err;

					break;
				}
			}
		}

		// ------------------------------------------------------
		// Verify we are at the ':' character

		if ( EJsonError err = Json_SkipWhitespace( spStr ) )
			return err;

		// only if we're not in an array

		if ( objStack.front()->aType != e_json_type_array )
		{
			if ( *spStr != ':' )
				return EJsonError_ExpectedColonCharacter;

			spStr++;

			if ( EJsonError err = Json_SkipWhitespace( spStr ) )
				return err;
		}

		// ------------------------------------------------------
		// Now, check the value

		c = *spStr;

		switch ( c )
		{
			case '{':
			{
				spStr++;
				cur->aType = e_json_type_object;

				objStack.push_front( cur );

				break;
			}
			case '[':
			{
				spStr++;
				cur->aType = e_json_type_array;

				objStack.push_front( cur );

				break;
			}
			case '"':
			case '\'':
			{
				if ( EJsonError err = json_parse_quote( spStr, c, cur->aString ) )
					return err;

				cur->aType = e_json_type_string;
				break;
			}
			default:
			{
				if ( Json_IsNumber( c ) || c == '.' || c == '+' || c == '-' )
				{
					if ( EJsonError err = json_parse_number( spStr, *cur ) )  // ---------------------------------------------------------- why is this a pointer?
						return err;
				}
				
				else
				{
					json_str_t string;
					if ( EJsonError err = json_parse_stringNoCheck( spStr, string ) )
						return err;

					if ( string.size == 4 && strncmp( string.data, "true", 4 ) == 0 )
					{
						cur->aType = e_json_type_true;
					}
					else if ( string.size == 5 && strncmp( string.data, "false", 5 ) == 0 )
					{
						cur->aType = e_json_type_false;
					}
					else if ( string.size == 4 && strncmp( string.data, "null", 4 ) == 0 )
					{
						cur->aType = e_json_type_null;
					}
					else
					{
						return EJsonError_InvalidCharacter;
					}
				}
			}
		}

		if ( EJsonError err = Json_SkipWhitespace( spStr ) )
			return err;

		c = *spStr;

		// ------------------------------------------------------
		// Check for the ',' character

		// TODO: this doesn't require a comma
		if ( c == ',' )
			spStr++;

		// if ( c != ':' )
		// 	return EJsonError_ExpectedColonCharacter;
	}

	return EJsonError_None;
}


void json_free( json_object_t* spRoot )
{
	std::vector< json_object_t* > objList;
	objList.push_back( spRoot );

	// First, get all objects into one huge list
	for ( size_t objIndex = 0; objIndex < objList.size(); objIndex++ )
	{
		// Does the current object have children?
		if ( !( objList[ objIndex ]->aType == e_json_type_object || objList[ objIndex ]->aType == e_json_type_array ) )
			continue;

		if ( objList[ objIndex ]->aObjects.count == 0 )
			continue;

		// put all children in the list and reserve memory for it
		size_t j = objIndex;
		objList.reserve( objList.size() + objList[j]->aObjects.count );
		for ( size_t i = 0; i < objList[j]->aObjects.count; i++ )
		{
			objList.push_back( &objList[j]->aObjects.data[ i ] );
		}
	}

	// Now free all the objects
	while ( !objList.empty() )
	{
		// If this is a string type, free it
		if ( objList[objList.size() - 1]->aType == e_json_type_string )
		{
			free( objList[objList.size() - 1]->aString.data );
			objList[ objList.size() - 1 ]->aString.data = nullptr;
			objList[ objList.size() - 1 ]->aString.size = 0;
		}
		else if ( objList[objList.size() - 1]->aType == e_json_type_object || objList[objList.size() - 1]->aType == e_json_type_array )
		{
			// Free the object list
			free( objList[objList.size() - 1]->aObjects.data );
			objList[ objList.size() - 1 ]->aObjects.data = nullptr;
			objList[ objList.size() - 1 ]->aObjects.count = 0;
		}

		// Free the name if it exists
		if ( objList[objList.size() - 1]->aName.data )
		{
			free( objList[ objList.size() - 1 ]->aName.data );
			objList[ objList.size() - 1 ]->aName.data = nullptr;
			objList[ objList.size() - 1 ]->aName.size = 0;
		}

		// pop the object off the stack
		objList.pop_back();
	}
}


void Json_ToString( json_object_t* spRoot )
{
}


const char* json_error_to_str( EJsonError sErr )
{
	switch ( sErr )
	{
		default:
		case EJsonError_None:
			return "None";

		case EJsonError_RootIsNullptr:
			return "Root is nullptr";

		case EJsonError_DataIsNullptr:
			return "Data is nullptr";

		case EJsonError_InvalidCharacter:
			return "Invalid Character";

		case EJsonError_OutOfMemory:
			return "Out of Memory";

		case EJsonError_UnexpectedRootType:
			return "Unexpected Root Type";

		case EJsonError_ExpectedEndOfBlock:
			return "Expected end of block";

		case EJsonError_ExpectedEndOfArray:
			return "Expected end of array";

		case EJsonError_ExpectedEndOfQuote:
			return "Expected end of quote";

		case EJsonError_KeyStartsWithNumber:
			return "Key starts with number";

		case EJsonError_InvalidQuotelessKeyCharacter:
			return "Invalid Quoteless Key Character";

		case EJsonError_InvalidDouble:
			return "Invalid Double";

		case EJsonError_InvalidInt:
			return "Invalid Integer";

		case EJsonError_ExpectedColonCharacter:
			return "Expected Colon Character";

		case EJsonError_ExpectedCommaCharacter:
			return "Expected Comma Character";

		case EJsonError_NewLineInQuote:
			return "New Line In Quote";
	}
}


const char* json_type_to_str( e_json_type sType )
{
	switch ( sType )
	{
		default:
			return "None";

		case e_json_type_invalid:
			return "Invalid";

		case e_json_type_object:
			return "Object";

		case e_json_type_string:
			return "String";

		case e_json_type_int:
			return "Int";

		case e_json_type_double:
			return "Double";

		case e_json_type_false:
			return "False";

		case e_json_type_true:
			return "True";

		case e_json_type_null:
			return "Null";

		case e_json_type_array:
			return "Array";
	}
}


constexpr size_t STR_BUF_SIZE = 512;


struct json_str_buf_t
{
	char*  data;
	size_t capacity;
	size_t size;
};


static void json_append_str_f( json_str_buf_t& buffer, const char* format, ... )
{
	va_list args, args_copy;

	va_start( args, format );
	va_copy( args_copy, args );

	int len = vsnprintf( nullptr, 0, format, args );
	va_end( args );

	if ( len < 0 )
	{
		va_end( args_copy );
		return;
	}

	if ( ( len + buffer.size ) > buffer.capacity )
	{
		size_t increase = MAX( len, STR_BUF_SIZE );
		char*  new_data = ch_realloc( buffer.data, buffer.capacity + increase );

		if ( !new_data )
		{
			printf( "json_to_str: failed to increase string buffer size!\n" );
			va_end( args_copy );
			return;
		}

		buffer.capacity += increase;
		buffer.data = new_data;
	}

	vsnprintf( &buffer.data[ buffer.size ], len + 1, format, args_copy );
	va_end( args_copy );

	buffer.size += len;
}


static void json_append_str( json_str_buf_t& buffer, const char* str, size_t len )
{
	if ( ( len + buffer.size ) > buffer.capacity )
	{
		size_t increase = MAX( len, STR_BUF_SIZE );
		char*  new_data = ch_realloc( buffer.data, buffer.capacity + increase );

		if ( !new_data )
		{
			printf( "json_to_str: failed to increase string buffer size!\n" );
			return;
		}

		buffer.capacity += increase;
		buffer.data = new_data;
	}

	memcpy( &buffer.data[ buffer.size ], str, len * sizeof( char ) );
	buffer.size += len;
}


static void json_append_str_escape_quotes( json_str_buf_t& buffer, const char* str, size_t len )
{
	// wow this feels dumb lol
	size_t quote_count = 0;

	for ( size_t i = 0; i < len; i++ )
	{
		if ( str[ i ] == '"' )
			quote_count++;
	}

	size_t new_len = len + quote_count;

	if ( ( new_len + buffer.size ) > buffer.capacity )
	{
		size_t increase = MAX( new_len, STR_BUF_SIZE );
		char*  new_data = ch_realloc( buffer.data, buffer.capacity + increase );

		if ( !new_data )
		{
			printf( "json_to_str: failed to increase string buffer size!\n" );
			return;
		}

		buffer.capacity += increase;
		buffer.data = new_data;
	}

	// cool
	for ( size_t i = 0, buf_i = buffer.size; i < len; i++ )
	{
		if ( str[ i ] == '"' )
			buffer.data[ buf_i++ ] = '\\';

		buffer.data[ buf_i++ ] = str[ i ];
	}

	buffer.size += new_len;
}


static void json_append_str( json_str_buf_t& buffer, const char* str )
{
	json_append_str( buffer, str, strlen( str ) );
}

static void json_append_tabs( json_str_buf_t& buffer, size_t depth )
{
	if ( depth > 32 )
	{
		printf( "JSON DEPTH IS GREATER THAN 32??\n" );
		return;
	}

	if ( depth == 0 )
		return;

	char tabs[ 32 ] = { 0 };
	memset( tabs, '\t', depth * sizeof( char ) );

	json_append_str( buffer, tabs, depth );
}


static void json_to_str_recurse( json_str_buf_t& buffer, json_object_t& object, u32 depth )
{
	json_append_tabs( buffer, depth );

	if ( object.aName.data )
	{
		json_append_str( buffer, "\"", 1 );
		json_append_str_escape_quotes( buffer, object.aName.data, object.aName.size );
		json_append_str( buffer, "\": ", 3 );
	}

	switch ( object.aType )
	{
		case e_json_type_object:
		{
			//if ( depth > 0 )
			//	json_append_str( buffer, "\n", 1 );
			//
			//json_append_tabs( buffer, depth );
			json_append_str( buffer, "{\n", 2 );

			for ( size_t i = 0; i < object.aObjects.count; i++ )
			{
				json_to_str_recurse( buffer, object.aObjects.data[ i ], depth + 1 );
			}

			//json_append_str( buffer, "\n", 1 );
			json_append_tabs( buffer, depth );
			json_append_str( buffer, "},\n", 3 );
			break;
		}
		case e_json_type_array:
		{
			if ( depth > 0 )
				json_append_str( buffer, "\n", 1 );

			json_append_tabs( buffer, depth );
			json_append_str( buffer, "[\n", 2 );
			//json_append_tabs( buffer, depth + 1 );

			for ( size_t i = 0; i < object.aObjects.count; i++ )
			{
				json_to_str_recurse( buffer, object.aObjects.data[ i ], depth + 1 );
			}

			//json_append_str( buffer, "\n", 1 );
			json_append_tabs( buffer, depth );
			json_append_str( buffer, "],\n", 3 );
			break;
		}
		case e_json_type_string:
		{
			json_append_str( buffer, "\"", 1 );
			json_append_str_escape_quotes( buffer, object.aString.data, object.aString.size );
			json_append_str( buffer, "\",\n", 3 );
			break;
		}
		case e_json_type_int:
		{
			json_append_str_f( buffer, "%lld,\n", object.aInt );
			break;
		}
		case e_json_type_double:
		{
			json_append_str_f( buffer, "%f,\n", object.aInt );
			break;
		}
		case e_json_type_false:
		{
			json_append_str( buffer, "false,\n", 6 );
			break;
		}
		case e_json_type_true:
		{
			json_append_str( buffer, "true,\n", 6 );
			break;
		}
		case e_json_type_null:
		{
			break;
		}
	}
}


json_str_t json_to_str( json_object_t* root )
{
	if ( !root )
		return {};

	json_str_buf_t buffer{};
	buffer.data     = ch_calloc< char >( STR_BUF_SIZE );
	buffer.capacity = STR_BUF_SIZE;
	buffer.size     = 0;

	//json_append_str( buffer, "{\n\t", 3 );

	json_to_str_recurse( buffer, *root, 0 );

	//json_append_str( buffer, "}\0", 2 );
	json_append_str( buffer, "\0", 1 );

	json_str_t output{};
	output.data = buffer.data;
	output.size = output.size;
	return output;
}


// --------------------------------------------------------------------------------------
// Builder Functions


