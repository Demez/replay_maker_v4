#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <utility>
#include <algorithm>


template< typename T >
struct ChVector
{
	T*       apData    = nullptr;
	uint32_t aSize     = 0;
	uint32_t aCapacity = 0;

	// Returns true if there are allocated buffers, false if the memory isn't and should not be accessed.
	bool     valid() const { return apData; }

	// Returns the buffer.
	T*&      data() { return apData; }

	// Returns the size of the buffer.
	uint32_t size() const { return aSize; }

	// Returns the size of the buffer in bytes
	size_t   size_bytes() const { return aSize * sizeof( T ); }

	// Returns the amount of the buffer currently allocated
	uint32_t capacity() const { return aCapacity; }

	// Returns if the buffer is empty or not.
	bool     empty() const { return aSize == 0; }

	// Returns the topmost buffer.
	T&       top() const { return apData[ aSize - 1 ]; }

	// Resizes the buffer
	void     resize( uint32_t sSize )
	{
#ifdef _DEBUG
		// CH_ASSERT()
#endif

		if ( sSize > 0 && sSize > aCapacity )
		{
			void* newData = realloc( apData, sSize * sizeof( T ) );
			if ( newData == nullptr )
				printf( "Failed to Resize ChVector< %s >: %zd bytes\n", typeid( T ).name(), sSize );

			apData = static_cast< T* >( newData );

			// if ( sZero && sSize > aCapacity )
			if ( sSize > aCapacity )
			{
				// Zero out the new data
				memset( &apData[ aCapacity ], 0, ( sSize - aCapacity ) * sizeof( T ) );
			}

			aCapacity = sSize;
		}
		else if ( sSize == 0 )
		{
			free_data();
		}
		else
		{
			if ( aSize < sSize )
			{
				// Zero out the new data
				memset( &apData[ aSize ], 0, ( sSize - aSize ) * sizeof( T ) );
			}
		}

		aSize = sSize;
	}

	// Pre-allocates memory while maintaining stored size
	void reserve( uint32_t sSize )
	{
		if ( sSize > 0 && sSize > aCapacity )
		{
			void* newData = realloc( apData, sSize * sizeof( T ) );
			if ( newData == nullptr )
				printf( "Failed to Resize ChVector< %s >: %zd bytes\n", typeid( T ).name(), sSize * sizeof( T ) );

			apData = static_cast< T* >( newData );

			// Zero out the new data
			memset( &apData[ aCapacity ], 0, ( sSize - aCapacity ) * sizeof( T ) );

			aCapacity = sSize;
		}
		else if ( sSize == 0 )
		{
			free_data();
		}
	}

	// Reset size, does not free any memory, use free_data() if you want that
	void clear()
	{
		aSize = 0;
	}

	// Free all allocated memory
	void free_data()
	{
		if ( apData )
		{
			free( apData );
			apData = nullptr;
		}

		aSize     = 0;
		aCapacity = 0;
	}

	// Free's extra memory allocated (aCapacity)
	void consolidate()
	{
		if ( aSize == aCapacity || !apData )
			return;

		if ( aSize == 0 )
		{
			free_data();
		}
		else
		{
			void* newData = realloc( apData, aSize * sizeof( T ) );
			if ( newData == nullptr )
				printf( "Failed to Resize ChVector< %s >: %zd bytes\n", typeid( T ).name(), apData );

			apData = static_cast< T* >( newData );
		}

		aCapacity = aSize;
	}

	// Add one element to the buffer and returns it
	T& emplace_back()
	{
		resize( aSize + 1 );
		return apData[ aSize - 1 ];
	}

	// Add one element to the buffer
	void push_back( const T& srData )
	{
		resize( aSize + 1 );
		top() = srData;
	}

	void push_back( T&& srData )
	{
		resize( aSize + 1 );

		// dst, src, size
		// memcpy( &apData[ aSize - 1 ], &srData, sizeof( T ) );
		top() = std::move( srData );
	}

	// Remove an item by index
	void remove( uint32_t sIndex )
	{
		if ( sIndex > aSize )
			printf( "Attempted to remove an index out of bounds in buffer: %zd > %zd\n", sIndex, aSize );

		// shift all memory back one index
		if ( aSize )
		{
			uint32_t sSize = aSize - sIndex;
			memcpy( &apData[ sIndex ], &apData[ sIndex + 1 ], sSize * sizeof( T ) );
		}

		aSize--;
	}

	// Find the index of an item in the vector
	uint32_t index( const T& sItem, uint32_t sFallback = UINT32_MAX )
	{
		auto it = std::find( begin(), end(), sItem );
		if ( it != end() )
			return it - begin();

		return sFallback;
	}

	// Search for an item and remove it from the vector
	void erase( const T& sItem )
	{
		uint32_t index = this->index( sItem );

		if ( index == UINT32_MAX )
		{
			printf( "Failed to find item in ChVector< %s >!\n", typeid( T ).name() );
			return;
		}

		remove( index );
	}

	// void insert( const T& sItem, uint32_t sIndex )
	// {
	// 	resize( aSize + 1, true );
	//
	// 	// shift all memory after this index by one index
	// 	if ( aSize > 1 )
	// 	{
	// 		uint32_t sSize = aSize - sIndex;
	// 		memcpy( &apData[ sIndex + 1 ], &apData[ sIndex ], sSize * sizeof( T ) );
	// 	}
	//
	// 	apData[ sIndex ] = std::move( sItem );
	// }

	// Index into the buffer
	T& operator[]( uint32_t sIndex ) const
	{
		if ( sIndex > aSize )
			printf( "Attempted to index out of bounds in buffer: %zd > %zd\n", sIndex, aSize );

		return apData[ sIndex ];
	}

	// Support for range based for loops
	T* begin() const
	{
		return apData;
	}

	T* end() const
	{
		return &apData[ aSize ];
	}

	// T front() const
	// {
	// 	if ( aSize == 0 )
	// 		Log_Fatal( "Attempted to get front() when vector size is 0\n" );
	//
	// 	return apData[ 0 ];
	// }
	//
	T* back() const
	{
		if ( aSize == 0 )
		{
			printf( "Attempted to get back() when vector size is 0\n" );
			return nullptr;
		}

		return &apData[ aSize > 0 ? aSize - 1 : 0 ];
	}

	ChVector() :
		apData( nullptr ), aSize( 0 ), aCapacity( 0 )
	{
	}

	ChVector( uint32_t sSize ) :
		apData( nullptr ), aSize( 0 ), aCapacity( 0 )
	{
		resize( sSize );
	}

	~ChVector()
	{
		free_data();
	}

	// --------------------------------------------------
	// necessary stuff for objects

	// copying
	void assign( const ChVector& other )
	{
		if ( !other.aSize )
			return;

		resize( other.aSize );

		memcpy( apData, other.apData, other.aSize * sizeof( T ) );

		// std::move( other.begin(), other.end(), apData );
	}

	// moving
	void assign( ChVector&& other )
	{
		if ( !other.aSize )
			return;

		resize( other.aSize );
		memcpy( apData, other.apData, other.aSize * sizeof( T ) );

		// swap the values of this one with that one
		// std::swap( aSize, other.aSize );
		// std::swap( aCapacity, other.aCapacity );
		// std::swap( apData, other.apData );
	}

	ChVector& operator=( const ChVector& other )
	{
		if ( valid() )
		{
			aSize = other.aSize;
			consolidate();
		}

		assign( other );
		return *this;
	}

	ChVector& operator=( ChVector&& other )
	{
		if ( valid() )
		{
			aSize = other.aSize;
			consolidate();
		}

		// assign( std::move( other ) );
		assign( other );
		return *this;
	}

	// copying
	ChVector( const ChVector& other )
	{
		assign( other );
	}

	// moving
	ChVector( ChVector&& other )
	{
		// assign( std::move( other ) );
		assign( other );
	}
};

