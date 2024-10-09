#pragma once

#include "util.h"
#include <vector>

enum EJsonError
{
	EJsonError_None,
	EJsonError_RootIsNullptr,
	EJsonError_DataIsNullptr,
	EJsonError_InvalidCharacter,
	EJsonError_OutOfMemory,
	EJsonError_UnexpectedRootType,
	EJsonError_ExpectedEndOfBlock,
	EJsonError_ExpectedEndOfArray,
	EJsonError_ExpectedEndOfQuote,
	EJsonError_KeyStartsWithNumber,
	EJsonError_InvalidQuotelessKeyCharacter,
	EJsonError_InvalidDouble,
	EJsonError_InvalidInt,
	EJsonError_ExpectedColonCharacter,
	EJsonError_ExpectedCommaCharacter,
	EJsonError_NewLineInQuote,

	EJsonError_Unknown,  // FALLBACK ERROR TYPE
};


enum e_json_type : char
{
	e_json_type_invalid,
	e_json_type_object,
	e_json_type_array,
	e_json_type_string,
	e_json_type_int,
	e_json_type_double,
	e_json_type_false,
	e_json_type_true,
	e_json_type_null,
};


struct json_object_t;


struct json_array_t
{
	json_object_t* data;
	size_t         count;
};


struct json_str_t
{
	char*  data;
	size_t size;
};


struct json_object_t
{
	json_str_t aName;
	e_json_type aType = e_json_type_invalid;

	union
	{
		json_array_t aObjects;
		s64          aInt;
		double       aDouble;
		json_str_t   aString;
	};

	json_object_t()
	{
	}

	json_object_t( const json_object_t& other )
	{
		memcpy( this, &other, sizeof( json_object_t ) );
	}

	~json_object_t()
	{
	}
};


EJsonError  json_parse( json_object_t* root, const char* source );
void        json_free( json_object_t* root );
const char* json_error_to_str( EJsonError err );
const char* json_type_to_str( e_json_type type );
json_str_t  json_to_str( json_object_t* root );


// json5 building


