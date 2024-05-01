#include "pch.h"

namespace DAWdle {
//these are all named after the class StrA, followed by the method name.
	TEST(TestCaseName, TestName) {
		EXPECT_EQ(1, 1);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAequals) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST == "urmomDotcom"sa), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAequals2) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST == "fish"sa), false);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrA[]) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST[3] == "m"sa), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrA[]2) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST == "D"sa), false);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAempty) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ(TEST.is_empty(), false);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAempty2) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ(""sa.is_empty(), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAfind) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ(TEST.find('m'), 2);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAfind2) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ(TEST.find('z'), -1);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrArfind) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ(TEST.rfind("momDot"sa), 8);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrArfind2) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ(TEST.rfind("father"sa), -1);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAstarts_with) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ(TEST.starts_with("urmom"sa), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAstarts_with2) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ(TEST.starts_with("oops"sa), false);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAends_with) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ(TEST.ends_with("Dotcom"sa), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAends_with2) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ(TEST.ends_with("oops"sa), false);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAslice) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST.slice(5, 8) == "Dot"sa), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAslice2) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST.slice(8, 5) == ""sa), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAslice3) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST.slice(0, 20) == "error"sa), false);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAprefix) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST.prefix(5) == "urmom"sa), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAprefix2) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST.prefix(30) == "urmomDotcom"sa), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAsuffix) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST.suffix(6) == "Dotcom"sa), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAsuffix2) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST.slice(20) == "urmomDotcom"sa), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAskip) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST.skip(5) == "Dotcom"sa), true);
		EXPECT_TRUE(true);
	}
	TEST(StrAClassMethods, StrAskip) {
		StrA TEST = "urmomDotcom"sa;
		EXPECT_EQ((TEST.skip(12) == ""sa), true);
		EXPECT_TRUE(true);
	}

}
