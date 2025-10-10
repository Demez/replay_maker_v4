#pragma once

#include "util.h"

// ===============================================
// Logging System


enum log_channel
{
	log_general,
	log_warning,
	log_error,
	log_ffmpeg
};


using e_log_color = u8;
enum e_log_color_ : e_log_color
{
	e_log_color_default,

	e_log_color_black,
	e_log_color_white,

	e_log_color_dark_blue,
	e_log_color_dark_green,
	e_log_color_dark_cyan,
	e_log_color_dark_red,
	e_log_color_dark_purple,
	e_log_color_dark_yellow,
	e_log_color_dark_gray,

	e_log_color_blue,
	e_log_color_green,
	e_log_color_cyan,
	e_log_color_red,
	e_log_color_purple,
	e_log_color_yellow,
	e_log_color_gray,

	e_log_color_max = e_log_color_gray,
	e_log_color_count,
};


bool log_init();
void log_shutdown();

void log_printf( const char* format, ... );
void log_printf( log_channel channel, const char* format, ... );

