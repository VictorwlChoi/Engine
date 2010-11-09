//----------------------------------------------------------------------------------------------------------------------
// ThreadingWin.inl
//
// Copyright (C) 2010 WhiteMoon Dreams, Inc.
// All Rights Reserved
//----------------------------------------------------------------------------------------------------------------------

#include "Platform/Atomic.h"

namespace Lunar
{
    /// Get the name assigned to this thread.
    ///
    /// @return  Thread name.
    ///
    /// @see SetName()
    const String& Thread::GetName() const
    {
        return m_name;
    }

    /// Suspend execution of the calling thread for at least a given period of time.
    ///
    /// @param[in] milliseconds  Number of milliseconds to sleep.  Note that the calling thread may not be awaken
    ///                          immediately upon the timer expiring.
    ///
    /// @see Yield()
    void Thread::Sleep( uint32_t milliseconds )
    {
        ::Sleep( milliseconds );
    }

    /// Yield the remainder of the calling thread's time slice for other threads of equal priority.
    ///
    /// If other threads of equal priority are awaiting execution, this will immediately yield execution to those
    /// threads.  If no other threads of equal priority are waiting, the thread will continue execution immediately.
    ///
    /// To yield control to lower-priority threads, Sleep() should be called instead with a non-zero amount of time.
    ///
    /// @see Sleep()
    void Thread::Yield()
    {
        ::Sleep( 0 );
    }

    /// Get the ID of the thread in which this function is called.
    ///
    /// @return  Current thread ID.
    Thread::id_t Thread::GetCurrentId()
    {
        return GetCurrentThreadId();
    }

    /// Allocate a thread-local storage index.
    ///
    /// When an index is allocated by a thread, any thread can subsequently store and retrieve values in the slot.
    /// Thread-local storage values are always initialized to zero for all threads.
    ///
    /// @return  Allocated index, or an invalid index if storage could not be allocated.
    ///
    /// @see Free(), Get(), Set()
    size_t ThreadLocalStorage::Allocate()
    {
        DWORD index = TlsAlloc();
        return( index != TLS_OUT_OF_INDEXES ? static_cast< size_t >( index ) : Invalid< size_t >() );
    }

    /// Free a previously allocated thread-local storage index.
    ///
    /// @param[in] index  Index of the storage slot to free.
    ///
    /// @see Allocate()
    void ThreadLocalStorage::Free( size_t index )
    {
        HELIUM_ASSERT( IsValid( index ) );
        HELIUM_VERIFY( TlsFree( static_cast< DWORD >( index ) ) );
    }

    /// Get the value stored for this thread in a thread-local storage slot.
    ///
    /// @param[in] index  Thread-local storage slot index.
    ///
    /// @return  Value currently stored in the specified slot.
    ///
    /// @see Allocate(), Free(), Set()
    uintptr_t ThreadLocalStorage::Get( size_t index )
    {
        HELIUM_ASSERT( IsValid( index ) );
        void* pValue = TlsGetValue( static_cast< DWORD >( index ) );

        return reinterpret_cast< uintptr_t >( pValue );
    }

    /// Set the thread-local storage value in a given storage slot for this thread.
    ///
    /// @param[in] index  Thread-local storage slot index.
    /// @param[in] value  Value to set.
    ///
    /// @see Allocate(), Free(), Get()
    void ThreadLocalStorage::Set( size_t index, uintptr_t value )
    {
        HELIUM_ASSERT( IsValid( index ) );
        HELIUM_VERIFY( TlsSetValue( static_cast< DWORD >( index ), reinterpret_cast< void* >( value ) ) );
    }

    /// Constructor.
    ThreadLocalStorage::ThreadLocalStorage()
    {
    }

    /// Destructor.
    ThreadLocalStorage::~ThreadLocalStorage()
    {
    }
}
