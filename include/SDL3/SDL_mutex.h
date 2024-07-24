/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef SDL_mutex_h_
#define SDL_mutex_h_

/**
 * # CategoryMutex
 *
 * Functions to provide thread synchronization primitives.
 */

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_error.h>

/******************************************************************************/
/* Enable thread safety attributes only with clang.
 * The attributes can be safely erased when compiling with other compilers.
 *
 * To enable analysis, set these environment variables before running cmake:
 *      export CC=clang
 *      export CFLAGS="-DSDL_THREAD_SAFETY_ANALYSIS -Wthread-safety"
 */
#if defined(SDL_THREAD_SAFETY_ANALYSIS) && \
    defined(__clang__) && (!defined(SWIG))
#define SDL_THREAD_ANNOTATION_ATTRIBUTE__(x)   __attribute__((x))
#else
#define SDL_THREAD_ANNOTATION_ATTRIBUTE__(x)   /* no-op */
#endif

#define SDL_CAPABILITY(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define SDL_SCOPED_CAPABILITY \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define SDL_GUARDED_BY(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define SDL_PT_GUARDED_BY(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define SDL_ACQUIRED_BEFORE(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(x))

#define SDL_ACQUIRED_AFTER(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(x))

#define SDL_REQUIRES(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(x))

#define SDL_REQUIRES_SHARED(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(x))

#define SDL_ACQUIRE(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(x))

#define SDL_ACQUIRE_SHARED(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(x))

#define SDL_RELEASE(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(release_capability(x))

#define SDL_RELEASE_SHARED(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(x))

#define SDL_RELEASE_GENERIC(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(release_generic_capability(x))

#define SDL_TRY_ACQUIRE(x, y) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(x, y))

#define SDL_TRY_ACQUIRE_SHARED(x, y) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(x, y))

#define SDL_EXCLUDES(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(x))

#define SDL_ASSERT_CAPABILITY(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define SDL_ASSERT_SHARED_CAPABILITY(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define SDL_RETURN_CAPABILITY(x) \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define SDL_NO_THREAD_SAFETY_ANALYSIS \
  SDL_THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

/******************************************************************************/


#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Synchronization functions return this value if they time out.
 *
 * Not all functions _can_ time out; some will block indefinitely.
 *
 * \since This macro is available since SDL 3.0.0.
 */
#define SDL_MUTEX_TIMEDOUT  1


/**
 *  \name Mutex functions
 */
/* @{ */

/**
 * A means to serialize access to a resource between threads.
 *
 * Mutexes (short for "mutual exclusion") are a synchronization primitive that
 * allows exactly one thread to proceed at a time.
 *
 * Wikipedia has a thorough explanation of the concept:
 *
 * https://en.wikipedia.org/wiki/Mutex
 *
 * \since This struct is available since SDL 3.0.0.
 */
typedef struct SDL_Mutex SDL_Mutex;

/**
 * Create a new mutex.
 *
 * All newly-created mutexes begin in the _unlocked_ state.
 *
 * Calls to SDL_LockMutex() will not return while the mutex is locked by
 * another thread. See SDL_TryLockMutex() to attempt to lock without blocking.
 *
 * SDL mutexes are reentrant.
 *
 * \returns the initialized and unlocked mutex or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroyMutex
 * \sa SDL_LockMutex
 * \sa SDL_TryLockMutex
 * \sa SDL_UnlockMutex
 */
extern SDL_DECLSPEC SDL_Mutex * SDLCALL SDL_CreateMutex(void);

/**
 * Lock the mutex.
 *
 * This will block until the mutex is available, which is to say it is in the
 * unlocked state and the OS has chosen the caller as the next thread to lock
 * it. Of all threads waiting to lock the mutex, only one may do so at a time.
 *
 * It is legal for the owning thread to lock an already-locked mutex. It must
 * unlock it the same number of times before it is actually made available for
 * other threads in the system (this is known as a "recursive mutex").
 *
 * This function does not fail; if mutex is NULL, it will return immediately
 * having locked nothing. If the mutex is valid, this function will always
 * block until it can lock the mutex, and return with it locked.
 *
 * \param mutex the mutex to lock.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_TryLockMutex
 * \sa SDL_UnlockMutex
 */
extern SDL_DECLSPEC void SDLCALL SDL_LockMutex(SDL_Mutex *mutex) SDL_ACQUIRE(mutex);

/**
 * Try to lock a mutex without blocking.
 *
 * This works just like SDL_LockMutex(), but if the mutex is not available,
 * this function returns `SDL_MUTEX_TIMEDOUT` immediately.
 *
 * This technique is useful if you need exclusive access to a resource but
 * don't want to wait for it, and will return to it to try again later.
 *
 * This function does not fail; if mutex is NULL, it will return 0 immediately
 * having locked nothing. If the mutex is valid, this function will always
 * either lock the mutex and return 0, or return SDL_MUTEX_TIMEOUT and lock
 * nothing.
 *
 * \param mutex the mutex to try to lock.
 * \returns 0 or `SDL_MUTEX_TIMEDOUT`.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockMutex
 * \sa SDL_UnlockMutex
 */
extern SDL_DECLSPEC int SDLCALL SDL_TryLockMutex(SDL_Mutex *mutex) SDL_TRY_ACQUIRE(0, mutex);

/**
 * Unlock the mutex.
 *
 * It is legal for the owning thread to lock an already-locked mutex. It must
 * unlock it the same number of times before it is actually made available for
 * other threads in the system (this is known as a "recursive mutex").
 *
 * It is illegal to unlock a mutex that has not been locked by the current
 * thread, and doing so results in undefined behavior.
 *
 * \param mutex the mutex to unlock.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockMutex
 * \sa SDL_TryLockMutex
 */
extern SDL_DECLSPEC void SDLCALL SDL_UnlockMutex(SDL_Mutex *mutex) SDL_RELEASE(mutex);

/**
 * Destroy a mutex created with SDL_CreateMutex().
 *
 * This function must be called on any mutex that is no longer needed. Failure
 * to destroy a mutex will result in a system memory or resource leak. While
 * it is safe to destroy a mutex that is _unlocked_, it is not safe to attempt
 * to destroy a locked mutex, and may result in undefined behavior depending
 * on the platform.
 *
 * \param mutex the mutex to destroy.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateMutex
 */
extern SDL_DECLSPEC void SDLCALL SDL_DestroyMutex(SDL_Mutex *mutex);

/* @} *//* Mutex functions */


/**
 *  \name Read/write lock functions
 */
/* @{ */

/**
 * A mutex that allows read-only threads to run in parallel.
 *
 * A rwlock is roughly the same concept as SDL_Mutex, but allows threads that
 * request read-only access to all hold the lock at the same time. If a thread
 * requests write access, it will block until all read-only threads have
 * released the lock, and no one else can hold the thread (for reading or
 * writing) at the same time as the writing thread.
 *
 * This can be more efficient in cases where several threads need to access
 * data frequently, but changes to that data are rare.
 *
 * There are other rules that apply to rwlocks that don't apply to mutexes,
 * about how threads are scheduled and when they can be recursively locked.
 * These are documented in the other rwlock functions.
 *
 * \since This struct is available since SDL 3.0.0.
 */
typedef struct SDL_RWLock SDL_RWLock;

/*
 * Synchronization functions return this value if they time out.
 *
 * Not all functions _can_ time out; some will block indefinitely.
 *
 * This symbol is just for clarity when dealing with SDL_RWLock
 * functions; its value is equivalent to SDL_MUTEX_TIMEOUT.
 *
 * \since This macro is available since SDL 3.0.0.
 */
#define SDL_RWLOCK_TIMEDOUT SDL_MUTEX_TIMEDOUT


/**
 * Create a new read/write lock.
 *
 * A read/write lock is useful for situations where you have multiple threads
 * trying to access a resource that is rarely updated. All threads requesting
 * a read-only lock will be allowed to run in parallel; if a thread requests a
 * write lock, it will be provided exclusive access. This makes it safe for
 * multiple threads to use a resource at the same time if they promise not to
 * change it, and when it has to be changed, the rwlock will serve as a
 * gateway to make sure those changes can be made safely.
 *
 * In the right situation, a rwlock can be more efficient than a mutex, which
 * only lets a single thread proceed at a time, even if it won't be modifying
 * the data.
 *
 * All newly-created read/write locks begin in the _unlocked_ state.
 *
 * Calls to SDL_LockRWLockForReading() and SDL_LockRWLockForWriting will not
 * return while the rwlock is locked _for writing_ by another thread. See
 * SDL_TryLockRWLockForReading() and SDL_TryLockRWLockForWriting() to attempt
 * to lock without blocking.
 *
 * SDL read/write locks are only recursive for read-only locks! They are not
 * guaranteed to be fair, or provide access in a FIFO manner! They are not
 * guaranteed to favor writers. You may not lock a rwlock for both read-only
 * and write access at the same time from the same thread (so you can't
 * promote your read-only lock to a write lock without unlocking first).
 *
 * \returns the initialized and unlocked read/write lock or NULL on failure;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroyRWLock
 * \sa SDL_LockRWLockForReading
 * \sa SDL_LockRWLockForWriting
 * \sa SDL_TryLockRWLockForReading
 * \sa SDL_TryLockRWLockForWriting
 * \sa SDL_UnlockRWLock
 */
extern SDL_DECLSPEC SDL_RWLock * SDLCALL SDL_CreateRWLock(void);

/**
 * Lock the read/write lock for _read only_ operations.
 *
 * This will block until the rwlock is available, which is to say it is not
 * locked for writing by any other thread. Of all threads waiting to lock the
 * rwlock, all may do so at the same time as long as they are requesting
 * read-only access; if a thread wants to lock for writing, only one may do so
 * at a time, and no other threads, read-only or not, may hold the lock at the
 * same time.
 *
 * It is legal for the owning thread to lock an already-locked rwlock for
 * reading. It must unlock it the same number of times before it is actually
 * made available for other threads in the system (this is known as a
 * "recursive rwlock").
 *
 * Note that locking for writing is not recursive (this is only available to
 * read-only locks).
 *
 * It is illegal to request a read-only lock from a thread that already holds
 * the write lock. Doing so results in undefined behavior. Unlock the write
 * lock before requesting a read-only lock. (But, of course, if you have the
 * write lock, you don't need further locks to read in any case.)
 *
 * This function does not fail; if rwlock is NULL, it will return immediately
 * having locked nothing. If the rwlock is valid, this function will always
 * block until it can lock the mutex, and return with it locked.
 *
 * \param rwlock the read/write lock to lock.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockRWLockForWriting
 * \sa SDL_TryLockRWLockForReading
 * \sa SDL_UnlockRWLock
 */
extern SDL_DECLSPEC void SDLCALL SDL_LockRWLockForReading(SDL_RWLock *rwlock) SDL_ACQUIRE_SHARED(rwlock);

/**
 * Lock the read/write lock for _write_ operations.
 *
 * This will block until the rwlock is available, which is to say it is not
 * locked for reading or writing by any other thread. Only one thread may hold
 * the lock when it requests write access; all other threads, whether they
 * also want to write or only want read-only access, must wait until the
 * writer thread has released the lock.
 *
 * It is illegal for the owning thread to lock an already-locked rwlock for
 * writing (read-only may be locked recursively, writing can not). Doing so
 * results in undefined behavior.
 *
 * It is illegal to request a write lock from a thread that already holds a
 * read-only lock. Doing so results in undefined behavior. Unlock the
 * read-only lock before requesting a write lock.
 *
 * This function does not fail; if rwlock is NULL, it will return immediately
 * having locked nothing. If the rwlock is valid, this function will always
 * block until it can lock the mutex, and return with it locked.
 *
 * \param rwlock the read/write lock to lock.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockRWLockForReading
 * \sa SDL_TryLockRWLockForWriting
 * \sa SDL_UnlockRWLock
 */
extern SDL_DECLSPEC void SDLCALL SDL_LockRWLockForWriting(SDL_RWLock *rwlock) SDL_ACQUIRE(rwlock);

/**
 * Try to lock a read/write lock _for reading_ without blocking.
 *
 * This works just like SDL_LockRWLockForReading(), but if the rwlock is not
 * available, then this function returns `SDL_RWLOCK_TIMEDOUT` immediately.
 *
 * This technique is useful if you need access to a resource but don't want to
 * wait for it, and will return to it to try again later.
 *
 * Trying to lock for read-only access can succeed if other threads are
 * holding read-only locks, as this won't prevent access.
 *
 * This function does not fail; if rwlock is NULL, it will return 0
 * immediately having locked nothing. If rwlock is valid, this function will
 * always either lock the rwlock and return 0, or return SDL_RWLOCK_TIMEOUT
 * and lock nothing.
 *
 * \param rwlock the rwlock to try to lock.
 * \returns 0 or `SDL_RWLOCK_TIMEDOUT`.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockRWLockForReading
 * \sa SDL_TryLockRWLockForWriting
 * \sa SDL_UnlockRWLock
 */
extern SDL_DECLSPEC int SDLCALL SDL_TryLockRWLockForReading(SDL_RWLock *rwlock) SDL_TRY_ACQUIRE_SHARED(0, rwlock);

/**
 * Try to lock a read/write lock _for writing_ without blocking.
 *
 * This works just like SDL_LockRWLockForWriting(), but if the rwlock is not
 * available, this function returns `SDL_RWLOCK_TIMEDOUT` immediately.
 *
 * This technique is useful if you need exclusive access to a resource but
 * don't want to wait for it, and will return to it to try again later.
 *
 * It is illegal for the owning thread to lock an already-locked rwlock for
 * writing (read-only may be locked recursively, writing can not). Doing so
 * results in undefined behavior.
 *
 * It is illegal to request a write lock from a thread that already holds a
 * read-only lock. Doing so results in undefined behavior. Unlock the
 * read-only lock before requesting a write lock.
 *
 * This function does not fail; if rwlock is NULL, it will return 0
 * immediately having locked nothing. If rwlock is valid, this function will
 * always either lock the rwlock and return 0, or return SDL_RWLOCK_TIMEOUT
 * and lock nothing.
 *
 * \param rwlock the rwlock to try to lock.
 * \returns 0 or `SDL_RWLOCK_TIMEDOUT`.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockRWLockForWriting
 * \sa SDL_TryLockRWLockForReading
 * \sa SDL_UnlockRWLock
 */
extern SDL_DECLSPEC int SDLCALL SDL_TryLockRWLockForWriting(SDL_RWLock *rwlock) SDL_TRY_ACQUIRE(0, rwlock);

/**
 * Unlock the read/write lock.
 *
 * Use this function to unlock the rwlock, whether it was locked for read-only
 * or write operations.
 *
 * It is legal for the owning thread to lock an already-locked read-only lock.
 * It must unlock it the same number of times before it is actually made
 * available for other threads in the system (this is known as a "recursive
 * rwlock").
 *
 * It is illegal to unlock a rwlock that has not been locked by the current
 * thread, and doing so results in undefined behavior.
 *
 * \param rwlock the rwlock to unlock.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockRWLockForReading
 * \sa SDL_LockRWLockForWriting
 * \sa SDL_TryLockRWLockForReading
 * \sa SDL_TryLockRWLockForWriting
 */
extern SDL_DECLSPEC void SDLCALL SDL_UnlockRWLock(SDL_RWLock *rwlock) SDL_RELEASE_GENERIC(rwlock);

/**
 * Destroy a read/write lock created with SDL_CreateRWLock().
 *
 * This function must be called on any read/write lock that is no longer
 * needed. Failure to destroy a rwlock will result in a system memory or
 * resource leak. While it is safe to destroy a rwlock that is _unlocked_, it
 * is not safe to attempt to destroy a locked rwlock, and may result in
 * undefined behavior depending on the platform.
 *
 * \param rwlock the rwlock to destroy.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateRWLock
 */
extern SDL_DECLSPEC void SDLCALL SDL_DestroyRWLock(SDL_RWLock *rwlock);

/* @} *//* Read/write lock functions */


/**
 *  \name Semaphore functions
 */
/* @{ */

/**
 * A means to manage access to a resource, by count, between threads.
 *
 * Semaphores (specifically, "counting semaphores"), let X number of threads
 * request access at the same time, each thread granted access decrementing a
 * counter. When the counter reaches zero, future requests block until a prior
 * thread releases their request, incrementing the counter again.
 *
 * Wikipedia has a thorough explanation of the concept:
 *
 * https://en.wikipedia.org/wiki/Semaphore_(programming)
 *
 * \since This struct is available since SDL 3.0.0.
 */
typedef struct SDL_Semaphore SDL_Semaphore;

/**
 * Create a semaphore.
 *
 * This function creates a new semaphore and initializes it with the value
 * `initial_value`. Each wait operation on the semaphore will atomically
 * decrement the semaphore value and potentially block if the semaphore value
 * is 0. Each post operation will atomically increment the semaphore value and
 * wake waiting threads and allow them to retry the wait operation.
 *
 * \param initial_value the starting value of the semaphore.
 * \returns a new semaphore or NULL on failure; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroySemaphore
 * \sa SDL_SignalSemaphore
 * \sa SDL_TryWaitSemaphore
 * \sa SDL_GetSemaphoreValue
 * \sa SDL_WaitSemaphore
 * \sa SDL_WaitSemaphoreTimeout
 */
extern SDL_DECLSPEC SDL_Semaphore * SDLCALL SDL_CreateSemaphore(Uint32 initial_value);

/**
 * Destroy a semaphore.
 *
 * It is not safe to destroy a semaphore if there are threads currently
 * waiting on it.
 *
 * \param sem the semaphore to destroy.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateSemaphore
 */
extern SDL_DECLSPEC void SDLCALL SDL_DestroySemaphore(SDL_Semaphore *sem);

/**
 * Wait until a semaphore has a positive value and then decrements it.
 *
 * This function suspends the calling thread until either the semaphore
 * pointed to by `sem` has a positive value or the call is interrupted by a
 * signal or error. If the call is successful it will atomically decrement the
 * semaphore value.
 *
 * This function is the equivalent of calling SDL_WaitSemaphoreTimeout() with
 * a time length of -1.
 *
 * \param sem the semaphore wait on.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SignalSemaphore
 * \sa SDL_TryWaitSemaphore
 * \sa SDL_WaitSemaphoreTimeout
 */
extern SDL_DECLSPEC int SDLCALL SDL_WaitSemaphore(SDL_Semaphore *sem);

/**
 * See if a semaphore has a positive value and decrement it if it does.
 *
 * This function checks to see if the semaphore pointed to by `sem` has a
 * positive value and atomically decrements the semaphore value if it does. If
 * the semaphore doesn't have a positive value, the function immediately
 * returns SDL_MUTEX_TIMEDOUT.
 *
 * \param sem the semaphore to wait on.
 * \returns 0 if the wait succeeds, `SDL_MUTEX_TIMEDOUT` if the wait would
 *          block, or a negative error code on failure; call SDL_GetError()
 *          for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SignalSemaphore
 * \sa SDL_WaitSemaphore
 * \sa SDL_WaitSemaphoreTimeout
 */
extern SDL_DECLSPEC int SDLCALL SDL_TryWaitSemaphore(SDL_Semaphore *sem);

/**
 * Wait until a semaphore has a positive value and then decrements it.
 *
 * This function suspends the calling thread until either the semaphore
 * pointed to by `sem` has a positive value, the call is interrupted by a
 * signal or error, or the specified time has elapsed. If the call is
 * successful it will atomically decrement the semaphore value.
 *
 * \param sem the semaphore to wait on.
 * \param timeoutMS the length of the timeout, in milliseconds.
 * \returns 0 if the wait succeeds, `SDL_MUTEX_TIMEDOUT` if the wait does not
 *          succeed in the allotted time, or a negative error code on failure;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SignalSemaphore
 * \sa SDL_TryWaitSemaphore
 * \sa SDL_WaitSemaphore
 */
extern SDL_DECLSPEC int SDLCALL SDL_WaitSemaphoreTimeout(SDL_Semaphore *sem, Sint32 timeoutMS);

/**
 * Atomically increment a semaphore's value and wake waiting threads.
 *
 * \param sem the semaphore to increment.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_TryWaitSemaphore
 * \sa SDL_WaitSemaphore
 * \sa SDL_WaitSemaphoreTimeout
 */
extern SDL_DECLSPEC int SDLCALL SDL_SignalSemaphore(SDL_Semaphore *sem);

/**
 * Get the current value of a semaphore.
 *
 * \param sem the semaphore to query.
 * \returns the current value of the semaphore.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC Uint32 SDLCALL SDL_GetSemaphoreValue(SDL_Semaphore *sem);

/* @} *//* Semaphore functions */


/**
 *  \name Condition variable functions
 */
/* @{ */

/**
 * A means to block multiple threads until a condition is satisfied.
 *
 * Condition variables, paired with an SDL_Mutex, let an app halt multiple
 * threads until a condition has occurred, at which time the app can release
 * one or all waiting threads.
 *
 * Wikipedia has a thorough explanation of the concept:
 *
 * https://en.wikipedia.org/wiki/Condition_variable
 *
 * \since This struct is available since SDL 3.0.0.
 */
typedef struct SDL_Condition SDL_Condition;

/**
 * Create a condition variable.
 *
 * \returns a new condition variable or NULL on failure; call SDL_GetError()
 *          for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BroadcastCondition
 * \sa SDL_SignalCondition
 * \sa SDL_WaitCondition
 * \sa SDL_WaitConditionTimeout
 * \sa SDL_DestroyCondition
 */
extern SDL_DECLSPEC SDL_Condition * SDLCALL SDL_CreateCondition(void);

/**
 * Destroy a condition variable.
 *
 * \param cond the condition variable to destroy.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateCondition
 */
extern SDL_DECLSPEC void SDLCALL SDL_DestroyCondition(SDL_Condition *cond);

/**
 * Restart one of the threads that are waiting on the condition variable.
 *
 * \param cond the condition variable to signal.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BroadcastCondition
 * \sa SDL_WaitCondition
 * \sa SDL_WaitConditionTimeout
 */
extern SDL_DECLSPEC int SDLCALL SDL_SignalCondition(SDL_Condition *cond);

/**
 * Restart all threads that are waiting on the condition variable.
 *
 * \param cond the condition variable to signal.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SignalCondition
 * \sa SDL_WaitCondition
 * \sa SDL_WaitConditionTimeout
 */
extern SDL_DECLSPEC int SDLCALL SDL_BroadcastCondition(SDL_Condition *cond);

/**
 * Wait until a condition variable is signaled.
 *
 * This function unlocks the specified `mutex` and waits for another thread to
 * call SDL_SignalCondition() or SDL_BroadcastCondition() on the condition
 * variable `cond`. Once the condition variable is signaled, the mutex is
 * re-locked and the function returns.
 *
 * The mutex must be locked before calling this function. Locking the mutex
 * recursively (more than once) is not supported and leads to undefined
 * behavior.
 *
 * This function is the equivalent of calling SDL_WaitConditionTimeout() with
 * a time length of -1.
 *
 * \param cond the condition variable to wait on.
 * \param mutex the mutex used to coordinate thread access.
 * \returns 0 when it is signaled or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BroadcastCondition
 * \sa SDL_SignalCondition
 * \sa SDL_WaitConditionTimeout
 */
extern SDL_DECLSPEC int SDLCALL SDL_WaitCondition(SDL_Condition *cond, SDL_Mutex *mutex);

/**
 * Wait until a condition variable is signaled or a certain time has passed.
 *
 * This function unlocks the specified `mutex` and waits for another thread to
 * call SDL_SignalCondition() or SDL_BroadcastCondition() on the condition
 * variable `cond`, or for the specified time to elapse. Once the condition
 * variable is signaled or the time elapsed, the mutex is re-locked and the
 * function returns.
 *
 * The mutex must be locked before calling this function. Locking the mutex
 * recursively (more than once) is not supported and leads to undefined
 * behavior.
 *
 * \param cond the condition variable to wait on.
 * \param mutex the mutex used to coordinate thread access.
 * \param timeoutMS the maximum time to wait, in milliseconds, or -1 to wait
 *                  indefinitely.
 * \returns 0 if the condition variable is signaled, `SDL_MUTEX_TIMEDOUT` if
 *          the condition is not signaled in the allotted time, or a negative
 *          error code on failure; call SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BroadcastCondition
 * \sa SDL_SignalCondition
 * \sa SDL_WaitCondition
 */
extern SDL_DECLSPEC int SDLCALL SDL_WaitConditionTimeout(SDL_Condition *cond,
                                                SDL_Mutex *mutex, Sint32 timeoutMS);

/* @} *//* Condition variable functions */


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_mutex_h_ */
