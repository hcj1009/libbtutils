#ifndef _INTERLOCKED_H_
#define _INTERLOCKED_H_

#include "utypes.h" // for LONG

// GCC 4.3 (and before) define macro _GLIBCXX_ATOMIC_BUILTINS
// GCC 4.4 and after define macros _GLIBCXX_ATOMIC_BUILTINS_[1|2|4|8] and don't define _GLIBCXX_ATOMIC_BUILTINS
// 2013 02 13 - updates on newer gcc versions
// GCC 4.6, from looking at compiler output logs, looks like it behaves like gcc 4.3 and before
// GCC 4.7 defines macro _GLIBCXX_ATOMIC_BUILTINS, and not the _[1|2|4|8] ones

// AVOID_GLIBCXX_ATOMIC_BUILTINS macro defined on compiler command line
// originating from makefile if the system libraries on the target platform
// do not correctly implement the gcc atomic memory access functions
// (see http://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html)
// This could be the case for some embedded devices.

#if ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7) || (__GNUC__ == 4 && __GNUC_MINOR__ <= 3)) && defined(_GLIBCXX_ATOMIC_BUILTINS) && !defined AVOID_GLIBCXX_ATOMIC_BUILTINS
// Case for gcc 4.7 and later, and gcc 4.3 and before
#define USE_GLIBCXX_ATOMIC_BUILTINS 1
#elif _GLIBCXX_ATOMIC_BUILTINS_4 && ((!defined _LP64) || _GLIBCXX_ATOMIC_BUILTINS_8) && !defined AVOID_GLIBCXX_ATOMIC_BUILTINS
// Case for gcc 4.4 - 4.6
#define USE_GLIBCXX_ATOMIC_BUILTINS 1
#else
// Roll our own atomic operations
#define USE_GLIBCXX_ATOMIC_BUILTINS 0
#endif

#ifdef WIN32

#include <windows.h>

#ifndef WIN_RT
#define _M_CEE_PURE
#endif	// WIN_RT

#include <intrin.h>
#include <assert.h>

#ifdef _DEBUG
inline LONG InterlockedAdd(LONG *ptr, LONG value) {
	assert((((ULONG_PTR)ptr) & (sizeof(LONG)-1)) == 0);
	return InterlockedExchangeAdd(ptr, value);
}
inline LONG MyInterlockedExchange(LONG *ptr, LONG value) {
	assert((((ULONG_PTR)ptr) & (sizeof(LONG)-1)) == 0);
	return InterlockedExchange(ptr, value);
}
inline void *MyInterlockedExchangePointer(void **ptr, void *value) {
	assert((((ULONG_PTR)ptr) & (sizeof(void *)-1)) == 0);
	return InterlockedExchangePointer(ptr, value);
}

inline void *MyInterlockedCompareExchangePointer(void **ptr, void *newvalue, void *oldvalue) {
	assert((((ULONG_PTR)ptr) & (sizeof(void *)-1)) == 0);
	return InterlockedCompareExchangePointer(ptr, newvalue, oldvalue);
}
#undef InterlockedExchange
#define InterlockedExchange MyInterlockedExchange
#undef InterlockedExchangePointer
#define InterlockedExchangePointer MyInterlockedExchangePointer
#undef InterlockedCompareExchangePointer
#define InterlockedCompareExchangePointer MyInterlockedCompareExchangePointer
#else
#define InterlockedAdd InterlockedExchangeAdd
#endif

#elif defined (__APPLE__)

#include <libkern/OSAtomic.h>

extern OSSpinLock g_interlocked_spin;

inline LONG InterlockedAdd(LONG * ptr, LONG value) {
	return OSAtomicAdd32Barrier((int32_t)value, (int32_t*)ptr);
}

#ifdef _LP64
inline long long int InterlockedAdd(long long int * ptr, long long int value) {
	return OSAtomicAdd64Barrier(value, ptr);
}
#endif

inline LONG InterlockedIncrement(LONG * ptr) {
	return OSAtomicIncrement32Barrier((int32_t*)ptr);
}

inline LONG InterlockedDecrement(LONG * ptr) {
	return OSAtomicDecrement32Barrier((int32_t*)ptr);
}

inline LONG InterlockedExchange(LONG *ptr, LONG value) {
	OSSpinLockLock(&g_interlocked_spin);
	LONG old = *ptr;
	*ptr = value;
	OSSpinLockUnlock(&g_interlocked_spin);
	return old;
}

inline void *InterlockedExchangePointer(void **ptr, void *value) {
	OSSpinLockLock(&g_interlocked_spin);
	void *old = *ptr;
	*ptr = value;
	OSSpinLockUnlock(&g_interlocked_spin);
	return old;
}

inline void *InterlockedCompareExchangePointer(void **ptr, void *newvalue, void *oldvalue) {
	OSSpinLockLock(&g_interlocked_spin);
	void *old = *ptr;
	if (old == oldvalue) *ptr = newvalue;
	OSSpinLockUnlock(&g_interlocked_spin);
	return old;
}

#elif USE_GLIBCXX_ATOMIC_BUILTINS

inline LONG InterlockedAdd(LONG* ptr, LONG value) {
	return __sync_add_and_fetch(ptr, value);
}

#ifdef _LP64
inline long long int InterlockedAdd(long long int * ptr, long long int value) {
	return __sync_add_and_fetch(ptr, value);
}
#endif // _LP64

inline LONG InterlockedIncrement(LONG* ptr) {
	return __sync_add_and_fetch(ptr, 1);
}

#ifdef _LP64
inline long long int InterlockedIncrement(long long int * ptr) {
	return __sync_add_and_fetch(ptr, 1);
}
#endif // _LP64

inline LONG InterlockedDecrement(LONG* ptr) {
	return __sync_sub_and_fetch(ptr, 1);
}

#ifdef _LP64
inline long long int InterlockedDecrement(long long int * ptr) {
	return __sync_sub_and_fetch(ptr, 1);
}
#endif // _LP64

inline LONG InterlockedExchange(LONG *ptr, LONG value) {
	return __sync_lock_test_and_set(ptr, value);
}

#ifdef _LP64
inline LONG InterlockedExchange(long long int *ptr, long long int value) {
	return __sync_lock_test_and_set(ptr, value);
}
#endif // _LP64

inline void *InterlockedExchangePointer(void **ptr, void *value) {
	return __sync_lock_test_and_set(ptr, value);
}

inline void *InterlockedCompareExchangePointer(void **ptr, void *newvalue, void *oldvalue) {
	return __sync_val_compare_and_swap(ptr, oldvalue, newvalue);
}

#else

#pragma message ("Using single mutex for synchronizing all atomic operations")

#include <pthread.h>

extern pthread_mutex_t g_interlocked_mutex = PTHREAD_MUTEX_INITIALIZER;

inline LONG InterlockedAdd(LONG* ptr, LONG value) {
	LONG rval;
	pthread_mutex_lock(&g_interlocked_mutex);
	*ptr += value;
	rval=*ptr;
	pthread_mutex_unlock(&g_interlocked_mutex);
	return rval;
}

#ifdef _LP64
inline long long int InterlockedAdd(long long int * ptr, long long int value) {
	long long int rval;
	pthread_mutex_lock(&g_interlocked_mutex);
	*ptr += value;
	rval = *ptr;
	pthread_mutex_unlock(&g_interlocked_mutex);
	return rval;
}
#endif // _LP64

inline LONG InterlockedIncrement(LONG* ptr) {
	LONG rval;
	pthread_mutex_lock(&g_interlocked_mutex);
	*ptr += 1;
	rval=*ptr;
	pthread_mutex_unlock(&g_interlocked_mutex);
	return rval;
}

#ifdef _LP64
inline long long int InterlockedIncrement(long long int * ptr) {
	return __sync_add_and_fetch(ptr, 1);
}
#endif // _LP64

inline LONG InterlockedDecrement(LONG* ptr) {
	return __sync_sub_and_fetch(ptr, 1);
}

#ifdef _LP64
inline long long int InterlockedDecrement(long long int * ptr) {
	long long int rval;
	pthread_mutex_lock(&g_interlocked_mutex);
	*ptr -= 1;
	rval = *ptr;
	pthread_mutex_unlock(&g_interlocked_mutex);
	return rval;
}
#endif // _LP64

inline LONG InterlockedExchange(LONG *ptr, LONG value) {
	LONG res;
	pthread_mutex_lock(&g_interlocked_mutex);
	res = *ptr;
	*ptr = value;
	pthread_mutex_unlock(&g_interlocked_mutex);
	return res;
}

#ifdef _LP64
inline LONG InterlockedExchange(long long int *ptr, long long int value) {
	long long int res;
	pthread_mutex_lock(&g_interlocked_mutex);
	res = *ptr;
	*ptr = value;
	pthread_mutex_unlock(&g_interlocked_mutex);
	return res;
}
#endif // _LP64

inline void *InterlockedExchangePointer(void **ptr, void *value) {
	void * res;
	pthread_mutex_lock(&g_interlocked_mutex);
	res = *ptr;
	*ptr = value;
	pthread_mutex_unlock(&g_interlocked_mutex);
	return res;
}

inline void *InterlockedCompareExchangePointer(void **ptr, void *newvalue, void *oldvalue) {
	void *result;
	pthread_mutex_lock(&g_interlocked_mutex);
	result = *ptr;
	if (result == oldvalue) *ptr = newvalue;
	pthread_mutex_unlock(&g_interlocked_mutex);
	return result;
}

#endif

/* TODO: enable gcc __i386__ version after testing it */
#if 0 // defined (__GNUC__) && defined (__i386__)

inline void InterlockedAdd(LONG *ptr, LONG value) {
	asm volatile ("lock addl %1,(%0)"
		      : : "r" (ptr), "r" (value)  :);

}

inline LONG InterlockedExchange(LONG *ptr, LONG value) {
	LONG retval;
	asm volatile ("movl  (%1), %0\n"
	     "0: \n"
	     "        lock cmpxchgl  %2, (%1)\n"
	     "        jnz    0b\n"
	      : "=&a" (retval) : "r" (ptr), "r" (value) : );
	return retval;
}
#endif

#if 0 // defined (__GNUC__) && defined (__x86_64__)

inline void InterlockedAdd(LONG *ptr, LONG value) {
	asm volatile ("lock addq %1,(%0)"
		      : : "r" (ptr), "r" (value) :);
}

inline LONG InterlockedExchange(LONG *ptr, LONG value) {
	LONG retval;
	asm volatile ("movq  (%1), %0\n"
	     "0: \n"
	     "        lock cmpxchgq  %2, (%1)\n"
	     "        jnz    0b\n"
	: "=&a" (retval) : "r" (ptr), "r" (value) : );
	return retval;
}

#endif

#endif//_INTERLOCKED_H_