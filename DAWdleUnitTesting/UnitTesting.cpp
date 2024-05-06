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

	TEST(Slice, PartiallyOutOfBounds) {
		EXPECT_EQ(str.slice(6, 20), "Dotcom"sa);
	}

	TEST(Slice, FullyOutOfBounds) {
		EXPECT_EQ(str.slice(20, 99), ""sa);
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

	TEST(StrALen, OneEmptyString) {
		EXPECT_EQ(total_stralen(""sa), 0);
	}

	TEST(StrALen, OneNonemptyString) {
		EXPECT_EQ(total_stralen(str), 12);
	}

	TEST(StrALen, MultipleEmptyStrings) {
		EXPECT_EQ(total_stralen(""sa, ""sa, ""sa), 0);
	}

	TEST(StrALen, MultipleNonemptyStrings) {
		EXPECT_EQ(total_stralen(str, "dawdle"sa, "Dot"sa, "com"sa), 24);
	}

	TEST(StrALen, MultipleNonemptyStringsOneEmpty) {
		EXPECT_EQ(total_stralen("dawdle"sa, ""sa, "com"sa), 9);
	}

	TEST(CopyToBuffer, OneNonemptyString) {
		char buf[1024];
		copy_strings_to_buffer(buf, str);
		EXPECT_EQ(memcmp(buf, str.str, str.length), 0);
	}

	TEST(CopyToBuffer, MultipleNonemptyStrings) {
		char buf[1024];
		StrA expected = "dawdleDotcomdawdleDotcom"sa;
		copy_strings_to_buffer(buf, str, "dawdle"sa, "Dot"sa, "com"sa);
		EXPECT_EQ(memcmp(buf, expected.str, expected.length), 0);
	}

	TEST(CopyToBuffer, MultipleNonemptyStringsOneEmpty) {
		char buf[1024];
		StrA expected = "dawdlecom"sa;
		copy_strings_to_buffer(buf, "dawdle"sa, ""sa, "com"sa);
		EXPECT_EQ(memcmp(buf, expected.str, expected.length), 0);
	}

	TEST(StrACat, EmptyStrings) {
		drill_lib_init();
		EXPECT_EQ(stracat(globalArena, ""sa, ""sa), ""sa);
	}

	TEST(StrACat, NonemptyStrings) {
		drill_lib_init();
		EXPECT_EQ(stracat(globalArena, "dawdle"sa, "Dot"sa, "com"sa), str);
	}

	TEST(StrACat, NonemptyStringsWithOneEmpty) {
		drill_lib_init();
		EXPECT_EQ(stracat(globalArena, "dawdle"sa, ""sa, "com"sa), "dawdlecom"sa);
	}
}

namespace ArenaArray {
	TEST(Pushback_Contains, Single) {
		ArenaArrayList<U32> test;
		test.push_back(42);
		EXPECT_TRUE(test.contains(42));
	}

	TEST(Popback, Single) {
		ArenaArrayList<U32> test;
		test.push_back(42);
		test.pop_back();
		EXPECT_FALSE(test.contains(42));
	}

	TEST(Empty, IsEmpty) {
		ArenaArrayList<U32> test;
		EXPECT_TRUE(test.empty());
	}

	TEST(Empty, IsNotEmpty) {
		ArenaArrayList<U32> test;
		test.push_back(42);
		EXPECT_FALSE(test.empty());
	}

	TEST(Last, CheckLast) {
		ArenaArrayList<U32> test;
		test.push_back(42);
		EXPECT_EQ(test.last(), 42);
	}
}

namespace MemoryTests {
	TEST(Memcpy, BasicCopy) {
		char src[] = "hello";
		char dest[6];
		memcpy(dest, src, sizeof(src));
		EXPECT_STREQ(src, dest);
	}

	TEST(Memcpy, OverlappingCopy) {
		char str[] = "overlapping";
		char dest[12];
		memcpy(dest, str + 4, 9);
		EXPECT_STREQ("lapping", dest);
	}

	TEST(Memcpy, ZeroSizeCopy) {
		char src[] = "hello";
		char dest[6] = "world";
		memcpy(dest, src, 0);
		EXPECT_STREQ("world", dest);
	}

	TEST(Memcpy, LargeCopy) {
		char src[1000];
		char dest[1000];
		memset(src, 'X', sizeof(src));
		memset(dest, 'A', sizeof(dest));
		memcpy(dest, src, sizeof(src));
		EXPECT_TRUE(memcmp(src, dest, sizeof(src)) == 0);
	}

	TEST(Memcpy, ReverseCopy) {
		char src[] = "hello";
		char dest[6];
		memcpy(dest, src + 4, 2);
		EXPECT_STREQ("o", dest);
	}

	TEST(Memcpy, NonCharTypeCopy) {
		int src[] = { 1, 2, 3, 4, 5 };
		int dest[5] = { 0 };
		memcpy(dest, src, sizeof(src));
		EXPECT_TRUE(memcmp(src, dest, sizeof(src)) == 0);
	}

	TEST(Memset, BasicSet) {
		char str[] = "hello";
		memset(str, 'X', 5);
		EXPECT_STREQ("XXXXX", str);
	}

	TEST(Memset, MemsetReturn) {
		char str[5];
		EXPECT_EQ(memset(str, 'X', 5), str);
	}

	TEST(Memset, LargeSet) {
		char str[1000];
		memset(str, 'X', sizeof(str));
		for (int i = 0; i < sizeof(str); ++i) {
			EXPECT_EQ(str[i], 'X');
		}
	}

	TEST(Memset, NonCharTypeSet) {
		int arr[10];
		memset(arr, 0, sizeof(arr));
		for (int i = 0; i < 10; ++i) {
			EXPECT_EQ(arr[i], 0);
		}
	}

	TEST(Strcmp, EqualStrings) {
		EXPECT_EQ(strcmp("hello", "hello"), 0);
	}

	TEST(Strcmp, DifferentStrings) {
		EXPECT_EQ(strcmp("hello", "hell"), 1);
		EXPECT_EQ(strcmp("hello", "helloo"), -1);
	}

	TEST(Strcmp, EmptyStrings) {
		EXPECT_EQ(strcmp("", ""), 0);
	}

	TEST(Strcmp, OneEmptyString) {
		EXPECT_EQ(strcmp("", "dotcom"), -1);
	}

	TEST(Strcmp, CaseSensitiveStrings) {
		EXPECT_EQ(strcmp("Hello", "hello"), -1);
		EXPECT_EQ(strcmp("hello", "Hello"), 1);
	}

	TEST(Strcmp, MultiByteCharacters) {
		EXPECT_EQ(strcmp("éstevan", "estevan"), 1);
	}

	TEST(Strcmp, NullTerminatedCheck) {
		char str[] = "hello";
		EXPECT_EQ(strcmp(str, "hello\0extra"), 0);
	}

	TEST(Strlen, BasicLength) {
		EXPECT_EQ(strlen("hello"), 5);
	}

	TEST(Strlen, EmptyString) {
		EXPECT_EQ(strlen(""), 0);
	}

	TEST(Strlen, LongString) {
		EXPECT_EQ(strlen("this is a long string with many characters"), 42);
	}

	TEST(Strlen, ExtremelyLongString) {
		char str[1000];
		for (int i = 0; i < sizeof(str); ++i) {
			str[i] = 'c';
		}
		str[sizeof(str) - 1] = '\0';
		EXPECT_EQ(strlen(str), 999);
	}

	TEST(Memcmp, EqualMemory) {
		int arr1[] = { 1, 2, 3, 4, 5 };
		int arr2[] = { 1, 2, 3, 4, 5 };
		EXPECT_EQ(memcmp(arr1, arr2, sizeof(arr1)), 0);
	}

	TEST(Memcmp, DifferentMemory) {
		int arr1[] = { 1, 2, 3, 4, 5 };
		int arr2[] = { 1, 2, 3, 4, 6 };
		EXPECT_LT(memcmp(arr1, arr2, sizeof(arr1)), 0);
	}

	TEST(Memcmp, EmptyMemory) {
		int arr1[10];
		int arr2[10];
		EXPECT_EQ(memcmp(arr1, arr2, sizeof(arr1)), 0);
	}

	TEST(Memcmp, LargeMemoryComparison) {
		char buffer1[1000];
		char buffer2[1000];
		memset(buffer1, 'A', sizeof(buffer1));
		memset(buffer2, 'A', sizeof(buffer2));
		buffer2[500] = 'B';
		EXPECT_LT(memcmp(buffer1, buffer2, sizeof(buffer1)), 0);
	}
	TEST(Memcmp, ComparePartialMemory) {
		char buffer1[10] = "hello";
		char buffer2[10] = "hello";
		EXPECT_EQ(memcmp(buffer1, buffer2, 3), 0);
	}
}