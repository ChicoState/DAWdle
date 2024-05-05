#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#pragma warning(push, 0)
#include <Windows.h>
#pragma warning(pop)
#include "DrillLibDefs.h"
#include "DrillMath.h"

extern "C" int _fltused = 0;

U64 performanceCounterTimerFrequency;

DEBUG_OPTIMIZE_ON

FINLINE void zero_memory(void* mem, U64 bytes) {
	__stosb(reinterpret_cast<Byte*>(mem), 0, bytes);
}

DEBUG_OPTIMIZE_OFF

#pragma warning(disable:28251) // Inconsistent annotation for ''
#pragma warning(disable:6001) // Using uninitialized memory. There appears to be a false positive for some functions here

// For some reason these functions fail to link in release mode but not in debug mode?
#ifdef NDEBUG

#pragma intrinsic(memcpy, memset, strcmp, strlen, memcmp)

#pragma function(memcpy)
void* __cdecl memcpy(void* dst, const void* src, size_t count) {
	const Byte* bSrc = reinterpret_cast<const Byte*>(src);
	Byte* bDst = reinterpret_cast<Byte*>(dst);
	/*while (count--) {
		*bDst++ = *bSrc++;
	}*/
	__movsb(bDst, bSrc, count);
	return dst;
}

#pragma function(memset)
void* __cdecl memset(void* dst, int val, size_t count) {
	Byte* bDst = reinterpret_cast<Byte*>(dst);
	/*while (count--) {
		*bDst++ = val;
	}*/
	__stosb(bDst, Byte(val), count);
	return dst;
}

#pragma function(strcmp)
int __cdecl strcmp(const char* s1, const char* s2) {
	const Byte* bS1 = reinterpret_cast<const Byte*>(s1);
	const Byte* bS2 = reinterpret_cast<const Byte*>(s2);
	Byte b1 = 0;
	Byte b2 = 0;
	while ((b1 = *bS1++) == (b2 = *bS2++)) {
		if (b1 == '\0') {
			break;
		}
	}
	return b1 - b2;
}

#pragma function(strlen)
size_t __cdecl strlen(const char* str) {
	size_t result = 0;
	if (str != nullptr) {
		while (*str++) {
			result++;
		}
	}
	return result;
}
#endif

#pragma function(memcmp)
int __cdecl memcmp(const void* m1, const void* m2, size_t n) {
	int diff = 0;
	const Byte* bM1 = reinterpret_cast<const Byte*>(m1);
	const Byte* bM2 = reinterpret_cast<const Byte*>(m2);
	while (n--) {
		if (*bM1++ != *bM2++) {
			diff = *--bM1 - *--bM2;
			break;
		}
	}
	return diff;
}

#pragma warning(default:6001)
#pragma warning(default:28251)

DEBUG_OPTIMIZE_ON

template<typename T>
FINLINE void swap(T* a, T* b) {
	// All my structs are POD, no need to mess around with move semantics or anything
	T tmp = *b;
	*b = *a;
	*a = tmp;
}

FINLINE U32 bswap32(U32 val) {
	return (val >> 24) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | (val << 24);
}

FINLINE U16 bswap16(U16 val) {
	return U16((val >> 8) | (val << 8));
}

FINLINE U32 lzcnt32(U32 val) {
	return _lzcnt_u32(val);
}

FINLINE U64 lzcnt64(U64 val) {
	return _lzcnt_u64(val);
}

template<typename To, typename From>
FINLINE constexpr To bitcast(const From& val) {
	return __builtin_bit_cast(To, val);
}

DEBUG_OPTIMIZE_OFF

#define MEMORY_ARENA_BYTES_TO_COMMIT_AT_A_TIME (32 * PAGE_SIZE)
#define MEMORY_ARENA_FRAME(arena) for (U64 MEMORY_ARENA_FRAME_stackPtr = (arena).stackPtr; MEMORY_ARENA_FRAME_stackPtr != ~0ull; (arena).stackPtr = MEMORY_ARENA_FRAME_stackPtr, MEMORY_ARENA_FRAME_stackPtr = ~0ull)

struct MemoryArena {
	U8* stackBase;
	U64 stackMaxSize;
	U64 stackPtr;

	bool init(U64 capacity) {
		stackBase = reinterpret_cast<U8*>(VirtualAlloc(nullptr, ((capacity + 4095) & ~0xFFF) + MEMORY_ARENA_BYTES_TO_COMMIT_AT_A_TIME, MEM_RESERVE, PAGE_READWRITE));
		stackBase = reinterpret_cast<U8*>(ALIGN_HIGH(reinterpret_cast<UPtr>(stackBase), MEMORY_ARENA_BYTES_TO_COMMIT_AT_A_TIME));
		if (!stackBase) {
			return false;
		}
		stackMaxSize = capacity;
		stackPtr = 0;
		return true;
	}

	void destroy() {
		VirtualFree(stackBase, 0, MEM_RELEASE);
	}

	void reset() {
		stackPtr = 0;
	}

	template<typename T>
	T* realloc(T* data, U32 oldCount, U32 newCount, U32 slack) {
		T* result = nullptr;
		if (data != nullptr && reinterpret_cast<UPtr>(data + oldCount) + slack == reinterpret_cast<UPtr>(stackBase + stackPtr)) {
			stackPtr += (newCount - oldCount) * sizeof(T);
			result = data;
		}
		else {
			if (newCount > oldCount) {
				stackPtr = ALIGN_HIGH(stackPtr, alignof(T));
				result = reinterpret_cast<T*>(stackBase + stackPtr);
				if (data) {
					memcpy(result, data, oldCount * sizeof(T));
				}
				stackPtr += newCount * sizeof(T);
			}
		}
		return result;
	}

	template<typename T>
	T* realloc(T* data, U32 oldCount, U32 newCount) {
		return realloc<T>(data, oldCount, newCount, 0);
	}

	template<typename T>
	T* alloc(U64 count) {
		stackPtr = ALIGN_HIGH(stackPtr, alignof(T));
		T* result = reinterpret_cast<T*>(stackBase + stackPtr);
		stackPtr += count * sizeof(T);
		return result;
	}

	template<typename T>
	T* zalloc(U64 count) {
		stackPtr = ALIGN_HIGH(stackPtr, alignof(T));
		T* result = reinterpret_cast<T*>(stackBase + stackPtr);
		memset(result, 0, count * sizeof(T));
		stackPtr += count * sizeof(T);
		return result;
	}

	template<typename T>
	T* alloc_bytes(U64 numBytes) {
		stackPtr = ALIGN_HIGH(stackPtr, alignof(T));
		T* result = reinterpret_cast<T*>(stackBase + stackPtr);
		stackPtr += numBytes;
		return result;
	}

	template<typename T>
	T* alloc_aligned_with_slack(U64 count, U32 alignment, U32 slackBytes) {
		alignment = max<U32>(alignment, alignof(T));
		stackPtr = ALIGN_HIGH(stackPtr, alignment);
		T* result = reinterpret_cast<T*>(stackBase + stackPtr);
		stackPtr += count * sizeof(T) + slackBytes;
		return result;
	}

	template<typename T>
	T* alloc_and_commit(U64 count) {
		stackPtr = ALIGN_HIGH(stackPtr, alignof(T));
		// Volatile, just in case the compiler somehow optimizes it out otherwise (though I don't think it reasonably could)
		volatile U8* commitBase = stackBase + stackPtr;
		for (U64 i = 0; i < count * sizeof(T); i += PAGE_SIZE) {
			*(commitBase + i) = 0;
		}
		T* result = reinterpret_cast<T*>(stackBase + stackPtr);
		stackPtr += count * sizeof(T);
		return result;
	}
};

DEBUG_OPTIMIZE_ON

// Things allocated here get pushed and popped with the call stack, 
// importantly, not necessarily at the same time as scopes, allowing objects to be passed out of their C++ scope
// There are two of them so that results can get returned on one while the other is used as temporary scratch space
// In multithreaded code, there should be two of these per thread
// Idea from Ryan Fleury
MemoryArena scratchArena0{};
MemoryArena scratchArena1{};
// Things allocated here exist for two frames. At the end of a frame, it is swapped to lastFrameArena
MemoryArena frameArena{};
// At the end of a frame, lastFrameArena is cleared and made the new frameArena
MemoryArena lastFrameArena{};
// Used to store temporary buffer values for audio nodes
MemoryArena audioArena{};
// Things allocated here exist for the duration of the program, it is never reset
MemoryArena globalArena{};

FINLINE MemoryArena& get_scratch_arena() {
	return scratchArena0;
}
FINLINE MemoryArena& get_scratch_arena_excluding(MemoryArena& arenaToNotReturn) {
	return &arenaToNotReturn == &scratchArena0 ? scratchArena1 : scratchArena0;
}

template<typename T>
struct ArenaArrayList {
	MemoryArena* allocator;
	T* data;
	U32 size;
	U32 capacity;

	FINLINE void reserve(U32 newCapacity) {
		if (newCapacity > capacity) {
			data = (allocator ? allocator : &globalArena)->realloc(data, capacity, newCapacity);
			capacity = newCapacity;
		}
	}

	FINLINE void resize(U32 newSize) {
		if (newSize < size) {
			size = newSize;
		}
		else if (newSize > size) {
			reserve(newSize);
			zero_memory(data + size, (newSize - size) * sizeof(T));
		}
	}

	FINLINE void push_back(const T value) {
		if (size == capacity) {
			reserve(max<U32>(capacity * 2, 8));
		}
		data[size++] = value;
	}

	FINLINE T& push_back() {
		if (size == capacity) {
			reserve(max<U32>(capacity * 2, 8));
		}
		return data[size++];
	}

	FINLINE T& pop_back() {
		return data[--size];
	}

	void push_back_n(const T* values, U32 numValues) {
		U32 newCapacity = capacity;
		if (newCapacity == 0) {
			newCapacity = 8;
		}
		U32 newSize = size + numValues;
		while (newCapacity < newSize) {
			newCapacity *= 2;
		}
		reserve(newCapacity);
		T* dst = data + size;
		while (numValues--) {
			*dst++ = *values++;
		}
		size = newSize;
	}

	FINLINE void pop_back_n(U32 numValues) {
		size -= numValues;
	}

	B32 contains(const T& value) {
		T* begin = data;
		T* end = begin + size;
		B32 returnVal = false;
		while (begin != end) {
			if (*begin == value) {
				returnVal = true;
				break;
			}
			begin++;
		}
		return returnVal;
	}

	B32 subrange_contains(U32 rangeStart, U32 rangeEnd, const T& value) {
		rangeStart = min(rangeStart, size);
		rangeEnd = min(rangeEnd, size);
		if (rangeStart >= rangeEnd) {
			return false;
		}
		T* begin = data + rangeStart;
		T* end = begin + rangeEnd;

		while (begin != end) {
			if (*begin == value) {
				return true;
			}
			begin++;
		}
		return false;
	}

	FINLINE T& last() {
		return data[size - 1];
	}

	FINLINE T* begin() {
		return &data[0];
	}

	FINLINE T* end() {
		return &data[size];
	}

	FINLINE T& back() {
		return data[size - 1];
	}

	FINLINE void clear() {
		size = 0;
	}

	FINLINE void reset() {
		size = 0;
		capacity = 0;
	}

	FINLINE bool empty() {
		return size == 0;
	}
};

// An ASCII string
struct StrA {
	const char* str;
	U64 length;

	FINLINE const char* c_str(MemoryArena& arena) const {
		if (length && str[length - 1] == '\0') {
			return str;
		}
		else {
			char* result = arena.alloc<char>(length + 1);
			memcpy(result, str, length);
			result[length] = '\0';
			return result;
		}
	}
	FINLINE B32 operator==(const StrA& other) const {
		return length == other.length && memcmp(str, other.str, length) == 0;
	}
	FINLINE char operator[](I64 pos) const {
		return str[pos < 0 ? length + pos : pos];
	}
	FINLINE B32 is_empty() const {
		return length == 0;
	}
	FINLINE I64 find(StrA other) const {
		if (other.length > length) {
			return -1;
		}
		for (U64 i = 0; i < length - other.length; i++) {
			if (memcmp(str + i, other.str, other.length) == 0) {
				return I64(i);
			}
		}
		return -1;
	}
	FINLINE I64 find(char c) const {
		for (U64 i = 0; i < length; i++) {
			if (str[i] == c) {
				return I64(i);
			}
		}
		return -1;
	}
	FINLINE I64 rfind(StrA other) const {
		if (other.length > length) {
			return -1;
		}
		for (I64 i = I64(length - other.length - 1); i >= 0; i--) {
			if (memcmp(str + i, other.str, other.length) == 0) {
				return i;
			}
		}
		return -1;
	}
	FINLINE I64 rfind(char c) const {
		for (I64 i = I64(length - 1); i >= 0; i--) {
			if (str[i] == c) {
				return i;
			}
		}
		return -1;
	}
	FINLINE B32 starts_with(StrA other) const {
		return other.length <= length && memcmp(str, other.str, other.length) == 0;
	}
	FINLINE B32 ends_with(StrA other) const {
		return other.length <= length && memcmp(str + (length - other.length), other.str, other.length) == 0;
	}
	FINLINE StrA slice(I64 begin, I64 end) const {
		if (begin < 0) {
			begin = max(I64(length) + begin, 0LL);
		}
		if (end < 0) {
			end = max(I64(length) + end, 0LL);
		}
		if (end < begin) {
			return StrA{};
		}
		U64 first = min(U64(begin), length);
		U64 last = min(U64(end), length);
		return StrA{ str + first, U64(last - first) };
	}
	FINLINE StrA prefix(I64 amount) const {
		return slice(0, amount);
	}
	FINLINE StrA suffix(I64 amount) const {
		return slice(-amount, I64_MAX);
	}
	FINLINE StrA skip(I64 amount) const {
		return slice(amount, I64_MAX);
	}
	FINLINE StrA substr(U64 begin, U64 newLength) const {
		U64 start = min(begin, length);
		return StrA{ str + start, min(newLength, length - start) };
	}
	const char* begin() const {
		return str;
	}
	const char* end() const {
		return str + length;
	}
	char front() const {
		return str[0];
	}
	char back() const {
		return str[length - 1];
	}
};

// Here's one of the C++ features I'll be using, should make string literals much nicer
// Technically user defined literal suffixes that don't start with an underscore are reserved, but I don't really care.
// If it becomes an issue, I'll change it then
FINLINE constexpr StrA operator""sa(const char* lit, U64 len) {
	return StrA{ lit, len };
}

FINLINE U64 total_stralen(StrA str) {
	return str.length;
}
template<typename... Others>
FINLINE U64 total_stralen(StrA str, Others... others) {
	return str.length + total_stralen(others...);
}
FINLINE void copy_strings_to_buffer(char* out, StrA str) {
	memcpy(out, str.str, str.length);
}
template<typename... Others>
FINLINE void copy_strings_to_buffer(char* out, StrA str, Others... others) {
	memcpy(out, str.str, str.length);
	copy_strings_to_buffer(out + str.length, others...);
}
template<typename... Others>
StrA stracat(MemoryArena& arena, StrA strA, Others... others) {
	U64 totalLength = total_stralen(strA, others...);
	char* result = arena.alloc<char>(totalLength);
	copy_strings_to_buffer(result, strA, others...);
	return StrA{ result, totalLength };
}

struct RWSpinLock {
	I32 lock;

	void lock_read() {
		while (true) {
			I32 lockVal = __iso_volatile_load32(&lock);
			if (lockVal != -1 && _InterlockedCompareExchange(reinterpret_cast<long*>(&lock), lockVal + 1, lockVal) == lockVal) {
				break;
			}
			while (__iso_volatile_load32(&lock) == -1);
		}
	}
	void lock_write() {
		while (true) {
			I32 val = 0;
			long result = _InterlockedCompareExchange(reinterpret_cast<long*>(&lock), -1, val);
			if (result == 0) {
				break;
			}
			while (__iso_volatile_load32(&lock) != 0);
		}
	}
	void unlock_read() {
		// Assumes you have already established a read lock
		_InterlockedDecrement(reinterpret_cast<unsigned int*>(&lock));
	}
	void unlock_write() {
		// Assumes you have already established a write lock
		_ReadWriteBarrier();
		__iso_volatile_store32(&lock, 0);
	}
};

struct ByteBuf {
	Byte* bytes;
	U32 offset;
	U32 capacity;
	B32 failed;

	FINLINE void wrap(void* buffer, U32 size) {
		bytes = reinterpret_cast<Byte*>(buffer);
		offset = 0;
		capacity = size;
		failed = false;
	}

	FINLINE bool has_data_left(U32 size) {
		return I64(capacity) - I64(offset) >= I64(size);
	}

	FINLINE void skip(U32 amount) {
		if (capacity - offset < amount) {
			failed = true;
		}
		else {
			offset += amount;
		}
	}

#define READ_FUNC(type, name, loadFunc) FINLINE type name() {\
		type result;\
		if (capacity - offset < sizeof(type)) {\
			result = 0;\
			failed = true;\
		} else {\
			result = loadFunc(bytes + offset);\
			offset += sizeof(type);\
		}\
		return result;\
	}
	READ_FUNC(U8, read_u8, LOAD_LE8)
		READ_FUNC(U16, read_u16, LOAD_LE16)
		READ_FUNC(U32, read_u32, LOAD_LE32)
		READ_FUNC(U64, read_u64, LOAD_LE64)
		READ_FUNC(U8, read_be8, LOAD_BE8)
		READ_FUNC(U16, read_be16, LOAD_BE16)
		READ_FUNC(U32, read_be32, LOAD_BE32)
		READ_FUNC(U64, read_be64, LOAD_BE64)
#undef READ_FUNC
		FINLINE F32 read_f32() {
		F32 result;
		if (capacity - offset < sizeof(F32)) {
			result = 0.0F;
			failed = true;
		}
		else {
			result = bitcast<F32>(LOAD_LE32(bytes + offset));
			offset += sizeof(F32);
		}
		return result;
	}
	FINLINE void read_bytes(void* output, U32 size) {
		if (capacity - offset < size) {
			failed = true;
		}
		else {
			memcpy(output, bytes + offset, size);
			offset += size;
		}
	}

#define WRITE_FUNC(type, name, storeFunc) FINLINE ByteBuf& name(type val) {\
		if (capacity - offset < sizeof(type)) {\
			failed = true;\
		} else {\
			storeFunc(bytes + offset, val);\
			offset += sizeof(type);\
		}\
		return *this;\
	}
	WRITE_FUNC(U8, write_u8, STORE_LE8)
		WRITE_FUNC(U16, write_u16, STORE_LE16)
		WRITE_FUNC(U32, write_u32, STORE_LE32)
		WRITE_FUNC(U64, write_u64, STORE_LE64)
		WRITE_FUNC(U8, write_be8, STORE_BE8)
		WRITE_FUNC(U16, write_be16, STORE_BE16)
		WRITE_FUNC(U32, write_be32, STORE_BE32)
		WRITE_FUNC(U64, write_be64, STORE_BE64)
#undef WRITE_FUNC
		FINLINE ByteBuf& write_f32(F32 val) {
		if (capacity - offset < sizeof(F32)) {
			failed = true;
		}
		else {
			STORE_LE32(bytes + offset, bitcast<U32>(val));
			offset += sizeof(F32);
		}
		return *this;
	}
	FINLINE ByteBuf& write_bytes(const void* src, U32 count) {
		if (capacity - offset < count) {
			failed = true;
		}
		else {
			memcpy(bytes + offset, src, count);
			offset += count;
		}
		return *this;
	}

	FINLINE M4x3F32 read_m4x3f32() {
		M4x3F32 m;
		if (capacity - offset < sizeof(M4x3F32)) {
			failed = true;
			m = M4x3F32{};
		}
		else {
			m.m00 = bitcast<F32>(LOAD_LE32(bytes + offset + 0));
			m.m01 = bitcast<F32>(LOAD_LE32(bytes + offset + 4));
			m.m02 = bitcast<F32>(LOAD_LE32(bytes + offset + 8));
			m.x = bitcast<F32>(LOAD_LE32(bytes + offset + 12));
			m.m10 = bitcast<F32>(LOAD_LE32(bytes + offset + 16));
			m.m11 = bitcast<F32>(LOAD_LE32(bytes + offset + 20));
			m.m12 = bitcast<F32>(LOAD_LE32(bytes + offset + 24));
			m.y = bitcast<F32>(LOAD_LE32(bytes + offset + 28));
			m.m20 = bitcast<F32>(LOAD_LE32(bytes + offset + 32));
			m.m21 = bitcast<F32>(LOAD_LE32(bytes + offset + 36));
			m.m22 = bitcast<F32>(LOAD_LE32(bytes + offset + 40));
			m.z = bitcast<F32>(LOAD_LE32(bytes + offset + 44));
		}
		return m;
	}
	FINLINE ByteBuf& write_m4x3f32(const M4x3F32& m) {
		if (capacity - offset < sizeof(M4x3F32)) {
			failed = true;
		}
		else {
			STORE_LE32(bytes + offset + 0, bitcast<U32>(m.m00));
			STORE_LE32(bytes + offset + 4, bitcast<U32>(m.m01));
			STORE_LE32(bytes + offset + 8, bitcast<U32>(m.m02));
			STORE_LE32(bytes + offset + 12, bitcast<U32>(m.x));
			STORE_LE32(bytes + offset + 16, bitcast<U32>(m.m10));
			STORE_LE32(bytes + offset + 20, bitcast<U32>(m.m11));
			STORE_LE32(bytes + offset + 24, bitcast<U32>(m.m12));
			STORE_LE32(bytes + offset + 28, bitcast<U32>(m.y));
			STORE_LE32(bytes + offset + 32, bitcast<U32>(m.m20));
			STORE_LE32(bytes + offset + 36, bitcast<U32>(m.m21));
			STORE_LE32(bytes + offset + 40, bitcast<U32>(m.m22));
			STORE_LE32(bytes + offset + 44, bitcast<U32>(m.z));
			offset += sizeof(M4x3F32);
		}
		return *this;
	}

	FINLINE StrA read_stra() {
		StrA result{};
		U32 length = read_u32();
		if (failed || !has_data_left(length)) {
			failed = true;
		}
		else {
			result = StrA{ reinterpret_cast<const char*>(bytes + offset), length };
			offset += length;
		}
		return result;
	}
	FINLINE ByteBuf& write_stra(StrA str) {
		if (str.length > U64(U32_MAX) || capacity - offset < U32(str.length)) {
			failed = true;
		}
		else {
			STORE_LE32(bytes + offset, U32(str.length));
			memcpy(bytes + offset + sizeof(U32), str.str, str.length);
			offset += 4 + U32(str.length);
		}
		return *this;
	}
};

DEBUG_OPTIMIZE_OFF

HANDLE consoleOutput;

// One day I'll get around to implementing a printf, then I can get rid of this horrid API
void print(const char* str) {
	// Doing a lot of WriteFile calls like this is going to be really slow, but I probably won't be printing a lot of stuff like this anyway
	WriteFile(consoleOutput, str, DWORD(strlen(str)), NULL, NULL);
}
void println() {
	print("\n");
}
void println(const char* str) {
	print(str);
	print("\n");
}
void print(StrA str) {
	WriteFile(consoleOutput, str.str, DWORD(str.length), NULL, NULL);
}
void println(StrA str) {
	print(str);
	print("\n"sa);
}
void print_integer(U64 num) {
	if (num == 0) {
		print("0");
	}
	else {
		char buffer[32];
		buffer[31] = '\0';
		char* str = buffer + 31;
		while (num) {
			*--str = char(num % 10) + '0';
			num /= 10;
		}
		print(str);
	}
}
void print_integer_pad(U64 num, I32 pad) {
	char buffer[32];
	buffer[31] = '\0';
	char* str = buffer + 31;
	while (num) {
		*--str = char(num % 10) + '0';
		num /= 10;
		pad--;
	}
	while (pad-- > 0) {
		*--str = '0';
	}
	print(str);
}
namespace SerializeTools {
	void serialize_f64(char* dstBuffer, U32* dstBufferSize, F64 startValue);
}
void print_float(F64 f) {
	char floatBuf[64];
	U32 floatBufSize = sizeof(floatBuf);
	SerializeTools::serialize_f64(floatBuf, &floatBufSize, f);
	print(StrA{ floatBuf, floatBufSize });
}
void println_integer(U64 num) {
	print_integer(num);
	print("\n");
}
void println_float(F64 f) {
	print_float(f);
	print("\n");
}

[[noreturn]] void abort(const char* message) {
	print(message);
	__debugbreak();
	ExitProcess(EXIT_FAILURE);
}

template<typename T>
T* read_full_file_to_arena(U32* count, MemoryArena& arena, StrA fileName) {
	T* result = nullptr;
	U64 oldStackPtr = arena.stackPtr;
	HANDLE file = CreateFileA(fileName.c_str(arena), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	arena.stackPtr = oldStackPtr;
	if (file != INVALID_HANDLE_VALUE) {
		U32 size = GetFileSize(file, NULL);
		result = arena.alloc_and_commit<T>(size);
		DWORD numBytesRead{};
		if (!ReadFile(file, result, size, &numBytesRead, NULL)) {
			result = nullptr;
			DWORD fileReadError = GetLastError();
			print("Failed to read file, code: ");
			println_integer(fileReadError);
		}
		CloseHandle(file);
		*count = numBytesRead / sizeof(T);
	}
	else {
		print("Failed to create file, code: ");
		println_integer(GetLastError());
	}
	return result;
}

B32 write_data_to_file(StrA fileName, void* data, U32 numBytes) {
	MemoryArena& stackArena = get_scratch_arena();
	U64 oldArenaPtr = stackArena.stackPtr;
	HANDLE file = CreateFileA(fileName.c_str(stackArena), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	B32 success = false;
	if (file != INVALID_HANDLE_VALUE) {
		success = !!WriteFile(file, data, numBytes, 0, NULL);
		CloseHandle(file);
	}
	else {
		print("Failed to create file, code: ");
		println_integer(GetLastError());
	}
	stackArena.stackPtr = oldArenaPtr;
	return success;
}

B32 run_program_and_wait(U32* exitCodeOut, StrA programName, StrA commandLine) {
	MemoryArena& stackArena = get_scratch_arena();
	BOOL success = FALSE;
	MEMORY_ARENA_FRAME(stackArena) {
		char* commandLineCStr = stackArena.alloc<char>(programName.length + 1 + commandLine.length + 1);
		memcpy(commandLineCStr, programName.str, programName.length);
		commandLineCStr[programName.length] = ' ';
		memcpy(commandLineCStr + programName.length + 1, commandLine.str, commandLine.length);
		commandLineCStr[programName.length + 1 + commandLine.length] = '\0';

		STARTUPINFOA startupInfo{};
		startupInfo.cb = sizeof(STARTUPINFOA);
		PROCESS_INFORMATION procInfo{};
		success = CreateProcessA(programName.c_str(stackArena), commandLineCStr, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &procInfo);

		if (success) {
			WaitForSingleObject(procInfo.hProcess, INFINITE);
			DWORD exitCode;
			if (GetExitCodeProcess(procInfo.hProcess, &exitCode)) {
				*exitCodeOut = exitCode;
			}
			else {
				*exitCodeOut = U32(-1);
			}
			CloseHandle(procInfo.hProcess);
			CloseHandle(procInfo.hThread);
		}
	}
	return B32(success != 0);
}

F64 current_time_seconds() {
	LARGE_INTEGER perfCounter;
	if (!QueryPerformanceCounter(&perfCounter)) {
		abort("Could not get performanceCounter");
	}
	return F64(perfCounter.QuadPart) / F64(performanceCounterTimerFrequency);
}

void (*previousPageFaultHandler)(int);

// This handler is used so we can reserve large amounts of memory up front and only commit it when we need to
LONG WINAPI page_fault_handler(PEXCEPTION_POINTERS exceptionPointers) {
	LONG result = EXCEPTION_CONTINUE_SEARCH;
	if (exceptionPointers->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
		// https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_record
		// For EXCEPTION_ACCESS_VIOLATION
		// ExceptionInformation[0] contains 0 if read, 1 if write
		// ExceptionInformation[1] contains the virtual address accessed
		UPtr address = ALIGN_LOW(exceptionPointers->ExceptionRecord->ExceptionInformation[1], MEMORY_ARENA_BYTES_TO_COMMIT_AT_A_TIME);
		LPVOID allocatedAddress = nullptr;
		if ((address >= reinterpret_cast<UPtr>(scratchArena0.stackBase) && (address - reinterpret_cast<UPtr>(scratchArena0.stackBase)) < scratchArena0.stackMaxSize) ||
			(address >= reinterpret_cast<UPtr>(scratchArena1.stackBase) && (address - reinterpret_cast<UPtr>(scratchArena1.stackBase)) < scratchArena1.stackMaxSize) ||
			(address >= reinterpret_cast<UPtr>(frameArena.stackBase) && (address - reinterpret_cast<UPtr>(frameArena.stackBase)) < frameArena.stackMaxSize) ||
			(address >= reinterpret_cast<UPtr>(lastFrameArena.stackBase) && (address - reinterpret_cast<UPtr>(lastFrameArena.stackBase)) < lastFrameArena.stackMaxSize) ||
			(address >= reinterpret_cast<UPtr>(audioArena.stackBase) && (address - reinterpret_cast<UPtr>(audioArena.stackBase)) < audioArena.stackMaxSize) ||
			(address >= reinterpret_cast<UPtr>(globalArena.stackBase) && (address - reinterpret_cast<UPtr>(globalArena.stackBase)) < globalArena.stackMaxSize)) {
			allocatedAddress = VirtualAlloc(reinterpret_cast<void*>(address), MEMORY_ARENA_BYTES_TO_COMMIT_AT_A_TIME, MEM_COMMIT, PAGE_READWRITE);
		}
		if (allocatedAddress) {
			result = EXCEPTION_CONTINUE_EXECUTION;
		}
	}
	return result;
}

B32 drill_lib_init() {
	LARGE_INTEGER perfFreq;
	if (!QueryPerformanceFrequency(&perfFreq)) {
		abort("Could not get performance counter frequency");
	}
	performanceCounterTimerFrequency = U64(perfFreq.QuadPart);

	consoleOutput = CreateFileA("CON", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	AddVectoredExceptionHandler(1, page_fault_handler);
	if (!scratchArena0.init(1 * GIGABYTE)) {
		return false;
	}
	if (!scratchArena1.init(1 * GIGABYTE)) {
		return false;
	}
	if (!frameArena.init(1 * GIGABYTE)) {
		return false;
	}
	if (!lastFrameArena.init(1 * GIGABYTE)) {
		return false;
	}
	if (!audioArena.init(1 * GIGABYTE)) {
		return false;
	}
	if (!globalArena.init(1 * GIGABYTE)) {
		return false;
	}
	return true;
}