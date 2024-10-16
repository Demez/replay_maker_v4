#pragma once

#include "util.h"

void        args_init( int argc, char* argv[] );
void        args_free();

void        args_print_help();

const char* args_register_str( const char* default_val, const char* desc, const char* cmd_switch );
bool        args_register_bool( const char* desc, const char* cmd_switch );

// float args_register_float( const char* default_val, const char* desc, u32 switch_count, const char* cmd_switch, ... );

