cmake_minimum_required( VERSION 3.18 FATAL_ERROR )

# use folders for visual studio
set_property( GLOBAL PROPERTY USE_FOLDERS ON )

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo")

set( CMAKE_POSITION_INDEPENDENT_CODE ON )
set( CMAKE_SHARED_LIBRARY_PREFIX "" )

project( replay_maker )

set( PROJ_OBJ "obj" )
set( PROJ_OUT "${CMAKE_CURRENT_LIST_DIR}/out" )

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJ_OUT} )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJ_OUT} )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJ_OUT} )

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )

    # set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}/bin )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJ_OUT} )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJ_OUT} )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJ_OUT} )
endforeach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )

set_property( GLOBAL PROPERTY PREFIX "" )
set_property( GLOBAL PROPERTY USE_FOLDERS ON )

set( CMAKE_CXX_STANDARD 20 )

# Compiler/Platform specifc options
if ( MSVC )

	add_compile_definitions(
		_AMD64_ __x86_64 __amd64
		NOMINMAX
		_CRT_SECURE_NO_WARNINGS
		_ALLOW_RUNTIME_LIBRARY_MISMATCH _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH _ALLOW_MSC_VER_MISMATCH
		_UNICODE=1
		UNICODE=1
	)
	
	# Use these runtime libraries for both so it doesn't crash smh my head
	# actually this is useless right now because of core.dll, god dammit
	# set_property( GLOBAL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" )
	
	set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" )
	set_property( GLOBAL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" )

	set( MSVC_VERSION 1939 )

	add_compile_options(
		# "/Wall"             # Show ALL Warnings
		"/W4"               # Warning Level 4
		"/MP"               # multi-threaded compilation
		"/permissive-"      # make MSVC very compliant
		"/Zc:__cplusplus"   # set the __cplusplus macro to be the correct version
		# "/fp:fast"          # fast floating point model
		"/GF"               # string pooling: allows compiler to create a single copy of identical strings in the program image and in memory during execution
		
		# disable stupid warnings
	#	"/wd4244"  # conversion from 'X' to 'Y', possible loss of data
	#	"/wd4267"  # conversion from 'X' to 'Y', possible loss of data (yes there's 2 idk why)
	#	"/wd4305"  # truncation from 'double' to 'float'
	#	"/wd4464"  # relative include path contains ".."
	#	"/wd4668"  # 'X' is not defined as a preprocessor macro, replacing '0' for '#if/#elif' (floods with win32 stuff)
	#	"/wd4514"  # 'X' unreferenced inline function has been removed
	#	"/wd4505"  # 'X' unreferenced function with internal linkage has been removed
	#	"/wd4100"  # 'X' unreferenced formal paramter
	#	"/wd4201"  # nonstandard extension used: nameless struct/union (used in glm)
	#	"/wd5045"  # Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
    #
	#	"/wd4706"  # assignment within conditional expression
	#	
	#	# probably temporary
	#	"/wd4365"  # signed/unsigned mismatch
	#	"/wd4061"  # enumerator 'X' in switch of enum 'Y' is not explicitly handled by a case label
	#	
	#	# padding stuff
	#	"/wd4324"  # 'X' structure was padded due to alignment specifier
	#	"/wd4820"  # 'X' bytes padding added after data member 'Y'
    #
	#	# Treat these warnings as errors
    #
	#	# MSVC doesn't care about this one, but this errors on gcc and can help catch a mistake i made unknowingly
	#	"/we4002"  # Too many arguments for function-like macro invocation
    #
	#	# I accidently get this a lot with printing std::string, so I want this to error so I know i made a mistake
	#	"/we4840"  # non-portable use of class 'type' as an argument to a variadic function
    #
	#	# This is self-explanitory
	#	"/we4129"  # 'X' unrecognized character escape sequence
	)

	set( COMPILE_OPTIONS_DEBUG
		"/Od"        # no optimizations
		"/ZI"        # edit and continue
		"/fp:except" # raise floating point exceptions
		#"/fsanitize=address" # use address sanitizing in Debug (incompatible with /INCREMENTAL)
	)
	
	set( COMPILE_OPTIONS_RELEASE
		"/fp:fast"
	)
	
	add_compile_options(
		"$<$<CONFIG:Debug>:${COMPILE_OPTIONS_DEBUG}>"
		"$<$<CONFIG:Release>:${COMPILE_OPTIONS_RELEASE}>"
		"$<$<CONFIG:RelWithDebInfo>:${COMPILE_OPTIONS_RELEASE}>"
	)
	
	set( LINK_OPTIONS_DEBUG
		""
	)
	
	set( LINK_OPTIONS_RELEASE
		""
	)
	
	add_link_options(
		"$<$<CONFIG:Debug>:${LINK_OPTIONS_DEBUG}>"
		"$<$<CONFIG:Release>:${LINK_OPTIONS_RELEASE}>"
		"$<$<CONFIG:RelWithDebInfo>:${LINK_OPTIONS_RELEASE}>"
	)
	
	add_link_options(
		"/INCREMENTAL"
	)

	link_libraries( Opengl32 )
	
	message( "Copying SDL3" )
	configure_file( "thirdparty/SDL3/lib/x64/SDL3.dll" "${PROJ_OUT}/SDL3.dll" COPYONLY )
	
	message( "Copying MPV" )
	configure_file( "thirdparty/mpv/libmpv-2.dll" "${PROJ_OUT}/libmpv-2.dll" COPYONLY )
	
else()  # linux
endif()


# ========================================================================================================

set(
    SRC_FILES

    src/main.cpp
    src/main.h
    src/util.cpp
    src/util.h
    src/args.cpp
    src/args.h
    src/logging.cpp
    src/logging.h

    src/ui/mpv_interface.cpp
    src/ui/ui_encode.cpp
    src/ui/ui_editor.cpp
    src/ui/ui_timeline.cpp
    src/ui/undo.cpp
    src/ui/key_bind.cpp

    src/imgui/imconfig.h
    src/imgui/imgui.cpp
    src/imgui/imgui_demo.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_impl_opengl3.cpp
    src/imgui/imgui_impl_sdl3.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_widgets.cpp
	src/imgui/imgui_freetype.cpp
	src/imgui/imgui_freetype.h

    src/encoder/encoder.cpp
    src/encoder/encoder.h
    src/encoder/encoder_main.cpp
	
    src/clip/clip.cpp
    src/clip/clip.h
    src/clip/json5.cpp
    src/clip/json5.h

	src/glad/glad.c
	src/glad/glad.h
	src/glad/khrplatform.h
)

if ( WIN32 )
	set(
		SRC_FILES
		${SRC_FILES}

		src/ui/sys_win32_drag_drop.cpp
		src/ui/sys_win32_replay.cpp
		src/imgui/imgui_impl_win32.cpp
		src/sys_win32.cpp
	)
else()
endif()


# ========================================================================================================


add_executable( replay_maker ${SRC_FILES} )

source_group(
	TREE ${CMAKE_CURRENT_LIST_DIR}/src
	PREFIX "Source Files"
	FILES ${SRC_FILES}
)

# native file dialog
target_link_libraries( replay_maker PRIVATE "nfd" "freetype" "SDL3" )
target_link_directories( replay_maker PRIVATE "thirdparty/nativefiledialog/build/src" )

if ( WIN32 )
	target_link_directories( replay_maker PRIVATE "thirdparty/SDL3/lib/x64" )
endif()

foreach( OUTPUT_CONFIG ${CMAKE_CONFIGURATION_TYPES} )
	target_link_directories(
		replay_maker PRIVATE
		"thirdparty/nativefiledialog/build/src/${OUTPUT_CONFIG}"
		"thirdparty/freetype/objs/x64/${OUTPUT_CONFIG} Static"
	)
endforeach( OUTPUT_CONFIG ${CMAKE_CONFIGURATION_TYPES} )

target_include_directories(
	replay_maker PRIVATE
	"src"
	"src/replay_maker_ui"
	"src/encoder"
	"src/imgui"
	"src/glad"
	"thirdparty/SDL3/include"
	"thirdparty/mpv/include"
	"thirdparty/nativefiledialog/src/include"
	"thirdparty/freetype/include"
)

set_target_properties(
	replay_maker PROPERTIES
	OUTPUT_NAME replay_maker
	# RUNTIME_OUTPUT_DIRECTORY ${PROJ_OUT}
	
	VS_DEBUGGER_WORKING_DIRECTORY ${PROJ_OUT}
)


