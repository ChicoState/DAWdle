#pragma once

#include "DrillLib.h"

namespace ParseTools {

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

FINLINE void skip_whitespace(StrA* str) {
	while (str->length && is_whitespace(str->str[0])) {
		str->str++, str->length--;
	}
}

const I32 POWER_OF_5_TABLE_OFFSET = 342;
const U64 POWER_OF_5_TABLE[]{
#include "ParseToolsPowerOf5Table.txt"
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
		*f64Out = 0.0;
		return true;
	}
	if (exponent > 308) {
		*f64Out = F64_INF;
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
		*f64Out = 0.0;
		return true;
	}
	if (expectedBinaryExponent <= -1022) {
		// Subnormals
		mostSignificantBits = ((mostSignificantBits >> U64(-1022 - expectedBinaryExponent + 1)) + 1ull) >> 1ull;
		// Mask off bit 52, it's implicitly 1
		*f64Out = bitcast<F64>((mostSignificantBits & ~(1ull << 52ull)));
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
		*f64Out = F64_INF;
		return true;
	}

	// Mask off bit 52, it's implicitly 1
	*f64Out = bitcast<F64>(U64(expectedBinaryExponent + 1023) << 52ull | (mostSignificantBits & ~(1ull << 52ull)));
	return true;
}

B32 parse_f32(F32* f32Out, StrA* str) {
	F64 f64;
	B32 result = parse_f64(&f64, str);
	*f32Out = F32(f64);
	return result;

}

}