#include "pch.h"
#include "../src/SerializeTools.h"
#include "../src/DrillLib.h"

namespace String {
	const StrA str = "dawdleDotcom"sa;
	const StrA str2 = "dawdledawdle"sa;

	TEST(Comparison, Equals) {
		EXPECT_TRUE(str == "dawdleDotcom"sa);
	}

	TEST(Comparison, UnequalLength) {
		EXPECT_FALSE(str == "dawdleDotco"sa);
	}

	TEST(Comparison, UnequalData) {
		EXPECT_FALSE(str == "dawdleDotnet"sa);
	}

	TEST(Indexing, ZeroIndex) {
		EXPECT_EQ(str[0], 'd');
	}

	TEST(Indexing, PositiveIndex) {
		EXPECT_EQ(str[5], 'e');
	}

	TEST(Indexing, NegativeIndex) {
		EXPECT_EQ(str[-1], 'm');
	}

	TEST(IsEmpty, Empty) {
		EXPECT_TRUE(""sa.is_empty());
	}

	TEST(IsEmpty, NotEmpty) {
		EXPECT_FALSE(str.is_empty());
	}

	TEST(FindFirstString, EmptyString) {
		EXPECT_EQ(str.find(""sa), 0);
	}

	TEST(FindFirstString, BeginningOfString) {
		EXPECT_EQ(str.find("dawdle"sa), 0);
	}

	TEST(FindFirstString, MiddleOfString) {
		EXPECT_EQ(str.find("Dot"sa), 6);
	}

	TEST(FindFirstString, EndOfString) {
		EXPECT_EQ(str.find("com"sa), 9);
	}

	TEST(FindFirstString, EntireString) {
		EXPECT_EQ(str.find("dawdleDotcom"sa), 0);
	}

	TEST(FindFirstString, FoundFirstWhenRepeating) {
		EXPECT_EQ(str2.find("dawdle"sa), 0);
	}

	TEST(FindFirstString, NotInString) {
		EXPECT_EQ(str.find("net"sa), -1);
	}

	TEST(FindFirstString, TooLong) {
		EXPECT_EQ(str.find("dawdleDotcomm"sa), -1);
	}

	TEST(FindFirstChar, FoundUnique) {
		EXPECT_EQ(str.find('e'), 5);
	}

	TEST(FindFirstChar, FoundFirstWhenRepeating) {
		EXPECT_EQ(str.find('d'), 0);
	}

	TEST(FindFirstChar, NotFound) {
		EXPECT_EQ(str.find('z'), -1);
	}

	TEST(FindLastString, BeginningOfString) {
		EXPECT_EQ(str.rfind("dawdle"sa), 0);
	}

	TEST(FindLastString, MiddleOfString) {
		EXPECT_EQ(str.rfind("Dot"sa), 6);
	}

	TEST(FindLastString, EndOfString) {
		EXPECT_EQ(str.rfind("com"sa), 9);
	}

	TEST(FindLastString, EntireString) {
		EXPECT_EQ(str.rfind("dawdleDotcom"sa), 0);
	}

	TEST(FindLastString, FoundLastWhenRepeating) {
		EXPECT_EQ(str2.rfind("dawdle"sa), 6);
	}

	TEST(FindLastString, NotInString) {
		EXPECT_EQ(str.rfind("net"sa), -1);
	}

	TEST(FindLastString, TooLong) {
		EXPECT_EQ(str.rfind("dawdleDotcomm"sa), -1);
	}

	TEST(FindLastChar, FoundUnique) {
		EXPECT_EQ(str.rfind('e'), 5);
	}

	TEST(FindLastChar, FoundLastWhenRepeating) {
		EXPECT_EQ(str.rfind('d'), 3);
	}

	TEST(FindLastChar, NotFound) {
		EXPECT_EQ(str.rfind('z'), -1);
	}

	TEST(StartsWith, DoesStartWith) {
		EXPECT_TRUE(str.starts_with("dawdle"sa));
	}

	TEST(StartsWith, DoesNotStartWith) {
		EXPECT_FALSE(str.starts_with("notdawdle"sa));
	}

	TEST(StartsWith, AlmostStartsWith) {
		EXPECT_FALSE(str.starts_with("dawdlent"sa));
	}

	TEST(StartsWith, TooLong) {
		EXPECT_FALSE(str.starts_with("dawdleDotcomm"sa));
	}

	TEST(EndsWith, DoesEndWith) {
		EXPECT_TRUE(str.ends_with("Dotcom"sa));
	}

	TEST(EndsWith, DoesNotEndWith) {
		EXPECT_FALSE(str.ends_with("Dotnet"sa));
	}

	TEST(EndsWith, AlmostEndsWith) {
		EXPECT_FALSE(str.ends_with("Dotcomm"sa));
	}

	TEST(EndsWith, TooLong) {
		EXPECT_FALSE(str.ends_with("dawdleDotComm"sa));
	}

	TEST(Slice, SliceBeginning) {
		EXPECT_EQ(str.slice(0, 6), "dawdle"sa);
	}

	TEST(Slice, SliceMiddle) {
		EXPECT_EQ(str.slice(6, 9), "Dot"sa);
	}

	TEST(Slice, SliceEnd) {
		EXPECT_EQ(str.slice(9, 12), "com"sa);
	}

	TEST(Slice, ZeroLengthSlice) {
		EXPECT_EQ(str.slice(5, 5), ""sa);
	}

	TEST(Slice, NegativeLengthSlice) {
		EXPECT_EQ(str.slice(5, 4), ""sa);
	}

	TEST(Slice, NegativeSlice) {
		EXPECT_EQ(str.slice(-6, -3), "Dot"sa);
	}

	TEST(Prefix, ZeroLengthPrefix) {
		EXPECT_EQ(str.prefix(0), ""sa);
	}

	TEST(Prefix, PositivePrefix) {
		EXPECT_EQ(str.prefix(6), "dawdle"sa);
	}

	TEST(Prefix, NegativePrefix) {
		EXPECT_EQ(str.prefix(-1), "dawdleDotco"sa);
	}

	TEST(Prefix, PrefixLongerThanString) {
		EXPECT_EQ(str.prefix(99), "dawdleDotcom"sa);
	}

	TEST(Suffix, ZeroLengthSuffix) {
		EXPECT_EQ(str.suffix(0), ""sa);
	}

	TEST(Suffix, PositiveSuffix) {
		EXPECT_EQ(str.suffix(6), "Dotcom"sa);
	}

	TEST(Suffix, NegativeSuffix) {
		EXPECT_EQ(str.suffix(-1), "awdleDotcom"sa);
	}

	TEST(Suffix, SuffixLongerThanString) {
		EXPECT_EQ(str.suffix(99), "dawdleDotcom"sa);
	}

	TEST(Skip, SkipZero) {
		EXPECT_EQ(str.skip(0), "dawdleDotcom"sa);
	}

	TEST(Skip, SkipPositiveAmount) {
		EXPECT_EQ(str.skip(6), "Dotcom"sa);
	}

	TEST(Skip, NegativeSkip) {
		EXPECT_EQ(str.skip(-1), "m"sa);
	}

	TEST(Skip, SkipLongerThanString) {
		EXPECT_EQ(str.skip(99), ""sa);
	}

	TEST(Substr, ZeroLengthSubstr) {
		EXPECT_EQ(str.substr(0, 0), ""sa);
	}

	TEST(Substr, BeginningOfString) {
		EXPECT_EQ(str.substr(0, 6), "dawdle"sa);
	}

	TEST(Substr, MiddleOfString) {
		EXPECT_EQ(str.substr(6, 3), "Dot"sa);
	}

	TEST(Substr, EndOfString) {
		EXPECT_EQ(str.substr(9, 3), "com"sa);
	}

	TEST(Substr, EntireString) {
		EXPECT_EQ(str.substr(0, 12), "dawdleDotcom"sa);
	}

	TEST(Substr, TooLong) {
		EXPECT_EQ(str.substr(9, 12), "com"sa);
	}
	TEST(Begin, Begin) {
		EXPECT_EQ(*str.begin(), 'd');
	}

	TEST(End, End) {
		EXPECT_EQ(*(str.end() - 1), 'm');
	}

	TEST(Front, Front) {
		EXPECT_EQ(str.front(), 'd');
	}

	TEST(Back, Back) {
		EXPECT_EQ(str.back(), 'm');
	}
}