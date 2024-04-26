#pragma once

#include "DrillLib.h"

namespace SerializeTools {

FINLINE B32 is_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}
FINLINE B32 is_digit(char c) {
	return c >= '0' && c <= '9';
}
FINLINE B32 is_hex_digit(char c) {
	return c >= '0' && c <= '9' || c >= 'A' && c <= 'F' || c >= 'a' && c <= 'f';
}
FINLINE B32 is_alpha(char c) {
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z';
}
FINLINE B32 is_upper_alpha(char c) {
	return c >= 'A' && c <= 'Z';
}
FINLINE B32 is_lower_alpha(char c) {
	return c >= 'a' && c <= 'z';
}

FINLINE StrA& skip_whitespace(StrA* str) {
	while (str->length && is_whitespace(str->str[0])) {
		str->str++, str->length--;
	}
	return *str;
}

const I32 POWER_OF_5_TABLE_OFFSET = 342;
const U64 POWER_OF_5_TABLE[]{
#include "SerializeToolsPowerOf5Table.txt"
};

// Implemented from "Number Parsing at a Gigabyte per Second" (Daniel Lemire)
// Slightly modified to make implementation easier, trading off a small amount of accuracy
// I don't care very much if we sometimes round the wrong way
// https://arxiv.org/pdf/2101.11408.pdf
B32 parse_f64(F64* f64Out, StrA* str) {
	skip_whitespace(str);
	if (str->length == 0) {
		return false;
	}
	const char* src = str->str;
	U64 srcLen = str->length;

	// Parse the raw numbers

	B32 isNegative = false;
	if (src[0] == '-' || src[0] == '+') {
		isNegative = src[0] == '-';
		src++, srcLen--;
	}

	U64 number = 0;
	U32 significandDigitCount = 0;
	U32 totalDigitCount = 0;
	I32 exponent = 0;
	while (srcLen && src[0] == '0') {
		src++, srcLen--, totalDigitCount++;
	}
	const U32 maxBase10DigitsThatCanFitIn64Bits = 19;
	// Both the integer and decimal parts are parsed as a single integer, with the exponent corrected to compensate
	while (srcLen && is_digit(src[0])) {
		if (significandDigitCount < maxBase10DigitsThatCanFitIn64Bits) {
			number = number * 10ull + U64(src[0] - '0');
			significandDigitCount++;
		} else {
			exponent++;
		}
		src++, srcLen--, totalDigitCount++;
	}
	if (srcLen && src[0] == '.') {
		src++, srcLen--;
		while (srcLen && is_digit(src[0])) {
			if (significandDigitCount < maxBase10DigitsThatCanFitIn64Bits) {
				number = number * 10ull + U64(src[0] - '0');
				exponent--;
				significandDigitCount++;
			}
			src++, srcLen--, totalDigitCount++;
		}
	}

	if (totalDigitCount == 0) {
		// Must have at least one digit
		return false;
	}

	if (srcLen && (src[0] == 'e' || src[0] == 'E')) {
		src++, srcLen--;
		B32 exponentNegative = false;
		I32 parsedExponent = 0;
		if (srcLen && (src[0] == '-' || src[0] == '+')) {
			exponentNegative = src[0] == '-';
			src++, srcLen--;
		}
		if (srcLen == 0 || !is_digit(src[0])) {
			// If there's an E, must have some numbers
			return false;
		}
		while (srcLen && is_digit(src[0])) {
			if (parsedExponent < 200000000) {
				parsedExponent = parsedExponent * 10 + (src[0] - '0');
			} else {
				// Avoid overflow, exponent too long
				return false;
			}
			src++, srcLen--;
		}
		if (exponentNegative) {
			exponent -= parsedExponent;
		} else {
			exponent += parsedExponent;
		}
	}
	*str = StrA{ src, srcLen };

	// Done parsing the raw numbers, convert them to an F64

	if (number == 0 || exponent < -342) {
		*f64Out = isNegative ? -0.0 : 0.0;
		return true;
	}
	if (exponent > 308) {
		*f64Out = isNegative ? -F64_INF : F64_INF;
		return true;
	}

	// The base 10 to base 2 conversion algorithm is based on the idea that
	// s * 10^e == s * 5^e * 2^e, which means we can have the number in base 2 exponential form as long as we know what s * 5^e is.
	// So, we have a table of all powers of 5^e from -342 to 308, since F64s can't represent numbers out of that range
	// For simplicity (and efficiency), the power of 5 table only actually contains the most significant 128 bits of the powers of 5.
	// This is because we only need 54 significant bits, and lower bits of the power of 5 will only ever contribute as a carry into those bits
	// The only reason we have the secondary lower 64 bits is to resolve the rounding correctly the majority of the time. After that, I stop caring about having a tiny error.
	// The table's entries are also shifted such that the MSB is always 1, for reasons described below.

	// For positive powers of 5, generating the table is simple, just do the exponent with a multi precision integer and take the highest 128 significant bits
	// For negative powers of 5, the multiply by inverse trick can be used, where you represent n/d as (n * (2^t / d)) / (2^t), which works as long as you choose the value of 2^t
	// to be greater than (d - 1) * N, where N is the maximum possible numerator (2^64 - 1 in this case)
	// This becomes (n * c) >> t, and since we only care about the most significant bits, we can ignore the shift and just do the multiply


	U64 leadingZeros = lzcnt64(number);
	// Make sure the MSB has a 1
	// Since both the power of 5 and this have an MSB of 1, that means the multiplied result will have at least one of the two MSBs set
	// This makes it easy to find the 54 most significant bits for the significand
	// It doesn't actually matter what the multipliers are shifted by, as long as we can find the 54 MSBs of the result.
	number <<= leadingZeros;

	U64 powerOf5MostSignificantBits = POWER_OF_5_TABLE[(POWER_OF_5_TABLE_OFFSET + exponent) * 2 + 0];
	U64 powerOf5NextMostSignificantBits = POWER_OF_5_TABLE[(POWER_OF_5_TABLE_OFFSET + exponent) * 2 + 1];

	U64 truncatedProductHigh;
	U64 truncatedProductLow = _umul128(powerOf5MostSignificantBits, number, &truncatedProductHigh);
	const U64 lower9Bits = (1 << 9) - 1;
	if ((truncatedProductHigh & lower9Bits) == lower9Bits) {
		U64 truncatedProductNextHigh;
		_umul128(powerOf5NextMostSignificantBits, number, &truncatedProductNextHigh);
		truncatedProductHigh += U64(_addcarry_u64(0, truncatedProductLow, truncatedProductNextHigh, &truncatedProductLow));
	}

	U64 truncatedProductMSB = truncatedProductHigh >> 63ull;
	// Extracting the most significant 54 bits
	U64 mostSignificantBits = truncatedProductHigh >> (64ull - 54ull - (1ull - truncatedProductMSB));

	// Finding the binary exponent (in a nutshell, read the paper for a real explanation)
	// For 10^e, this splits into 5^e * 2^e
	// We want to make the 5^e part between 1 and 2, since that's how the significand is defined in floating point
	// For e >= 0, 5^e * 2^e = 5^e / 2^floor(log_2(5^q)) * 2^(q + floor(log_2(5^q)))	
	// For e < 0, 5^e * 2^e = 2^ceil(log_2(5^-e)) / 5^-e * 2^(e-ceil(log_2(5^-e)))
	// So, for getting 5^e between 1 and 2, we get a binary exponent of e + floor(log_2(5^e)) == e - ceil(log_2(5^-e)) == e + trunc(log_2(5^e))
	// e + log_2(5^e) == e + e * log_2(5) == e * (1 + log_2(5)) ~= e * 3.321928
	// To do that in integer math, we can scale by 2^16: (e * (1 + log_2(5) * 2^16) / 2^16
	// round(log_2(5) * 2^16) == 217706. This integer approximation is valid for all base 10 exponents that are valid for F64s.
	// So now we have the binary exponent as (217706 * exponent >> 16).
	// This will correct the normalized power of 5 to the actual power of 5, but since the power of 5 is multiplied with the significand, we also need to
	// correct the normalized product to the significand's magnitude
	// The significand has a magnitude of 2^63, since the MSB is always 1, so we also need to increase the exponent by that amount.
	// (217706 * exponent >> 16) + 63
	// However, we also artificially shifted the significand such that the MSB would be 1, so we need to remove that shift from the exponent as well
	// (217706 * exponent >> 16) + 63 - leadingZeros
	// The significand times the power of 5 can also overflow and take on one more bit, so if the MSB of the product is set we need to correct by 1 more in the exponent
	// (217706 * exponent >> 16) + 63 - leadingZeros + truncatedProductMSB
	I32 expectedBinaryExponent = (217706 * exponent >> 16) + 63 - I32(leadingZeros) + I32(truncatedProductMSB);

	if (expectedBinaryExponent <= -1022 - 64) {
		// Too small for even a subnormal
		*f64Out = isNegative ? -0.0 : 0.0;
		return true;
	}
	if (expectedBinaryExponent <= -1022) {
		// Subnormals
		mostSignificantBits = ((mostSignificantBits >> U64(-1022 - expectedBinaryExponent + 1)) + 1ull) >> 1ull;
		// Mask off bit 52, it's implicitly 1
		*f64Out = bitcast<F64>(U64(isNegative) << 63ull | (mostSignificantBits & ~(1ull << 52ull)));
		return true;
	}

	// Skip rounding ties, I don't care about perfect rounding

	mostSignificantBits = (mostSignificantBits + 1ull) >> 1ull;
	if (mostSignificantBits == (1ull << 53ull)) {
		// The rounding addition overflowed into the next exponent
		mostSignificantBits >>= 1ull;
		expectedBinaryExponent += 1;
	}

	if (expectedBinaryExponent > 1023) {
		*f64Out = isNegative ? -F64_INF : F64_INF;
		return true;
	}

	// Mask off bit 52, it's implicitly 1
	*f64Out = bitcast<F64>(U64(isNegative) << 63ull | U64(expectedBinaryExponent + 1023) << 52ull | (mostSignificantBits & ~(1ull << 52ull)));
	return true;
}
B32 parse_f32(F32* f32Out, StrA* str) {
	F64 f64;
	B32 result = parse_f64(&f64, str);
	*f32Out = F32(f64);
	return result;
}

void serialize_f64(char* dstBuffer, U32* dstBufferSize, F64 startValue) {
	U32 dstBufferCapacity = *dstBufferSize;
	*dstBufferSize = 0;
	B32 negative = B32(bitcast<U64>(startValue) >> 63ull);
	U64 significand = bitcast<U64>(startValue) & (1ull << 52ull) - 1ull;
	I32 exponent = I32(bitcast<U64>(startValue) >> 52ull & (1ull << 11ull) - 1ull) - 1023;
	if (negative && dstBufferCapacity) {
		dstBufferCapacity--;
		(*dstBufferSize)++;
		*dstBuffer++ = '-';
	}
	if (significand == 0 && exponent == -1023) {
		memcpy(dstBuffer, "0.0", min(dstBufferCapacity, 3u));
		*dstBufferSize += min(dstBufferCapacity, 3u);
		return;
	}
	if (exponent == -1023) {
		// Subnormal
		U32 normalizeFactor = 53 - (64 - _lzcnt_u64(significand));
		exponent -= normalizeFactor;
		significand <<= normalizeFactor;
	} else if (exponent == 1024) {
		// Inf or nan
		if (significand != 0) {
			if (significand & 1ull << 51ull) {
				memcpy(dstBuffer, "QNaN", min(dstBufferCapacity, 4u));
				*dstBufferSize += min(dstBufferCapacity, 4u);
			} else {
				memcpy(dstBuffer, "SNaN", min(dstBufferCapacity, 4u));
				*dstBufferSize += min(dstBufferCapacity, 4u);
			}
		} else {
			memcpy(dstBuffer, "Inf", min(dstBufferCapacity, 3u));
			*dstBufferSize += min(dstBufferCapacity, 3u);
		}
		return;
	} else {
		// Add back the implicit 1 bit
		significand |= 1ull << 52ull;
	}
	exponent -= 52;

	// Implementation of Dragonbox (by Junekey Jeon)
	// https://github.com/jk-jeon/dragonbox/blob/master/other_files/Dragonbox.pdf
	// The basic idea here is that we scale powers of 10 into the power of 2 range.
	// If any of the powers of 10 are within our interval of numbers that will round to the source number, we go with that
	// Otherwise, we try again with the next smaller power of 10, which is guaranteed to have a number in our interval

	// Assume round to even
	B32 leftBoundInInterval = (significand & 1) == 0;
	B32 rightBoundInInterval = (significand & 1) == 0;

	U64 base10Digits;
	I32 base10Exponent;
	if (significand != 1ull << 52ull || exponent <= -1023 - 52 + 1) {
		// Rounding interval is always 2^e in this case
		// Part 1

		// Step 1: compute k
		// I did not come up with these names, they are named like this to reflect the paper
		// This code is not going to be readable whether I name things well or not, go read the paper for an actual explanation
		const I32 kappa = 2;
		const U64 tenToTheKappaPower = 100;
		const U64 tenToTheKappaPlusOnePower = 1000;
		// k = kappa - floor(e * log_10(2))
		I32 k = kappa - (exponent * 78913 >> 18);

		// Step 2: compute zi
		// beta = e + floor(k * log_2(10))
		I32 beta = exponent + (k * 217706 >> 16);
		// phiTildeK = ceil(10^k * 2^(-e * k))
		// = ceil((5^k)(2^k)(2^-k)(2^e))
		// = ceil((5^k)(2^e))
		U64 pow5Hi = POWER_OF_5_TABLE[(POWER_OF_5_TABLE_OFFSET + k) * 2 + 0];
		U64 pow5Lo = POWER_OF_5_TABLE[(POWER_OF_5_TABLE_OFFSET + k) * 2 + 1];
		U64 multiplicand = (2ull * significand + 1ull) << beta;
		U64 mulHi;
		_mulx_u64(multiplicand, pow5Lo, &mulHi);
		U64 zi;
		U64 mulLo = _mulx_u64(multiplicand, pow5Hi, &zi);
		_addcarryx_u64(_addcarryx_u64(0, mulHi, mulLo, &mulHi), 0, zi, &zi);
		B32 zIsInteger = mulHi == 0;

		// Step 3: compute s, r by dividing zi by 10^(kappa + 1)
		U64 s;
		_mulx_u64(zi, 2361183241434822607ull, &s);
		s >>= 71 - 64;
		U64 r = zi - tenToTheKappaPlusOnePower * s;

		// Step 4: compute deltai
		U64 deltai = pow5Hi >> (64 - beta - 1);

		// Step 5: check if r > deltai holds
		if (r > deltai) {
			// intersection is empty
		} else {
			// Step 6: check if r < deltai holds. If true, check if r == zf == 0 and wr not in interval
			if (r < deltai) {
				if (r == 0 && zIsInteger && !rightBoundInInterval) {
					// intersection is empty
				} else {
					// Our element is s * 10^(-k + kappa + 1)
					base10Digits = s;
					base10Exponent = -k + kappa + 1;
					goto generateDigits;
				}
			} else {
				// Step 7: r == deltai. Compute the parity of xi
				multiplicand = (2ull * significand - 1ull) << beta;
				mulLo = _mulx_u64(multiplicand, pow5Lo, &mulHi);
				U64 midBits = multiplicand * pow5Hi + mulHi;
				// The parity bit is the beta-th bit, counting from the MSB
				B32 xIsOdd = midBits >> 64 - beta & 1;
				// It's an integer if the 64 lower bits starting after the parity bit are 0
				B32 xIsInteger = (midBits & (1 << 64 - beta) - 1) == 0 && mulLo >> 64 - beta == 0;
				if (xIsOdd) {
					// Odd number, zf < deltaf, so s * 10^(-k + kappa + 1) is the unique element in I intersect (TheIntegers * 10^(-k0 + 1))
					base10Digits = s;
					base10Exponent = -k + kappa + 1;
					goto generateDigits;
				} else {
					if (!leftBoundInInterval) {
						// intersection is empty
					} else if (xIsInteger) {
						// s * 10^(-k + kappa + 1) is the unique element in I intersect (TheIntegers * 10^(-k0 + 1))
						base10Digits = s;
						base10Exponent = -k + kappa + 1;
						goto generateDigits;
					} else {
						// intersection is empty
					}
				}
			}
		}

		// Part 2

		// Step 1: compute D = rTilde + 10^k / 2 - deltai / 2
		U64 sTilde = r != 0 ? s : s - 1;
		U64 rTilde = r != 0 ? r : tenToTheKappaPlusOnePower;
		U64 capitalD = rTilde + tenToTheKappaPower / 2 - deltai / 2;
		// Step 2: compute t, p by dividing D by 10^k. We only need to know if D is divisible by 10^k (t * 10^k == D)
		U64 t = capitalD / tenToTheKappaPower;
		if (capitalD != tenToTheKappaPower * t) {
			// Step 3: answer is (10 * sTilde + t) * 10 ^ (-k + kappa)
			base10Digits = 10 * sTilde + t;
			base10Exponent = -k + kappa;
		} else {
			// Step 4: compare parity of yi with that of D - 10^k / 2
			multiplicand = (2ull * significand) << beta;
			mulLo = _mulx_u64(multiplicand, pow5Lo, &mulHi);
			U64 midBits = multiplicand * pow5Hi + mulHi;
			// The parity bit is the beta-th bit, counting from the MSB
			B32 yIsOdd = midBits >> 64 - beta & 1;
			B32 yIsInteger = (midBits & (1 << 64 - beta) - 1) == 0 && mulLo >> 64 - beta == 0;
			if (yIsOdd != (capitalD - tenToTheKappaPower / 2 & 1)) {
				// Parity is different, answer is (10 * sTilde + t - 1) * 10 ^ (-k + kappa)
				base10Digits = 10 * sTilde + t - 1;
				base10Exponent = -k + kappa;
			} else if (yIsInteger) {
				// Step 5: we have a tie, break according to the rule. Answer is (10 * sTilde + t - 1) * 10 ^ (-k + kappa)
				base10Digits = 10 * sTilde + t - 1;
				base10Exponent = -k + kappa;
			} else {
				// Step 6: answer is (10 * sTilde + t) * 10 ^ (-k + kappa)
				base10Digits = 10 * sTilde + t;
				base10Exponent = -k + kappa;
			}
		}
	} else {
		// Part 3
		// Rare asymmetric interval case

		// Step 1: compute k0 and beta
		// While the old range was just the size of the exponent, the new one is
		// = 2^(e - 1) + 2^(e - 2)
		// = (1/2) * 2^e + (1/4) * 2^e
		// = (3/4) * 2^e
		// We want the base 10 logarithm of this to compute k0
		// = log_10((3/4) * 2^e)
		// = e * log_10(2) + log_10(3/4)
		// = e * 0.30102999566 - 0.1249387366
		// k0 = -floor(e * log_10(2) + log_10(3/4))
		I32 k0 = -(exponent * 1262611 - 524031 >> 22);
		// beta = e + floor(k0 * log_2(10))
		I32 beta = exponent + (k0 * 217706 >> 16);
		// Step 2: compute xi and zi
		const I32 p = 52;
		U64 pow5Hi = POWER_OF_5_TABLE[(POWER_OF_5_TABLE_OFFSET + k0) * 2 + 0];
		U64 pow5Lo = POWER_OF_5_TABLE[(POWER_OF_5_TABLE_OFFSET + k0) * 2 + 1];
		U64 xi = (pow5Hi - (pow5Hi >> p + 2)) >> (64 - p - beta - 1);
		U64 zi = (pow5Hi + (pow5Hi >> p + 1)) >> (64 - p - beta - 1);
		// Step 3: compute xiTilde and ziTilde 
		// I think this computation is wrong
		U64 isInteger = exponent >= 0 && exponent <= 3;
		U64 xiTilde = isInteger ? xi : xi + 1;
		U64 ziTilde = zi;

		// Step 4: compute floor(ziTilde / 10) and check if xiTilde <= floor(ziTilde / 10) * 10
		U64 ziTildeOver10 = ziTilde / 10;
		if (xiTilde <= ziTildeOver10 * 10) {
			// Unique element, ziTildeOver10 * 10 ^ (-k0 + 1) is our answer
			base10Digits = ziTildeOver10;
			base10Exponent = -k0 + 1;
		} else {
			// Step 5: compute yru
			U64 yru = (pow5Hi >> 62 - p - beta) + 1 >> 1;
			// Step 6: detect tie, then choose between yru and yrd = yru - 1
			// Tie condition check is
			// -p - 2 - floor((p + 4) * log_5(2) - log_5(3)) <= e <= -p - 2 - floor((p + 2) * log_5(2))
			// Both of these bounds work out to be -78 for F64
			if (exponent == -78) {
				if (yru & 1) {
					// Answer is (yru - 1) * 10 ^ (-k0)
					base10Digits = yru - 1;
					base10Exponent = -k0;
				} else {
					// Answer is yru * 10 ^ (-k0)
					base10Digits = yru;
					base10Exponent = -k0;
				}
			} else {
				if (yru >= xiTilde) {
					// Step 7: yru >= xiTilde holds
					// Answer is yru * 10 ^ (-k0)
					base10Digits = yru;
					base10Exponent = -k0;
				} else {
					// Step 8:
					// Answer is (yru + 1) * 10 ^ (-k0)
					base10Digits = yru + 1;
					base10Exponent = -k0;
				}
			}
		}
	}

generateDigits:;
	char digits[96];
	// Leave some padding so exponents can have more trailing zeros
	memset(digits, '0', 96);
	char* digitPtr = &digits[64];
	I32 digitCount = 0;
	I32 trailingZeroCount = 0;
	while (base10Digits) {
		char digit = base10Digits % 10 + '0';
		if (trailingZeroCount == digitCount && digit == '0') {
			trailingZeroCount++;
		}
		*--digitPtr = digit;
		digitCount++;
		base10Digits /= 10;
	}
	I32 decimalPoint = digitCount + base10Exponent;
	B32 useScientificNotation = abs(digitCount + base10Exponent) > 12;
	char* resultPtr = digits;
	if (useScientificNotation) {
		*resultPtr++ = digitPtr[0];
		*resultPtr++ = '.';
		if (digitCount - trailingZeroCount == 1) {
			*resultPtr++ = '0';
		} else {
			for (I32 i = 1; i < digitCount - trailingZeroCount; i++) {
				*resultPtr++ = digitPtr[i];
			}
		}
		*resultPtr++ = 'e';
		I32 finalExponent = base10Exponent + digitCount - 1;
		if (finalExponent < 0) {
			*resultPtr++ = '-';
			finalExponent = -finalExponent;
		}
		digitPtr = &digits[63];
		while (finalExponent) {
			*--digitPtr = finalExponent % 10 + '0';
			finalExponent /= 10;
		}
		memcpy(resultPtr, digitPtr, &digits[63] - digitPtr);
		resultPtr += &digits[63] - digitPtr;
	} else {
		if (decimalPoint <= 0) {
			*resultPtr++ = '0';
		} else {
			for (I32 i = 0; i < decimalPoint; i++) {
				*resultPtr++ = digitPtr[i];
			}
		}
		*resultPtr++ = '.';
		if (decimalPoint >= digitCount - trailingZeroCount) {
			*resultPtr++ = '0';
		} else {
			for (I32 i = decimalPoint; i < digitCount - trailingZeroCount; i++) {
				*resultPtr++ = digitPtr[i];
			}
		}
	}
	U32 finalNumberCharCount = resultPtr - &digits[0];
	memcpy(dstBuffer, &digits[0], min(finalNumberCharCount, dstBufferCapacity));
	*dstBufferSize += min(finalNumberCharCount, dstBufferCapacity);
}
void serialize_f32(char* dstBuffer, U32* dstBufferSize, F32 startValue) {
	serialize_f64(dstBuffer, dstBufferSize, F64(startValue));
}

}