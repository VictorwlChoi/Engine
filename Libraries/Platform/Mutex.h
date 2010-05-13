#pragma once

#include "API.h"

#include "Common/Types.h"

#ifndef WIN32
# include <pthread.h>
#endif

namespace Platform
{
  class PLATFORM_API Mutex
  {
  public:
#ifdef WIN32
    typedef CRITICAL_SECTION Handle;
#else
    typedef pthread_mutex_t Handle;
#endif

  private:
    Handle m_Handle;

  public:
    Mutex();
    ~Mutex();

    const Handle& GetHandle()
    {
      return m_Handle;
    }

    void Lock();
    void Unlock();
  };

  //
  // TakeMutex - Allocate one of these on the stack to have it hold a mutex while in a function
  //  This technique is preferred in c++ exception throwing APIs
  //

  class TakeMutex
  {
  private:
    Mutex& m_Mutex;

  public:
    TakeMutex(Mutex& mutex)
      : m_Mutex (mutex)
    {
      m_Mutex.Lock();
    }

  private:
    TakeMutex(const TakeMutex& rhs)
      : m_Mutex (rhs.m_Mutex)
    {

    }

  public:
    ~TakeMutex()
    {
      m_Mutex.Unlock();
    }
  };

  //
  // Locker - Simple template to make some data only accessible to one thread at a time
  //

  template<class T>
  class Locker
  {
  public:
    friend class Handle;
    class Handle
    {
    public:
      Handle( Locker* locker )
        : m_Locker( locker )
      {
        m_Locker->m_Mutex.Lock();
      }

    public:
      ~Handle()
      {
        m_Locker->m_Mutex.Unlock();
      }

      inline T* operator->()
      {
        return &m_Locker->m_Data;
      }

    private:
      Locker* m_Locker;
    };

    inline Handle Lock()
    {
      return Handle( this );
    }

  private:
    T               m_Data;
    Platform::Mutex m_Mutex;
  };
}