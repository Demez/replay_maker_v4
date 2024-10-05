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


enum EJsonType : char
{
	EJsonType_Invalid,
	EJsonType_Object,
	EJsonType_String,
	EJsonType_Int,
	EJsonType_Double,
	EJsonType_False,
	EJsonType_True,
	EJsonType_Null,
	EJsonType_Array,
};


struct JsonObject_t;


struct JsonArray_t
{
	JsonObject_t* apData;
	size_t        aCount;
};


struct json_str_t
{
	char*  data;
	u64    size;
};


struct JsonObject_t
{
	json_str_t aName;
	EJsonType aType = EJsonType_Invalid;

	union
	{
		JsonArray_t       aObjects;
		s64               aInt;
		double            aDouble;
		json_str_t        aString;
	};

	JsonObject_t()
	{
	}

	JsonObject_t( const JsonObject_t& other )
	{
		memcpy( this, &other, sizeof( JsonObject_t ) );
	}

	~JsonObject_t()
	{
	}
};


EJsonError  Json_Parse( JsonObject_t* spRoot, const char* spSource );
void        Json_Free( JsonObject_t* spRoot );
const char* Json_ErrorToStr( EJsonError sErr );
const char* Json_TypeToStr( EJsonType sType );

