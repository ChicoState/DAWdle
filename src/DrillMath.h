#pragma once
#include <intrin.h>
#include <immintrin.h>
#include "DrillLibDefs.h"

DEBUG_OPTIMIZE_ON

// The cos approximation is more accurate than the sin one I generated (gentler curve over [0, 0.5) I guess), so we can just use it for sin as well
// Cos approximation: 1.00010812282562255859375 + x * (-1.79444365203380584716796875e-2 + x * (-19.2416629791259765625 + x * (-5.366222381591796875 + x * (93.06533050537109375 + x * (-74.45227813720703125)))))
// Found using Sollya (https://www.sollya.org/)
// This isn't a particularly good polynomial approximation in general, but it's good enough for me.
// It's a good bit faster than the microsoft standard library implementation while trading off some accuracy, which is a win for me.
// This paper should provide some info on how to make it better if I need it (https://arxiv.org/pdf/1508.03211.pdf)

FINLINE __m128 cosf32x4(__m128 xmmX) {
	__m128 xRoundedDown = _mm_round_ps(xmmX, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
	__m128 xIn0to1 = _mm_sub_ps(xmmX, xRoundedDown);
	__m128 point5 = _mm_set_ps1(0.5F);
	__m128 xIn0to1MinusPoint5 = _mm_sub_ps(xIn0to1, point5);
	__m128 isGreaterThanPoint5 = _mm_cmpge_ps(xIn0to1, point5);
	__m128 xInRange0ToPoint5 = _mm_blendv_ps(xIn0to1, xIn0to1MinusPoint5, isGreaterThanPoint5);
	__m128 sinApprox = _mm_fmadd_ps(xInRange0ToPoint5, _mm_set_ps1(-74.45227813720703125F), _mm_set_ps1(93.06533050537109375F));
	sinApprox = _mm_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm_set_ps1(-5.366222381591796875F));
	sinApprox = _mm_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm_set_ps1(-19.2416629791259765625F));
	sinApprox = _mm_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm_set_ps1(-1.79444365203380584716796875e-2F));
	sinApprox = _mm_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm_set_ps1(1.00010812282562255859375F));
	__m128 sinApproxFlipped = _mm_sub_ps(_mm_setzero_ps(), sinApprox);
	sinApprox = _mm_blendv_ps(sinApprox, sinApproxFlipped, isGreaterThanPoint5);
	return sinApprox;
}

FINLINE __m128 sinf32x4(__m128 xmmX) {
	return cosf32x4(_mm_sub_ps(xmmX, _mm_set_ps1(0.25F)));
}

FINLINE __m128 tanf32x4(__m128 xmmX) {
	return _mm_div_ps(sinf32x4(xmmX), cosf32x4(xmmX));
}

// Optimized using pracma in R
// The polynomial approximation initially didn't behave well near 1 where acos is supposed to be 0,
// so I tried scaling it by sqrt(1 - x), which is a function that goes to 0 at x == 1, then optimizing the polynomial for acos(x)/sqrt(1 - x)
// That worked pretty well
// I've actually used R for something! Thanks Nicholas Lytal.
FINLINE __m128 acosf32x4(__m128 xmmX) {
	__m128 shouldFlip = _mm_cmplt_ps(xmmX, _mm_set_ps1(0.0F));
	__m128 absX = _mm_and_ps(xmmX, _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF)));
	__m128 sqrtOneMinusX = _mm_sqrt_ps(_mm_sub_ps(_mm_set_ps1(1.0F), absX));
	sqrtOneMinusX = _mm_xor_ps(sqrtOneMinusX, _mm_and_ps(shouldFlip, _mm_castsi128_ps(_mm_set1_epi32(0x80000000))));
	__m128 acosApprox = _mm_fmadd_ps(absX, _mm_set_ps1(-0.002895482F), _mm_set_ps1(0.011689540F));
	acosApprox = _mm_fmadd_ps(absX, acosApprox, _mm_set_ps1(-0.033709585F));
	acosApprox = _mm_fmadd_ps(absX, acosApprox, _mm_set_ps1(0.249986371F));
	return _mm_fmadd_ps(acosApprox, sqrtOneMinusX, _mm_and_ps(_mm_set_ps1(0.5F), shouldFlip));
}
FINLINE __m128 asinf32x4(__m128 xmmX) {
	return _mm_sub_ps(_mm_set_ps1(0.25F), acosf32x4(xmmX));
}
// This function uses the identity that atan(x) + atan(1/x) is a quarter turn in order to only have to approximate atan between 0 and 1
// Optimized using Sollya, approx = 0.15886362603x - 0.04743765592x^3 + 0.01357402989x^5
// If abs(x) <= 1, use approx(x). Otherwise, use 0.25 - approx(1/x)
FINLINE __m128 atanf32x4(__m128 xmmX) {
	const __m128 signBit = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
	const __m128 signBitMaskOff = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
	__m128 xSign = _mm_and_ps(xmmX, signBit);
	__m128 absX = _mm_and_ps(xmmX, signBitMaskOff);
	__m128 isGreaterThan1 = _mm_cmpgt_ps(absX, _mm_set_ps1(1.0F));
	absX = _mm_blendv_ps(absX, _mm_rcp_ps(absX), isGreaterThan1);
	__m128 xSq = _mm_mul_ps(absX, absX);
	__m128 atanApprox = _mm_fmadd_ps(xSq, _mm_set_ps1(0.01357402989F), _mm_set_ps1(-0.04743765592F));
	atanApprox = _mm_fmadd_ps(xSq, atanApprox, _mm_set_ps1(0.15886362603F));
	atanApprox = _mm_mul_ps(absX, atanApprox);
	atanApprox = _mm_sub_ps(_mm_and_ps(_mm_set_ps1(0.25F), isGreaterThan1), _mm_xor_ps(atanApprox, _mm_andnot_ps(isGreaterThan1, signBit)));
	return _mm_or_ps(atanApprox, xSign);
}
FINLINE __m128 atan2f32x4(__m128 xmmY, __m128 xmmX) {
	const __m128 signBit = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
	// Rotate the input by a quarter turn so that way we go from -0.25 to 0.25 over the top half and -0.25 to 0.25 over the bottom half
	__m128 atanResult = atanf32x4(_mm_div_ps(xmmX, _mm_xor_ps(xmmY, signBit)));
	__m128 isInLowerHalf = _mm_cmplt_ps(xmmY, _mm_setzero_ps());
	// Add either 0.25 or 0.75 depending on the half to get the turn back out
	return _mm_add_ps(_mm_blendv_ps(_mm_set_ps1(0.25F), _mm_set_ps1(0.75F), isInLowerHalf), atanResult);
}

FINLINE __m256 cosf32x8(__m256 ymmX) {
	__m256 xRoundedDown = _mm256_round_ps(ymmX, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
	__m256 xIn0to1 = _mm256_sub_ps(ymmX, xRoundedDown);
	__m256 point5 = _mm256_set1_ps(0.5F);
	__m256 xIn0to1MinusPoint5 = _mm256_sub_ps(xIn0to1, point5);
	__m256 isGreaterThanPoint5 = _mm256_cmp_ps(xIn0to1, point5, _CMP_GE_OQ);
	__m256 xInRange0ToPoint5 = _mm256_blendv_ps(xIn0to1, xIn0to1MinusPoint5, isGreaterThanPoint5);
	__m256 sinApprox = _mm256_fmadd_ps(xInRange0ToPoint5, _mm256_set1_ps(-74.45227813720703125F), _mm256_set1_ps(93.06533050537109375F));
	sinApprox = _mm256_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm256_set1_ps(-5.366222381591796875F));
	sinApprox = _mm256_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm256_set1_ps(-19.2416629791259765625F));
	sinApprox = _mm256_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm256_set1_ps(-1.79444365203380584716796875e-2F));
	sinApprox = _mm256_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm256_set1_ps(1.00010812282562255859375F));
	__m256 sinApproxFlipped = _mm256_sub_ps(_mm256_setzero_ps(), sinApprox);
	sinApprox = _mm256_blendv_ps(sinApprox, sinApproxFlipped, isGreaterThanPoint5);
	return sinApprox;
}

FINLINE __m256 sinf32x8(__m256 ymmX) {
	return cosf32x8(_mm256_sub_ps(ymmX, _mm256_set1_ps(0.25F)));
}

FINLINE __m256 tanf32x8(__m256 ymmX) {
	return _mm256_div_ps(sinf32x8(ymmX), cosf32x8(ymmX));
}

FINLINE F32 cosf32(F32 x) {
	// MSVC is very bad at optimizing *_ss intrinsics, so it's actually faster to simply use the x4 version and take one of the results
	// The *_ss version was more than 50% (!) slower in my tests, and a quick look in godbolt explains why
	return _mm_cvtss_f32(cosf32x4(_mm_set_ps1(x)));
	/*__m128 xmmX = _mm_set_ss(x);
	__m128 xRoundedDown = _mm_round_ss(_mm_undefined_ps(), xmmX, _MM_ROUND_MODE_DOWN);
	__m128 xIn0to1 = _mm_sub_ss(xmmX, xRoundedDown);
	__m128 point5 = _mm_set_ss(0.5F);
	__m128 xIn0to1MinusPoint5 = _mm_sub_ss(xIn0to1, point5);
	__m128 isGreaterThanPoint5 = _mm_cmpge_ss(xIn0to1, point5);
	__m128 xInRange0ToPoint5 = _mm_blendv_ps(xIn0to1, xIn0to1MinusPoint5, isGreaterThanPoint5);
	__m128 sinApprox = _mm_fmadd_ss(xInRange0ToPoint5, _mm_set_ss(-74.45227813720703125F), _mm_set_ss(93.06533050537109375F));
	sinApprox = _mm_fmadd_ss(xInRange0ToPoint5, sinApprox, _mm_set_ss(-5.366222381591796875F));
	sinApprox = _mm_fmadd_ss(xInRange0ToPoint5, sinApprox, _mm_set_ss(-19.2416629791259765625F));
	sinApprox = _mm_fmadd_ss(xInRange0ToPoint5, sinApprox, _mm_set_ss(-1.79444365203380584716796875e-2F));
	sinApprox = _mm_fmadd_ss(xInRange0ToPoint5, sinApprox, _mm_set_ss(1.00010812282562255859375F));
	__m128 sinApproxFlipped = _mm_sub_ss(_mm_setzero_ps(), sinApprox);
	sinApprox = _mm_blendv_ps(sinApprox, sinApproxFlipped, isGreaterThanPoint5);
	return _mm_cvtss_f32(sinApprox);*/
}
FINLINE F32 sinf32(F32 x) {
	// MSVC is very bad at optimizing *_ss intrinsics, so it's actually faster to simply use the x4 version and take one of the results
	return _mm_cvtss_f32(cosf32x4(_mm_set_ps1(x - 0.25F)));
}
FINLINE F32 tanf32(F32 x) {
	return _mm_cvtss_f32(tanf32x4(_mm_set_ps1(x)));
}
FINLINE F32 acosf32(F32 x) {
	return _mm_cvtss_f32(acosf32x4(_mm_set_ps1(x)));
}
FINLINE F32 asinf32(F32 x) {
	return _mm_cvtss_f32(asinf32x4(_mm_set_ps1(x)));
}
FINLINE F32 atanf32(F32 x) {
	return _mm_cvtss_f32(atanf32x4(_mm_set_ps1(x)));
}
FINLINE F32 atan2f32(F32 y, F32 x) {
	return _mm_cvtss_f32(atan2f32x4(_mm_set_ps1(y), _mm_set_ps1(x)));
}
FINLINE F32 sqrtf32(F32 x) {
	return _mm_cvtss_f32(_mm_sqrt_ps(_mm_set_ss(x)));
}
FINLINE F32 sincosf32(F32* sinOut, F32 x) {
	F32 cosine = cosf32(x);
	*sinOut = sinf32(x); // Could probably optimize this using the pythagorean identity and the result of cosf, I'll figure it out later
	return cosine;
}
FINLINE F32 fractf32(F32 f) {
	return f - _mm_cvtss_f32(_mm_round_ps(_mm_set_ss(f), _MM_ROUND_MODE_TOWARD_ZERO));
}
FINLINE F32 truncf32(F32 f) {
	return _mm_cvtss_f32(_mm_round_ps(_mm_set_ss(f), _MM_ROUND_MODE_TOWARD_ZERO));
}
FINLINE F32 floorf32(F32 f) {
	return _mm_cvtss_f32(_mm_round_ps(_mm_set_ss(f), _MM_ROUND_MODE_DOWN));
}
FINLINE F32 ceilf32(F32 f) {
	return _mm_cvtss_f32(_mm_round_ps(_mm_set_ss(f), _MM_ROUND_MODE_UP));
}
FINLINE F32 absf32(F32 f) {
	return _mm_cvtss_f32(_mm_and_ps(_mm_set_ss(f), _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF))));
}
FINLINE I32 signumf32(F32 f) {
	return (f > 0.0F) - (f < 0.0F);
}

FINLINE F64 truncf64(F64 f) {
	return _mm_cvtsd_f64(_mm_round_pd(_mm_set_sd(f), _MM_ROUND_MODE_TOWARD_ZERO));
}
FINLINE F64 fractf64(F64 f) {
	return f - _mm_cvtsd_f64(_mm_round_pd(_mm_set_sd(f), _MM_ROUND_MODE_TOWARD_ZERO));
}

template<typename T>
FINLINE T max(T a, T b) {
	return a > b ? a : b;
}
template<typename T, typename... C>
FINLINE T max(T a, T b, C... c) {
	return max(a > b ? a : b, c...);
}
template<typename T>
FINLINE T min(T a, T b) {
	return a < b ? a : b;
}
template<typename T, typename... C>
FINLINE T min(T a, T b, C... c) {
	return min(a < b ? a : b, c...);
}

template<typename T>
FINLINE T clamp(T val, T low, T high) {
	return min(high, max(low, val));
}
template<typename T>
FINLINE T clamp01(T val) {
	return clamp(val, static_cast<T>(0.0), static_cast<T>(1.0));
}

template<typename T>
FINLINE T abs(T val) {
	return val < T(0) ? -val : val;
}

FINLINE B32 epsilon_eq(F32 a, F32 b, F32 eps) {
	return absf32(a - b) <= max(a, b) * eps;
}

FINLINE U16 next_power_of_two(U16 x) {
	return U16(1u << (16u - __lzcnt16(x - 1u)));
}
FINLINE U32 next_power_of_two(U32 x) {
	return 1u << (32u - __lzcnt(x - 1u));
}
FINLINE U64 next_power_of_two(U64 x) {
	return 1ull << (64ull - __lzcnt64(x - 1ull));
}

#pragma pack(push, 1)
union V2F32 {
	struct {
		F32 x, y;
	};
	F32 v[2];
};
#pragma pack(pop)

FINLINE V2F32 operator+(V2F32 a, V2F32 b) {
	return V2F32{ a.x + b.x, a.y + b.y };
}
FINLINE V2F32 operator+(F32 a, V2F32 b) {
	return V2F32{ a + b.x, a + b.y };
}
FINLINE V2F32 operator+(V2F32 a, F32 b) {
	return V2F32{ a.x + b, a.y + b };
}
FINLINE V2F32 operator+=(V2F32& a, V2F32 b) {
	a.x += b.x;
	a.y += b.y;
	return a;
}
FINLINE V2F32 operator+=(V2F32& a, F32 b) {
	a.x += b;
	a.y += b;
	return a;
}

FINLINE V2F32 operator-(V2F32 a, V2F32 b) {
	return V2F32{ a.x - b.x, a.y - b.y };
}
FINLINE V2F32 operator-(F32 a, V2F32 b) {
	return V2F32{ a - b.x, a - b.y };
}
FINLINE V2F32 operator-(V2F32 a, F32 b) {
	return V2F32{ a.x - b, a.y - b };
}
FINLINE V2F32 operator-(V2F32 a) {
	return V2F32{ -a.x, -a.y };
}
FINLINE V2F32 operator-=(V2F32& a, V2F32 b) {
	a.x -= b.x;
	a.y -= b.y;
	return a;
}
FINLINE V2F32 operator-=(V2F32& a, F32 b) {
	a.x -= b;
	a.y -= b;
	return a;
}

FINLINE V2F32 operator*(V2F32 a, V2F32 b) {
	return V2F32{ a.x * b.x, a.y * b.y };
}
FINLINE V2F32 operator*(V2F32 a, F32 b) {
	return V2F32{ a.x * b, a.y * b };
}
FINLINE V2F32 operator*(F32 a, V2F32 b) {
	return V2F32{ a * b.x, a * b.y };
}
FINLINE V2F32 operator*=(V2F32& a, V2F32 b) {
	a.x *= b.x;
	a.y *= b.y;
	return a;
}
FINLINE V2F32 operator*=(V2F32& a, F32 b) {
	a.x *= b;
	a.y *= b;
	return a;
}

FINLINE V2F32 operator/(V2F32 a, V2F32 b) {
	return V2F32{ a.x / b.x, a.y / b.y };
}
FINLINE V2F32 operator/(V2F32 a, F32 b) {
	F32 invB = 1.0F / b;
	return V2F32{ a.x * invB, a.y * invB };
}
FINLINE V2F32 operator/(F32 a, V2F32 b) {
	return V2F32{ a / b.x, a / b.y };
}
FINLINE V2F32 operator/=(V2F32& a, V2F32 b) {
	a.x /= b.x;
	a.y /= b.y;
	return a;
}
FINLINE V2F32 operator/=(V2F32& a, F32 b) {
	F32 invB = 1.0F / b;
	a.x *= invB;
	a.y *= invB;
	return a;
}

FINLINE F32 dot(V2F32 a, V2F32 b) {
	return a.x * b.x + a.y * b.y;
}
FINLINE F32 cross(V2F32 a, V2F32 b) {
	return a.x * b.y - a.y * b.x;
}
FINLINE F32 length_sq(V2F32 v) {
	return v.x * v.x + v.y * v.y;
}
FINLINE F32 distance_sq(V2F32 a, V2F32 b) {
	F32 x = a.x - b.x;
	F32 y = a.y - b.y;
	return x * x + y * y;
}
FINLINE F32 length(V2F32 v) {
	return sqrtf32(v.x * v.x + v.y * v.y);
}
FINLINE F32 distance(V2F32 a, V2F32 b) {
	F32 x = a.x - b.x;
	F32 y = a.y - b.y;
	return sqrtf32(x * x + y * y);
}
FINLINE V2F32 normalize(V2F32 v) {
	F32 invLen = 1.0F / sqrtf32(v.x * v.x + v.y * v.y);
	return V2F32{ v.x * invLen, v.y * invLen };
}
FINLINE B32 epsilon_eq(V2F32 a, V2F32 b, F32 epsilon) {
	return epsilon_eq(a.x, b.x, epsilon) && epsilon_eq(a.y, b.y, epsilon);
}
FINLINE V2F32 get_orthogonal(V2F32 v) {
	return V2F32{ -v.y, v.x };
}

void println_v2f32(V2F32 vec) {
	print("(");
	print_float(vec.x);
	print(", ");
	print_float(vec.y);
	print(")\n");
}

#pragma pack(push, 1)
union V3F32 {
	struct {
		F32 x, y, z;
	};
	F32 v[3];

	FINLINE F32 length_sq() {
		return x * x + y * y + z * z;
	}

	FINLINE F32 length() {
		return sqrtf32(x * x + y * y + z * z);
	}

	FINLINE V3F32& normalize() {
		F32 invLen = 1.0F / length();
		x *= invLen;
		y *= invLen;
		z *= invLen;
		return *this;
	}

	FINLINE V2F32 xy() {
		return V2F32{ x, y };
	}
	FINLINE V2F32 xz() {
		return V2F32{ x, z };
	}
	FINLINE V2F32 yz() {
		return V2F32{ y, z };
	}
};
#pragma pack(pop)

static constexpr V3F32 V3F32_UP{ 0.0F, 1.0F, 0.0F };
static constexpr V3F32 V3F32_DOWN{ 0.0F, -1.0F, 0.0F };
static constexpr V3F32 V3F32_NORTH{ 0.0F, 0.0F, -1.0F };
static constexpr V3F32 V3F32_SOUTH{ 0.0F, 0.0F, 1.0F };
static constexpr V3F32 V3F32_EAST{ 1.0F, 0.0F, 0.0F };
static constexpr V3F32 V3F32_WEST{ -1.0F, 0.0F, 0.0F };

FINLINE V3F32 operator+(V3F32 a, F32 b) {
	return V3F32{ a.x + b, a.y + b, a.z + b };
}
FINLINE V3F32 operator+(F32 a, V3F32 b) {
	return V3F32{ a + b.x, a + b.y, a + b.z };
}
FINLINE V3F32 operator+(V3F32 a, V3F32 b) {
	return V3F32{ a.x + b.x, a.y + b.y, a.z + b.z };
}

FINLINE V3F32 operator-(V3F32 a, V3F32 b) {
	return V3F32{ a.x - b.x, a.y - b.y, a.z - b.z };
}
FINLINE V3F32 operator-(V3F32 a, F32 b) {
	return V3F32{ a.x - b, a.y - b, a.z - b };
}
FINLINE V3F32 operator-(F32 a, V3F32 b) {
	return V3F32{ a - b.x, a - b.y, a - b.z };
}
FINLINE V3F32 operator-(V3F32 a) {
	return V3F32{ -a.x, -a.y, -a.z };
}

FINLINE V3F32 operator*(V3F32 a, V3F32 b) {
	return V3F32{ a.x * b.x, a.y * b.y, a.z * b.z };
}
FINLINE V3F32 operator*(V3F32 a, F32 b) {
	return V3F32{ a.x * b, a.y * b, a.z * b };
}
FINLINE V3F32 operator*(F32 a, V3F32 b) {
	return V3F32{ a * b.x, a * b.y, a * b.z };
}

FINLINE V3F32 operator/(V3F32 a, V3F32 b) {
	return V3F32{ a.x / b.x, a.y / b.y, a.z / b.z };
}
FINLINE V3F32 operator/(V3F32 a, F32 b) {
	F32 invB = 1.0F / b;
	return V3F32{ a.x * invB, a.y * invB, a.z * invB };
}
FINLINE V3F32 operator/(F32 a, V3F32 b) {
	return V3F32{ a / b.x, a / b.y, a / b.z };
}


FINLINE F32 dot(V3F32 a, V3F32 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}
FINLINE V3F32 cross(V3F32 a, V3F32 b) {
	return V3F32{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
FINLINE F32 length_sq(V3F32 v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}
FINLINE F32 distance_sq(V3F32 a, V3F32 b) {
	return (a.x - b.x) * (a.y - b.y) * (a.z - b.z);
}
FINLINE F32 length(V3F32 v) {
	return sqrtf32(v.x * v.x + v.y * v.y + v.z * v.z);
}
FINLINE F32 distance(V3F32 a, V3F32 b) {
	return sqrtf32((a.x - b.x) * (a.y - b.y) * (a.z - b.z));
}
FINLINE V3F32 normalize(V3F32 v) {
	F32 invLen = 1.0F / sqrtf32(v.x * v.x + v.y * v.y + v.z * v.z);
	return V3F32{ v.x * invLen, v.y * invLen, v.z * invLen };
}

DEBUG_OPTIMIZE_OFF

void println_v3f32(V3F32 vec) {
	print("(");
	print_float(vec.x);
	print(", ");
	print_float(vec.y);
	print(", ");
	print_float(vec.z);
	print(")\n");
}

struct RGBA8;
#pragma pack(push, 1)
union V4F32 {
	struct {
		F32 x, y, z, w;
	};
	F32 v[4];

	RGBA8 to_rgba8();
};
#pragma pack(pop)

void println_v4f32(V4F32 vec) {
	print("(");
	print_float(vec.x);
	print(", ");
	print_float(vec.y);
	print(", ");
	print_float(vec.z);
	print(", ");
	print_float(vec.w);
	print(")\n");
}

// Not optimized, might be worth coming back to
template<typename Vec>
F32 time_along_line(Vec pos, Vec linePointA, Vec linePointB) {
	Vec lineDir = linePointB - linePointA;
	Vec posToA = pos - linePointA;
	return dot(lineDir, posToA) / length_sq(lineDir);
}
template<typename Vec>
F32 distance_to_line(Vec pos, Vec linePointA, Vec linePointB) {
	F32 t = time_along_line(pos, linePointA, linePointB);
	return distance(pos, linePointA + t * (linePointB - linePointA));
}
template<typename Vec>
F32 distance_to_ray(Vec pos, Vec rayPoint, Vec rayDirection) {
	F32 t = time_along_line(pos, rayPoint, rayPoint + rayDirection);
	return distance(pos, rayPoint + max(t, 0.0F) * rayDirection);
}
template<typename Vec>
F32 distance_to_segment(Vec pos, Vec linePointA, Vec linePointB) {
	F32 t = time_along_line(pos, linePointA, linePointB);
	return distance(pos, linePointA + clamp01(t) * (linePointB - linePointA));
}
F32 signed_distance_to_line(V2F32 pos, V2F32 linePointA, V2F32 linePointB) {
	F32 t = time_along_line(pos, linePointA, linePointB);
	V2F32 lineDirection = linePointB - linePointA;
	return distance(pos, linePointA + t * lineDirection) * F32(signumf32(cross(lineDirection, pos - linePointA)));
}
F32 signed_distance_to_ray(V2F32 pos, V2F32 rayPoint, V2F32 rayDirection) {
	F32 t = time_along_line(pos, rayPoint, rayPoint + rayDirection);
	return distance(pos, rayPoint + max(t, 0.0F) * rayDirection) * F32(signumf32(cross(rayDirection, pos - rayPoint)));
}
F32 signed_distance_to_segment(V2F32 pos, V2F32 linePointA, V2F32 linePointB) {
	F32 t = time_along_line(pos, linePointA, linePointB);
	V2F32 lineDirection = linePointB - linePointA;
	return distance(pos, linePointA + clamp01(t) * lineDirection) * F32(signumf32(cross(lineDirection, pos - linePointA)));
}

DEBUG_OPTIMIZE_ON

template<typename Vec>
FINLINE Vec mix(Vec a, Vec b, F32 t) {
	return a + (b - a) * t;
}
template<typename Vec>
FINLINE Vec clamped_mix(Vec a, Vec b, F32 t) {
	if (t <= 0.0F) {
		return a;
	} else if (t >= 1.0F) {
		return b;
	} else {
		return a + (b - a) * t;
	}
}

FINLINE F32 vec2_angle(V2F32 a, V2F32 b) {
	F32 sign = F32(signumf32(cross(a, b)));
	return sign * acosf32(dot(a, b) / (length(a) * length(b)));
}

struct QF32;

struct AxisAngleF32 {
	V3F32 axis;
	F32 angle;

	FINLINE QF32 to_quaternion();
};

struct QF32 {
	F32 x, y, z, w;

	FINLINE QF32& from_axis_angle(AxisAngleF32 axisAngle) {
		F32 sinHalfAngle;
		F32 cosHalfAngle = sincosf32(&sinHalfAngle, axisAngle.angle * 0.5F);
		x = axisAngle.axis.x * sinHalfAngle;
		y = axisAngle.axis.y * sinHalfAngle;
		z = axisAngle.axis.z * sinHalfAngle;
		w = cosHalfAngle;
		return *this;
	}

	// https://www.3dgep.com/understanding-quaternions/
	// https://fgiesen.wordpress.com/2019/02/09/rotating-a-single-vector-using-a-quaternion/
	FINLINE V3F32 transform(V3F32 v) {
		F32 x2 = x + x;
		F32 y2 = y + y;
		F32 z2 = z + z;
		F32 tx = (y2 * v.z - z2 * v.y);
		F32 ty = (z2 * v.x - x2 * v.z);
		F32 tz = (x2 * v.y - y2 * v.x);
		return V3F32{
			v.x + w * tx + (y * tz - z * ty),
			v.y + w * ty + (z * tx - x * tz),
			v.z + w * tz + (x * ty - y * tx)
		};
	}

	FINLINE QF32 conjugate() {
		return QF32{ -x, -y, -z, w };
	}

	FINLINE F32 magnitude_sq() {
		return x * x + y * y + z * z + w * w;
	}

	FINLINE F32 magnitude() {
		return sqrtf32(x * x + y * y + z * z + w * w);
	}

	FINLINE QF32 normalize() {
		F32 invMagnitude = 1.0F / magnitude();
		return QF32{ x * invMagnitude, y * invMagnitude, z * invMagnitude, w * invMagnitude };
	}

	FINLINE QF32 inverse() {
		F32 invMagSq = 1.0F / magnitude_sq();
		return QF32{ -x * invMagSq, -y * invMagSq, -z * invMagSq, w * invMagSq };
	}
};

FINLINE QF32 operator*(QF32 a, QF32 b) {
	// General equation for a quaternion product
	// q.w = a.w * b.w - dot(a.v, b.v)
	// q.v = a.w * b.v + b.w * a.v + cross(a.v, b.v);
	// This can be derived by using the form (xi + yj + zk + w) and multiplying two of them together
	return QF32{
		a.w * b.x + b.w * a.x + (a.y * b.z - a.z * b.y),
		a.w * b.y + b.w * a.y + (a.z * b.x - a.x * b.z),
		a.w * b.z + b.w * a.y + (a.x * b.y - a.y * b.x),
		a.w * b.w - (a.x * b.x + a.y * b.y + a.z * b.z)
	};
}

FINLINE QF32 AxisAngleF32::to_quaternion() {
	// A quaternion is { v * sin(theta/2), cos(theta/2) }, where v is the axis and theta is the angle
	F32 sinHalfAngle;
	F32 cosHalfAngle = sincosf32(&sinHalfAngle, angle * 0.5F);
	return QF32{ axis.x * sinHalfAngle, axis.y * sinHalfAngle, axis.z * sinHalfAngle, cosHalfAngle };
}

struct M2F32 {
	F32 m00, m01,
		m10, m11;
	FINLINE M2F32& set_identity() {
		m00 = 1.0F;
		m01 = 0.0F;
		m10 = 0.0F;
		m11 = 1.0F;
		return *this;
	}
	FINLINE M2F32& set_zero() {
		m00 = 0.0F;
		m01 = 0.0F;
		m10 = 0.0F;
		m11 = 0.0F;
		return *this;
	}
	FINLINE M2F32& set(const M2F32& other) {
		m00 = other.m00;
		m01 = other.m01;
		m10 = other.m10;
		m11 = other.m11;
		return *this;
	}
	FINLINE M2F32& rotate(F32 angle) {
		F32 s;
		F32 c = sincosf32(&s, angle);
		F32 tmp00 = m00 * c + m01 * s;
		F32 tmp01 = m10 * c + m11 * s;
		F32 tmp10 = m00 * -s + m01 * c;
		F32 tmp11 = m10 * -s + m11 * c;
		m00 = tmp00;
		m01 = tmp01;
		m10 = tmp10;
		m11 = tmp11;
		return *this;
	}
	FINLINE M2F32& transpose() {
		F32 tmp = m10;
		m10 = m01;
		m01 = tmp;
		return *this;
	}
	FINLINE V2F32 transform(V2F32 vec) {
		F32 x = m00 * vec.x + m01 * vec.y;
		F32 y = m10 * vec.x + m11 * vec.y;
		return V2F32{ x, y };
	}
	FINLINE V2F32 operator*(V2F32 vec) {
		return transform(vec);
	}
};

// I don't think this counts as a 4x3 matrix, it's really a 4x4 matrix with the 4th row assumed to be 0, 0, 0, 1
// I should come up with a better name for this
#pragma pack(push, 1)
struct M4x3F32 {
	F32 m00, m01, m02, x,
		m10, m11, m12, y,
		m20, m21, m22, z;

	FINLINE M4x3F32 copy() {
		return *this;
	}

	FINLINE M4x3F32& set_zero() {
		m00 = 0.0F;
		m01 = 0.0F;
		m02 = 0.0F;
		m10 = 0.0F;
		m11 = 0.0F;
		m12 = 0.0F;
		m20 = 0.0F;
		m21 = 0.0F;
		m22 = 0.0F;
		x = 0.0F;
		y = 0.0F;
		z = 0.0F;
		return *this;
	}

	FINLINE M4x3F32& set_identity() {
		m00 = 1.0F;
		m01 = 0.0F;
		m02 = 0.0F;
		m10 = 0.0F;
		m11 = 1.0F;
		m12 = 0.0F;
		m20 = 0.0F;
		m21 = 0.0F;
		m22 = 1.0F;
		x = 0.0F;
		y = 0.0F;
		z = 0.0F;
		return *this;
	}

	FINLINE M4x3F32& transpose_rotation() {
		F32 tmp0 = m01;
		F32 tmp1 = m02;
		F32 tmp2 = m12;
		m01 = m10;
		m02 = m20;
		m12 = m21;
		m10 = tmp0;
		m20 = tmp1;
		m21 = tmp2;
		return *this;
	}

	// This is a simplification of qf32.rotate(), substituting in (1, 0, 0), (0, 1, 0), and (0, 0, 1) as the vectors to rotate
	// Pretty much we just rotate the basis vectors of the identity matrix
	FINLINE M4x3F32& set_orientation_from_quat(QF32 q) {
		F32 yy2 = 2.0F * q.y * q.y;
		F32 zz2 = 2.0F * q.z * q.z;
		F32 zw2 = 2.0F * q.z * q.w;
		F32 xy2 = 2.0F * q.x * q.y;
		F32 xz2 = 2.0F * q.x * q.z;
		F32 yw2 = 2.0F * q.y * q.w;
		F32 xx2 = 2.0F * q.x * q.x;
		F32 xw2 = 2.0F * q.x * q.w;
		F32 yz2 = 2.0F * q.y * q.z;
		m00 = 1.0F - yy2 - zz2;
		m10 = zw2 + xy2;
		m20 = xz2 - yw2;
		m01 = xy2 - zw2;
		m11 = 1.0F - zz2 - xx2;
		m21 = xw2 + yz2;
		m02 = yw2 + xz2;
		m12 = yz2 - xw2;
		m22 = 1.0F - xx2 - yy2;
		return *this;
	}

	FINLINE M4x3F32& set_offset(V3F32 offset) {
		x = offset.x;
		y = offset.y;
		z = offset.z;
		return *this;
	}

	FINLINE M4x3F32& translate(V3F32 offset) {
		x += m00 * offset.x + m01 * offset.y + m02 * offset.z;
		y += m10 * offset.x + m11 * offset.y + m12 * offset.z;
		z += m20 * offset.x + m21 * offset.y + m22 * offset.z;
		return *this;
	}

	FINLINE M4x3F32& add_offset(V3F32 offset) {
		x += offset.x;
		y += offset.y;
		z += offset.z;
		return *this;
	}

	FINLINE M4x3F32& rotate_quat(QF32 q) {
		V3F32 row0 = q.transform(V3F32{ m00, m01, m02 });
		V3F32 row1 = q.transform(V3F32{ m10, m11, m12 });
		V3F32 row2 = q.transform(V3F32{ m20, m21, m22 });
		m00 = row0.x;
		m01 = row0.y;
		m02 = row0.z;
		m10 = row1.x;
		m11 = row1.y;
		m12 = row1.z;
		m20 = row2.x;
		m21 = row2.y;
		m22 = row2.z;
		return *this;
	}

	FINLINE M4x3F32& rotate_axis_angle(V3F32 axis, F32 angle) {
		return rotate_quat(QF32{}.from_axis_angle(AxisAngleF32{ axis, angle }));
	}

	FINLINE M4x3F32& scale(V3F32 scaling) {
		m00 *= scaling.x;
		m10 *= scaling.x;
		m20 *= scaling.x;
		m01 *= scaling.y;
		m11 *= scaling.y;
		m21 *= scaling.y;
		m02 *= scaling.z;
		m12 *= scaling.z;
		m22 *= scaling.z;
		return *this;
	}

	FINLINE M4x3F32& scale_global(V3F32 scaling) {
		m00 *= scaling.x;
		m10 *= scaling.x;
		m20 *= scaling.x;
		x *= scaling.x;
		m01 *= scaling.y;
		m11 *= scaling.y;
		m21 *= scaling.y;
		y *= scaling.y;
		m02 *= scaling.z;
		m12 *= scaling.z;
		m22 *= scaling.z;
		z *= scaling.z;
		return *this;
	}

	FINLINE F32 determinant_upper_left_3x3_corner() {
		return m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) + m02 * (m10 * m21 - m11 * m20);
	}

	FINLINE M4x3F32& invert() {
		// 22 mul, 17 fma, 3 add, 1 div
		// Calculate the minor for each element in the 3x3 upper left corner, multiplied by the cofactor
		F32 t00 = m11 * m22 - m12 * m21;
		F32 t01 = m12 * m20 - m10 * m22;
		F32 t02 = m10 * m21 - m11 * m20;
		F32 t10 = m02 * m21 - m01 * m22;
		F32 t11 = m00 * m22 - m02 * m20;
		F32 t12 = m01 * m20 - m00 * m21;
		F32 t20 = m01 * m12 - m02 * m11;
		F32 t21 = m02 * m10 - m00 * m12;
		F32 t22 = m00 * m11 - m01 * m10;

		F32 inverseDet = 1.0F / (m00 * t00 + m01 * t01 + m02 * t02);

		// Transpose and multiply by inverse determinant
		m00 = inverseDet * t00;
		m01 = inverseDet * t10;
		m02 = inverseDet * t20;
		m10 = inverseDet * t01;
		m11 = inverseDet * t11;
		m12 = inverseDet * t21;
		m20 = inverseDet * t02;
		m21 = inverseDet * t12;
		m22 = inverseDet * t22;

		F32 tx = x;
		F32 ty = y;
		F32 tz = z;
		// Inverse translation part
		x = -m00 * tx - m01 * ty - m02 * tz;
		y = -m10 * tx - m11 * ty - m12 * tz;
		z = -m20 * tx - m21 * ty - m22 * tz;

		return *this;
	}

	// This function is untested, and I'm not actually sure if I did the math right
	FINLINE M4x3F32& invert_orthogonal() {
		// 12 mul, 6 fma, 3 add, 3 rsqrt
		F32 invXScale = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(m00 * m00 + m01 * m01 + m02 * m02)));
		F32 invYScale = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(m10 * m10 + m11 * m11 + m12 * m12)));
		F32 invZScale = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(m20 * m20 + m21 * m21 + m22 * m22)));
		invXScale *= invXScale;
		invYScale *= invYScale;
		invZScale *= invZScale;

		F32 t01 = m01;
		F32 t02 = m02;
		F32 t12 = m12;

		// Multiply by inverse scale squared and transpose
		m00 = m00 * invXScale;
		m01 = m10 * invYScale;
		m02 = m20 * invZScale;
		m10 = t01 * invXScale;
		m11 = m11 * invYScale;
		m12 = m21 * invZScale;
		m20 = t02 * invXScale;
		m21 = t12 * invYScale;
		m22 = m22 * invZScale;

		F32 tx = x;
		F32 ty = y;
		F32 tz = z;
		// Inverse translation part
		x = -m00 * tx - m01 * ty - m02 * tz;
		y = -m10 * tx - m11 * ty - m12 * tz;
		z = -m20 * tx - m21 * ty - m22 * tz;
		return *this;
	}

	FINLINE M4x3F32& invert_orthonormal() {
		transpose_rotation();
		F32 tx = x;
		F32 ty = y;
		F32 tz = z;
		// Inverse translation part
		x = -m00 * tx - m01 * ty - m02 * tz;
		y = -m10 * tx - m11 * ty - m12 * tz;
		z = -m20 * tx - m21 * ty - m22 * tz;
		return *this;
	}

	FINLINE V3F32 transform(V3F32 vec) const {
		return V3F32{
			x + m00 * vec.x + m01 * vec.y + m02 * vec.z,
			y + m10 * vec.x + m11 * vec.y + m12 * vec.z,
			z + m20 * vec.x + m21 * vec.y + m22 * vec.z
		};
	}

	FINLINE V3F32 get_row(U32 idx) {
		if (idx == 0) {
			return V3F32{ m00, m01, m02 };
		} else if (idx == 1) {
			return V3F32{ m10, m11, m12 };
		} else {
			return V3F32{ m20, m21, m22 };
		}
	}

	FINLINE M4x3F32& set_row(U32 idx, V3F32 row) {
		if (idx == 0) {
			m00 = row.x;
			m01 = row.y;
			m02 = row.z;
		} else if (idx == 1) {
			m10 = row.x;
			m11 = row.y;
			m12 = row.z;
		} else {
			m20 = row.x;
			m21 = row.y;
			m22 = row.z;
		}
		return *this;
	}
};
#pragma pack(pop)

FINLINE M4x3F32 operator*(const M4x3F32& a, const M4x3F32& b) {
	M4x3F32 dst;
	dst.m00 = a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20;
	dst.m01 = a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21;
	dst.m02 = a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22;
	dst.x = a.m00 * b.x + a.m01 * b.y + a.m02 * b.z + a.x;
	dst.m10 = a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20;
	dst.m11 = a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21;
	dst.m12 = a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22;
	dst.y = a.m10 * b.x + a.m11 * b.y + a.m12 * b.z + a.y;
	dst.m20 = a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20;
	dst.m21 = a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21;
	dst.m22 = a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22;
	dst.z = a.m20 * b.x + a.m21 * b.y + a.m22 * b.z + a.z;
	return dst;
}

FINLINE V3F32 operator*(const M4x3F32& a, const V3F32& b) {
	return a.transform(b);
}

DEBUG_OPTIMIZE_OFF

void println_m4x3f32(M4x3F32 m) {
	print("[");
	print_float(m.m00);
	print(", ");
	print_float(m.m01);
	print(", ");
	print_float(m.m02);
	print(", ");
	print_float(m.x);
	print("]\n");
	print("[");
	print_float(m.m10);
	print(", ");
	print_float(m.m11);
	print(", ");
	print_float(m.m12);
	print(", ");
	print_float(m.y);
	print("]\n");
	print("[");
	print_float(m.m20);
	print(", ");
	print_float(m.m21);
	print(", ");
	print_float(m.m22);
	print(", ");
	print_float(m.z);
	print("]\n\n");
}

struct PerspectiveProjection {
	F32 xScale;
	F32 xZBias;
	F32 yScale;
	F32 yZBias;
	F32 nearPlane;

	// I decided to think about this intuitively for once instead of just copying some standard projection matrix diagram
	// This is my thought process in case I have to debug it later
	//
	// So we start the fov in 4 directions and a nearplane
	// For each fov direction, we can use sin and cosine to get the direction vector from the angle, then project that onto a plane at z=1
	// Getting the coordinate that isn't 1 is all that matters here, so we can scale the x/y component by the inverse of the one pointing in the z direction
	// This results in sin over cos telling us where the projected vectors extend to on the z=1 plane
	// All four of these numbers defines a rectangle on the z=1 plane
	// r and l will be the right and left x coords of the rectangle and u and d will be the up and down y coords of the rectangle
	// Now, to project, we want to project everything onto the rectangle through the view point, scaling everything down with distance linearly
	// I'll consider only the x coordinate first, since the y should be similar
	// We want to scale to the center of the rectangle, so first off, we have to translate the space so the center of the rectangle is at x=0
	// xCenter = (r + l) / 2
	// x = x - xCenter
	// This is a linear translation, but to transform the frustum space correctly, it needs to be perspective. We can simply scale up the translation value with distance from camera
	// x = x - z * xCenter
	// Now, things at Z = 1 should be centered on x = 0
	// To get the l to r range into the -1 to 1 NDC range, we can divide out half the distance from l to r, scaling the rectangle to a -1 to 1 square
	// halfXRange = (r - l) / 2
	// x = (x - z * xCenter) / halfXRange
	// This works for a skewed orthographic projection, but since we want perspective, we still need to scale stuff down
	// Something twice as far away should scale down twice as much, so we can divide by z to scale things linearly with distance
	// x = (x - z * xCenter) / (z * halfXRange)
	// x = (x - z * (r + l) * 0.5) / (z * (r - l) * 0.5)
	// The y axis should be quite similar
	// y = (y - z * (u + d) * 0.5) / (z * (u - d) * 0.5)
	// Rearranging so we get easy constants and divide by z at the end so we can use that as a w coordinate in a shader
	// x = ((x - z * (r + l) * 0.5) / ((r - l) * 0.5)) / z
	// x = ((x / ((r - l) * 0.5)) - (z * (r + l) * 0.5) / ((r - l) * 0.5)) / z
	// x = (x * (1 / ((r - l) * 0.5)) - z * ((r + l) / (r - l))) / z
	// y = (y * (1 / ((u - d) * 0.5)) - z * ((u + d) / (u - d))) / z
	// Since we're using reverse z, our resulting depth should decrease as distance increases, in order to get better resolution at distance
	// z = 1 / z
	// This puts things less than z=1 at a depth greater than 1 (out of bounds), but we'd like things less than the near plane to be out of bounds, so we can scale the depth down by the near plane
	// z = nearPlane / z
	// Alright, that's all our values, with a division by z at the end, so we can just stick that in the w coordinate in the shader so the division happens automatically
	// This gives us 5 constants to pass
	// xScale = 1 / ((r - l) * 0.5)
	// xZBias = (r + l) / (r - l)
	// yScale = 1 / ((u - d) * 0.5)
	// yZBias = (u + d) / (u - d)
	// nearPlane = nearPlane
	FINLINE void project_perspective(F32 nearZ, F32 fovRight, F32 fovLeft, F32 fovUp, F32 fovDown) {
		F32 sinRight;
		F32 cosRight = sincosf32(&sinRight, fovRight);
		F32 rightX = sinRight / cosRight;
		F32 sinLeft;
		F32 cosLeft = sincosf32(&sinLeft, fovLeft);
		F32 leftX = sinLeft / cosLeft;
		F32 sinUp;
		F32 cosUp = sincosf32(&sinUp, fovUp);
		F32 upY = sinUp / cosUp;
		F32 sinDown;
		F32 cosDown = sincosf32(&sinDown, fovDown);
		F32 downY = sinDown / cosDown;
		xScale = 1.0F / ((rightX - leftX) * 0.5F);
		xZBias = (rightX + leftX) / (rightX - leftX);
		yScale = 1.0F / ((upY - downY) * 0.5F);
		yZBias = (upY + downY) / (upY - downY);
		nearPlane = nearZ;
	}

	FINLINE V3F32 transform(V3F32 vec) {
		F32 x = vec.x * xScale + vec.z * xZBias;
		F32 y = vec.y * yScale + vec.z * yZBias;
		F32 z = nearPlane;
		F32 invZ = -1.0F / vec.z;
		return V3F32{ x * invZ, y * invZ, z * invZ };
	}
};

// Projective matrices only output the near plane to the z component, so we can optimize away the third row of the 4x4 matrix
struct ProjectiveTransformMatrix {
	F32 m00, m01, m02, m03,
		m10, m11, m12, m13,
		//m20, m21, m22, m23,
		m30, m31, m32, m33;

	FINLINE void generate(PerspectiveProjection projection, M4x3F32 transform) {
		// projectiveTransformMatrix = projectionMatrix * transformMatrix
		m00 = transform.m00 * projection.xScale + transform.m20 * projection.xZBias;
		m01 = transform.m01 * projection.xScale + transform.m21 * projection.xZBias;
		m02 = transform.m02 * projection.xScale + transform.m22 * projection.xZBias;
		m03 = transform.x * projection.xScale + transform.z * projection.xZBias;
		m10 = transform.m10 * projection.yScale + transform.m20 * projection.yZBias;
		m11 = transform.m11 * projection.yScale + transform.m21 * projection.yZBias;
		m12 = transform.m12 * projection.yScale + transform.m22 * projection.yZBias;
		m13 = transform.y * projection.yScale + transform.z * projection.yZBias;
		m30 = -transform.m20;
		m31 = -transform.m21;
		m32 = -transform.m22;
		m33 = -transform.z;
	}
};

DEBUG_OPTIMIZE_ON

#pragma pack(push, 1)
struct RGBA8 {
	U8 r, g, b, a;

	B32 operator==(const RGBA8& other) {
		return r == other.r && g == other.g && b == other.b && a == other.a;
	}
	V4F32 to_v4f32() {
		F32 scale = 1.0F / 255.0F;
		return V4F32{ F32(r) * scale, F32(g) * scale, F32(b) * scale, F32(a) * scale };
	}
	RGBA8 to_rgba8() {
		return *this;
	}
};
RGBA8 V4F32::to_rgba8() {
	return RGBA8{ U8(clamp01(x) * 255.0F), U8(clamp01(y) * 255.0F), U8(clamp01(z) * 255.0F), U8(clamp01(w) * 255.0F) };
}
struct RGB8 {
	U8 r, g, b;

	B32 operator==(const RGB8& other) {
		return r == other.r && g == other.g && b == other.b;
	}
	RGB8 operator+(RGB8 other) {
		return RGB8{ U8(r + other.r), U8(g + other.g), U8(b + other.b) };
	}
	RGB8 operator-(RGB8 other) {
		return RGB8{ U8(r - other.r), U8(g - other.g), U8(b - other.b) };
	}
	RGBA8 to_rgba8() {
		return RGBA8{ r, g, b, 255 };
	}
};
struct RG8 {
	U8 r, g;

	B32 operator==(RG8 other) {
		return r == other.r && g == other.g;
	}
	RGBA8 to_rgba8() {
		return RGBA8{ r, 0, 0, g };
	}
};
struct R8 {
	U8 r;

	B32 operator==(R8 other) {
		return r == other.r;
	}
	RGBA8 to_rgba8() {
		return RGBA8{ r, 0, 0, 255 };
	}
};
#pragma pack(pop)


enum Direction1 : I32 {
	DIRECTION1_INVALID = -1,
	DIRECTION1_LEFT = 0,
	DIRECTION1_RIGHT = 1,
	DIRECTION1_Count = 2
};
enum Direction2 : I32 {
	DIRECTION2_INVALID = -1,
	DIRECTION2_LEFT = 0,
	DIRECTION2_RIGHT = 1,
	DIRECTION2_FRONT = 2,
	DIRECTION2_BACK = 3,
	DIRECTION2_Count = 4
};
enum Direction3 : I32 {
	DIRECTION3_INVALID = -1,
	DIRECTION3_LEFT = 0,
	DIRECTION3_RIGHT = 1,
	DIRECTION3_FRONT = 2,
	DIRECTION3_BACK = 3,
	DIRECTION3_UP = 4,
	DIRECTION3_DOWN = 5,
	DIRECTION3_Count = 6
};

enum Axis2 : I32 {
	AXIS2_INVALID = -1,
	AXIS2_X = 0,
	AXIS2_Y = 1,
	AXIS2_Count = 2
};

FINLINE Axis2 axis2_orthogonal(Axis2 axis) {
	if (axis == AXIS2_X) {
		return AXIS2_Y;
	} else if (axis == AXIS2_Y) {
		return AXIS2_X;
	} else {
		return axis;
	}
}

enum Axis3 : I32 {
	AXIS3_INVALID = -1,
	AXIS3_X = 0,
	AXIS3_Y = 1,
	AXIS3_Z = 2,
	AXIS3_Count = 3
};

#pragma pack(push, 1)
struct Rng1F32 {
	F32 minX, maxX;

	void init(F32 mnX, F32 mxX) {
		minX = min(mnX, mxX);
		maxX = max(mnX, mxX);
	}
	F32 midpoint() {
		return (minX + maxX) * 0.5F;
	}
};
#pragma pack(pop)
FINLINE Rng1F32 rng_union(Rng1F32 a, Rng1F32 b) {
	return Rng1F32{ min(a.minX, b.minX), max(a.maxX, b.maxX) };
}
FINLINE Rng1F32 rng_intersect(Rng1F32 a, Rng1F32 b) {
	Rng1F32 rng{ max(a.minX, b.minX), min(a.maxX, b.maxX) };
	return rng.maxX < rng.minX ? Rng1F32{} : rng;;
}
FINLINE F32 rng_area(Rng1F32 rng) {
	return rng.maxX - rng.minX;
}
FINLINE B32 rng_contains_point(Rng1F32 rng, F32 v) {
	return v >= rng.minX && v <= rng.maxX;
}

#pragma pack(push, 1)
struct Rng2F32 {
	F32 minX, minY, maxX, maxY;

	V2F32 midpoint() {
		return V2F32{ (minX + maxX) * 0.5F, (minY + maxY) * 0.5F };
	}
};
#pragma pack(pop)
FINLINE Rng2F32 rng_union(Rng2F32 a, Rng2F32 b) {
	return Rng2F32{ min(a.minX, b.minX), min(a.minY, b.minY), max(a.maxX, b.maxX), max(a.maxY, b.maxY) };
}
FINLINE Rng2F32 rng_intersect(Rng2F32 a, Rng2F32 b) {
	Rng2F32 rng{ max(a.minX, b.minX), max(a.minY, b.minY), min(a.maxX, b.maxX), min(a.maxY, b.maxY) };
	return rng.maxX < rng.minX || rng.maxY < rng.minY ? Rng2F32{} : rng;
}
FINLINE F32 rng_area(Rng2F32 rng) {
	return (rng.maxX - rng.minX) * (rng.maxY - rng.minY);
}
FINLINE B32 rng_contains_point(Rng2F32 rng, V2F32 v) {
	return v.x >= rng.minX && v.x <= rng.maxX && v.y >= rng.minY && v.y <= rng.maxY;
}

#pragma pack(push, 1)
struct Rng3F32 {
	F32 minX, minY, minZ, maxX, maxY, maxZ;

	V3F32 midpoint() {
		return V3F32{ (minX + maxX) * 0.5F, (minY + maxY) * 0.5F, (minZ + maxZ) * 0.5F };
	}
};
#pragma pack(pop)
FINLINE Rng3F32 rng_union(Rng3F32 a, Rng3F32 b) {
	return Rng3F32{ min(a.minX, b.minX), min(a.minY, b.minY), min(a.minZ, b.minZ), max(a.maxX, b.maxX), max(a.maxY, b.maxY), max(a.maxZ, b.maxZ) };
}
FINLINE Rng3F32 rng_intersect(Rng3F32 a, Rng3F32 b) {
	Rng3F32 rng{ max(a.minX, b.minX), max(a.minY, b.minY), max(a.minZ, b.minZ), min(a.maxX, b.maxX), min(a.maxY, b.maxY), min(a.maxZ, b.maxZ) };
	return rng.maxX < rng.minX || rng.maxY < rng.minY || rng.maxZ < rng.minZ ? Rng3F32{} : rng;
}
FINLINE F32 rng_area(Rng3F32 rng) {
	return (rng.maxX - rng.minX) * (rng.maxY - rng.minY) * (rng.maxZ - rng.minZ);
}
FINLINE B32 rng_contains_point(Rng3F32 rng, V3F32 v) {
	return v.x >= rng.minX && v.x <= rng.maxX && v.y >= rng.minY && v.y <= rng.maxY && v.z >= rng.minZ && v.z <= rng.maxZ;
}


FINLINE U32 pack_unorm4x8(V4F32 v) {
	return U32(v.x * 255.0F) | U32(v.y * 255.0F) << 8 | U32(v.z * 255.0F) << 16 | U32(v.w * 255.0F) << 24;
}

template <typename T>
FINLINE T eval_bezier_quadratic(T start, T control, T end, F32 t) {
	// What you get if you multiply everything out. Should be faster than the intuitive version
	F32 invT = 1.0F - t;
	F32 tSq = t * t;
	F32 invTSq = invT * invT;
	return invTSq * start + (2.0F * invT * t) * control + tSq * end;
}
template<typename T>
FINLINE T eval_bezier_cubic(T start, T controlA, T controlB, T end, F32 t) {
	// What you get if you multiply everything out. Should be faster than the intuitive version
	F32 invT = 1.0F - t;
	F32 tSq = t * t;
	F32 invTSq = invT * invT;
	F32 tCu = tSq * t;
	F32 invTCu = invTSq * invT;
	return invTCu * start + (3.0F * invTSq * t) * controlA + (3.0F * tSq * invT) * controlB + tCu * end;
}

DEBUG_OPTIMIZE_OFF