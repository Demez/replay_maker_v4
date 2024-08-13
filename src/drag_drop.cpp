#include "main.h"

#if 1
  #include <vector>

// #include <WinUser.h>
// #include <Windows.h>
// #include <direct.h>
//#include <fileapi.h>
// #include <handleapi.h>
// #include <io.h>
// #include <memory>
 #include <shlobj_core.h>
// #include <shlwapi.h>
#include <windowsx.h>  // GET_X_LPARAM/GET_Y_LPARAM
#include <wtypes.h>    // HWND


ULONG gDropFormats[] = {
	// CF_TEXT,   // dragging in a file path/url from discord or your internet browser
	CF_HDROP,  // file drop
};

ULONG gDropFormatCount = ARR_SIZE( gDropFormats );


struct window_drop_target : public IDropTarget
{
	HWND                    aHWND          = 0;
	LONG                    nRef           = 0L;

	bool                    aDropSupported = false;

	wchar_t**               drop_files     = nullptr;
	u32*                    drop_files_len  = nullptr;
	u32                     drop_file_count = 0;

	bool add_drop_file( const wchar_t* file )
	{
		{
			wchar_t** new_memory = (wchar_t**)realloc( drop_files, (drop_file_count + 1) * sizeof( wchar_t* ) );

			if ( new_memory == nullptr )
			{
				printf( "realloc failed\n" );
				return false;
			}

			drop_files = new_memory;
		}

		{
			u32* new_memory = (u32*)realloc( drop_files_len, (drop_file_count + 1) * sizeof( u32 ) );

			if ( new_memory == nullptr )
			{
				printf( "realloc failed\n" );
				return false;
			}

			drop_files_len = new_memory;
		}

		// allocate memory for the string
		drop_files_len[ drop_file_count ] = wcslen( file );
		drop_files[ drop_file_count ]     = (wchar_t*)malloc( ( drop_files_len[ drop_file_count ] + 1 ) * sizeof( wchar_t ) );

		wcscpy( drop_files[ drop_file_count ], file );

		drop_file_count++;

		return true;
	}

	void clear_drop_files()
	{
		for ( u32 i = 0; i < drop_file_count; i++ )
		{
			free( drop_files[ i ] );
			drop_files[ i ] = nullptr;
		}

		// dont free these arrays, we can just realloc them later
		//free( drop_files );
		//free( drop_files_len );
		//
		//drop_files     = nullptr;
		//drop_files_len = nullptr;
		drop_file_count = 0;
	}

	bool                    IsValidClipboardType( IDataObject* pDataObj, FORMATETC& fmtetc )
	{
		ULONG lFmt;
		for ( lFmt = 0; lFmt < gDropFormatCount; lFmt++ )
		{
			fmtetc.cfFormat = gDropFormats[ lFmt ];
			if ( pDataObj->QueryGetData( &fmtetc ) == S_OK )
				return true;
		}

		return false;
	}

	bool GetFilesFromDataObject( FORMATETC& fmtetc, IDataObject* pDataObj )
	{
		STGMEDIUM pmedium;
		HRESULT   ret = pDataObj->GetData( &fmtetc, &pmedium );

		if ( ret != S_OK )
			return false;

		DROPFILES* dropfiles = (DROPFILES*)GlobalLock( pmedium.hGlobal );
		HDROP      drop      = (HDROP)dropfiles;

		auto       fileCount = DragQueryFileW( drop, 0xFFFFFFFF, NULL, NULL );

		for ( UINT i = 0; i < fileCount; i++ )
		{
			TCHAR filepath[ MAX_PATH ];
			DragQueryFile( drop, i, filepath, ARR_SIZE( filepath ) );
			// NOTE: maybe check if we can load this file here?
			// some callback function or ImageLoader_SupportsImage()?

			// TODO: DO ASYNC TO NOT LOCK UP FILE EXPLORER !!!!!!!!
		// 	if ( ImageLoader_SupportsImage( filepath ) )
			{
				add_drop_file( filepath );
			}
		}

		GlobalUnlock( pmedium.hGlobal );

		ReleaseStgMedium( &pmedium );

		return true;
	}

	HRESULT DragEnter( IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect ) override
	{
		clear_drop_files();

		// needed?
		// FORMATETC formats;
		// pDataObj->EnumFormatEtc( DATADIR_GET, formats );

		FORMATETC fmtetc = { CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		bool      valid  = IsValidClipboardType( pDataObj, fmtetc );
		if ( !valid )
			return S_FALSE;

		if ( !GetFilesFromDataObject( fmtetc, pDataObj ) )
			return S_FALSE;

		if ( drop_file_count == 0 )
		{
			aDropSupported = false;
			*pdwEffect     = DROPEFFECT_SCROLL;
			// return S_FALSE;
			return S_OK;
		}
		else
		{
			aDropSupported = true;
			*pdwEffect     = DROPEFFECT_LINK;
			return S_OK;
		}
	}

	HRESULT DragOver( DWORD grfKeyState, POINTL pt, DWORD* pdwEffect ) override
	{
		if ( aDropSupported )
		{
			*pdwEffect = DROPEFFECT_LINK;
		}
		else
		{
			*pdwEffect = DROPEFFECT_SCROLL;  // actually shows the "invalid" cursor or whatever it's called
		}

		return S_OK;
	}

	HRESULT DragLeave() override
	{
		return S_OK;
	}

	HRESULT Drop( IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect ) override
	{
		FORMATETC fmtetc = { CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		bool      valid  = IsValidClipboardType( pDataObj, fmtetc );
		if ( !valid )
			return S_FALSE;

		if ( !GetFilesFromDataObject( fmtetc, pDataObj ) )
			return S_FALSE;

		// TODO: make async/non-blocking
	//	ImageView_SetImage( aDropFiles[ 0 ] );

		if ( drop_file_count == 0 )
			return S_FALSE;

		wchar_t* file = drop_files[ 0 ];

		mpv_cmd_loadfile( file );

		// SetFocus( aHWND );

		return S_OK;
	}

	// from IUnknown

	// this is not being called, huh
	HRESULT QueryInterface( REFIID riid, void** ppvObject ) override
	{
		printf( "QueryInterface\n" );

		if ( riid == IID_IUnknown || riid == IID_IDropTarget )
		{
			AddRef();
			*ppvObject = this;
			return S_OK;
		}
		else
		{
			*ppvObject = 0;
			return E_NOINTERFACE;
		};
	}

	ULONG AddRef() override
	{
		return ++nRef;
		// printf( "AddRef\n" );
		// return 0;
	}

	ULONG Release() override
	{
		ULONG uRet = --nRef;
		if ( uRet == 0 )
			delete this;

		return uRet;

		// printf( "Release\n" );
		// return 0;
	}
};


window_drop_target** g_drop_target = nullptr;
u32                  g_drop_target_count = 0;

// ---------------------------------------------------------------------


bool win32_register_drag_drop( HWND hwnd )
{
	// allocate a new drop target
	auto new_memory = (window_drop_target**)realloc( g_drop_target, (g_drop_target_count + 1) * sizeof( window_drop_target* ) );

	if ( new_memory == nullptr )
	{
		printf( "realloc failed\n" );
		return false;
	}

	g_drop_target = new_memory;

	g_drop_target[ g_drop_target_count ] = new window_drop_target;
	window_drop_target* target = g_drop_target[ g_drop_target_count ];

	g_drop_target_count++;

	target->aHWND = hwnd;

	// window_drop_target* test   = new window_drop_target;

	if ( RegisterDragDrop( hwnd, target ) != S_OK )
	{
		printf( "RegisterDragDrop failed\n" );
		return false;
	}

	return true;
}

#endif