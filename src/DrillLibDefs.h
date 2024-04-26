#pragma once

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

// Use FINLINE for small functions that are called often and probably don't need to be stepped into for debugging
// Usually used in conjunction with DEBUG_OPTIMIZE macros to make sure they're optimized even in debug mode
// For things I've tested with, this can result in an order of magnitude of speed improvement (though still not as good as release mode)
#define FINLINE __inline __forceinline
#ifndef NDEBUG
// t means optimize for speed
#define DEBUG_OPTIMIZE_ON __pragma(optimize("t", on))
// pragma optimize with a blank string means default
#define DEBUG_OPTIMIZE_OFF __pragma(optimize("", on))
#else
#define DEBUG_OPTIMIZE_ON
#define DEBUG_OPTIMIZE_OFF
#endif
#define NODISCARD [[nodiscard]]

#define KILOBYTE 1024
#define MEGABYTE (KILOBYTE * 1024)
#define GIGABYTE (MEGABYTE * 1024)
#define TERABYTE (U64(GIGABYTE) * 1024ull)

// Standard 4k page
#define PAGE_SIZE 4096

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
#define ALIGN_LOW(num, alignment) ((num) & ~((alignment) - 1))
#define ALIGN_HIGH(num, alignment) (((num) + (static_cast<decltype(num)>(alignment) - 1)) & ~(static_cast<decltype(num)>(alignment) - 1))
#define OFFSET_OF(type, member) __builtin_offsetof(type, member) //(reinterpret_cast<uptr>(&reinterpret_cast<type*>(uptr(0))->member))

// Suppress "hides previous local declaration", intended behavior for this construct
#define DEFER_LOOP(before, after) __pragma(warning(suppress : 4456))\
	for (U32 DEFER_LOOP_I = ((before), 0u); DEFER_LOOP_I == 0u; DEFER_LOOP_I++, (after))

#define LOG_TIME(str) __pragma(warning(suppress : 4456))\
	for (F64 LOG_TIME_TIME = current_time_seconds(), LOG_TIME_I = 0.0; LOG_TIME_I == 0.0; print(str), println_float(F32(current_time_seconds() - LOG_TIME_TIME)), LOG_TIME_I = 1.0)

// DLL because I don't want to type DOUBLY_LINKED_LIST every time
#define DLL_INSERT_HEAD(item, head, tail, prev, next) ((item)->prev = nullptr, (item)->next = (head), (head) ? (head)->prev = (item) : nullptr, (head) = (item), (tail) = (tail) ? (tail) : (item))
#define DLL_INSERT_TAIL(item, head, tail, prev, next) ((item)->next = nullptr, (item)->prev = (tail), (tail) ? (tail)->next = (item) : nullptr, (tail) = (item), (head) = (head) ? (head) : (item))
#define DLL_REMOVE(item, head, tail, prev, next) (((item) == (head) ? (head) = (item)->next : nullptr), ((item) == (tail) ? (tail) = (item)->prev : nullptr), ((item)->next ? (item)->next->prev = (item)->prev : nullptr), ((item)->prev ? (item)->prev->next = (item)->next : nullptr), (item)->next = nullptr, (item)->prev = nullptr)
#define DLL_REPLACE(item, other, head, tail, prev, next) ((other)->prev = (item)->prev, (other)->next = (item)->next, ((item) == (head) ? (head) = (other) : nullptr), ((item) == (tail) ? (tail) = (other) : nullptr), ((item)->next ? (item)->next->prev = (other) : nullptr), ((item)->prev ? (item)->prev->next = (other) : nullptr), (item)->next = nullptr, (item)->prev = nullptr)

#define ASSERT(cond, msg) if(!(cond)) { abort(msg); }
#ifndef NDEBUG
#define DEBUG_ASSERT(cond, msg) if(!(cond)) { __debugbreak(); abort(msg); }
#else
#define DEBUG_ASSERT
#endif

#define MATH_PI 3.1415926535F
#define DEG_TO_RAD(x) ((x) * (MATH_PI / 180.0F))
#define RAD_TO_DEG(x) ((x) * (180.0F / MATH_PI))
#define TURN_TO_RAD(x) ((x) * (2.0F * MATH_PI))
#define RAD_TO_TURN(x) ((x) * (1.0F / (2.0F*MATH_PI)))
#define DEG_TO_TURN(x) ((x) * (1.0F / 360.0F))
#define TURN_TO_DEG(x) ((x) * 360.0F)
#define MILLISECOND_TO_NANOSECOND(milliseconds) ((milliseconds) * 1000000)
// Window reference time is in units of 100 nanoseconds
#define MILLISECOND_TO_REFERENCE_TIME(milliseconds) ((milliseconds) * 10000)
#define REFERENCE_TIME_TO_NANOSECOND(refTime) ((refTime) * 100)
#define REFERENCE_TIME_TO_MICROSECOND(refTime) ((refTime) / 10)
#define REFERENCE_TIME_TO_MILLISECOND(refTime) ((refTime) / 10000)

#define PX_TO_MILLIMETER(px) ((px) * 0.2645833333F)
#define MILLIMETER_TO_PX(mm) ((mm) * 3.7795275591F)

#define DRILL_LIB_MAKE_VERSION(major, minor, patch) ((((major) & 0b1111111111) << 20) | (((minor) & 0b1111111111) << 10) | ((patch) & 0b1111111111))

#define DRILL_LIB_VERSION DRILL_LIB_MAKE_VERSION(1, 2, 0)

typedef signed __int8 I8;
typedef unsigned __int8 U8;
typedef signed __int16 I16;
typedef unsigned __int16 U16;
typedef signed __int32 I32;
typedef unsigned __int32 U32;
typedef signed __int64 I64;
typedef unsigned __int64 U64;
typedef U64 UPtr;
typedef U8 Byte;
typedef float F32;
typedef double F64;
typedef U8 B8;
typedef U32 B32;
typedef U32 Flags32;

#define U8_MAX 0xFF
#define U16_MAX 0xFFFF
#define U32_MAX 0xFFFFFFFF
#define U64_MAX 0xFFFFFFFFFFFFFFFFULL
#define I8_MAX 0x7F
#define I16_MAX 0x7FFF
#define I32_MAX 0x7FFFFFFF
#define I64_MAX 0x7FFFFFFFFFFFFFFFULL
#define I8_MIN I8(0x80)
#define I16_MIN I16(0x8000)
#define I32_MIN I32(0x80000000)
#define I64_MIN I64(0x8000000000000000LL)
#define F32_SMALL (__builtin_bit_cast(F32, 0x00800000u))
#define F32_LARGE (__builtin_bit_cast(F32, 0x7F7FFFFFu))
#define F32_INF (__builtin_bit_cast(F32, 0x7F800000u))
#define F32_QNAN (__builtin_bit_cast(F32, 0x7FFFFFFFu))
#define F32_SNAN (__builtin_bit_cast(F32, 0x7FBFFFFFu))
#define F64_SMALL (__builtin_bit_cast(F64, 0x0010000000000000ull))
#define F64_LARGE (__builtin_bit_cast(F64, 0x7FEFFFFFFFFFFFFFull))
#define F64_INF (__builtin_bit_cast(F64, 0x7FF0000000000000ull))
#define F64_QNAN (__builtin_bit_cast(F64, 0x7FFFFFFFFFFFFFFFull))
#define F64_SNAN (__builtin_bit_cast(F64, 0x7FF7FFFFFFFFFFFFull))

#define DRILL_LIB_REDECLARE_STDINT
#ifdef DRILL_LIB_REDECLARE_STDINT
typedef I8 int8_t;
typedef U8 uint8_t;
typedef I16 int16_t;
typedef U16 uint16_t;
typedef I32 int32_t;
typedef U32 uint32_t;
typedef I64 int64_t;
typedef U64 uint64_t;
typedef UPtr uintptr_t;
#endif

DEBUG_OPTIMIZE_ON

FINLINE U16 load_le16(void* ptr) {
	U16 result;
	memcpy(&result, ptr, sizeof(U16));
	return result;
}
FINLINE U32 load_le32(void* ptr) {
	U32 result;
	memcpy(&result, ptr, sizeof(U32));
	return result;
}
FINLINE U64 load_le64(void* ptr) {
	U64 result;
	memcpy(&result, ptr, sizeof(U64));
	return result;
}
FINLINE void store_le16(void* ptr, U16 val) {
	memcpy(ptr, &val, sizeof(U16));
}
FINLINE void store_le32(void* ptr, U32 val) {
	memcpy(ptr, &val, sizeof(U32));
}
FINLINE void store_le64(void* ptr, U64 val) {
	memcpy(ptr, &val, sizeof(U64));
}

DEBUG_OPTIMIZE_OFF

#define LOAD_LE8(ptr) (*reinterpret_cast<U8*>(ptr))
#define LOAD_LE16(ptr) (load_le16((ptr)))
#define LOAD_LE32(ptr) (load_le32((ptr)))
#define LOAD_LE64(ptr) (load_le64((ptr)))
#define STORE_LE8(ptr, val) (*reinterpret_cast<U8*>(ptr) = (val))
#define STORE_LE16(ptr, val) (store_le16((ptr), (val)))
#define STORE_LE32(ptr, val) (store_le32((ptr), (val)))
#define STORE_LE64(ptr, val) (store_le64((ptr), (val)))
#define LOAD_BE8(ptr) (*reinterpret_cast<U8*>(ptr))
#define LOAD_BE16(ptr) (_load_be_u16((ptr)))
#define LOAD_BE32(ptr) (_load_be_u32((ptr)))
#define LOAD_BE64(ptr) (_load_be_u64((ptr)))
#define STORE_BE8(ptr, val) (*reinterpret_cast<U8*>(ptr) = (val))
#define STORE_BE16(ptr, val) (_store_be_u16((ptr), (val)))
#define STORE_BE32(ptr, val) (_store_be_u32((ptr), (val)))
#define STORE_BE64(ptr, val) (_store_be_u64((ptr), (val)))


void print(const char* str);
void print_integer(U64 num);
void print_float(F64 f);
void println_integer(U64 num);
void println_float(F64 f);
[[noreturn]] void abort(const char* message);

#define RUNTIME_ASSERT(cond, msg) if(!(cond)) { abort(msg); }