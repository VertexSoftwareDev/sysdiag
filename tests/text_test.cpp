#include <gtest/gtest.h>

#include "core/text.hpp"

namespace text = sysdiag::text;

TEST(Text, Trim) {
    EXPECT_EQ(text::trim("  Intel(R) Core(TM)  "), "Intel(R) Core(TM)");
    EXPECT_EQ(text::trim("\t\r\n"), "");
    EXPECT_EQ(text::trim(""), "");
    EXPECT_EQ(text::trim("x"), "x");
}

TEST(Text, IEquals) {
    EXPECT_TRUE(text::iequals("CPU", "cpu"));
    EXPECT_TRUE(text::iequals("", ""));
    EXPECT_FALSE(text::iequals("cpu", "cpus"));
    EXPECT_FALSE(text::iequals("cpu", "gpu"));
}

TEST(Text, SanitizeUtf8KeepsValidText) {
    const std::string turkish = "\xC4\x9E\xC3\xBC\xC5\x9F\xC4\xB1\xC3\xB6\xC3\xA7";  // Ğüşıöç
    EXPECT_EQ(text::sanitize_utf8(turkish), turkish);
    const std::string emoji = "\xF0\x9F\x98\x80";  // U+1F600
    EXPECT_EQ(text::sanitize_utf8(emoji), emoji);
    EXPECT_EQ(text::sanitize_utf8("plain"), "plain");
}

TEST(Text, SanitizeUtf8ReplacesInvalidSequences) {
    const std::string fffd = "\xEF\xBF\xBD";
    EXPECT_EQ(text::sanitize_utf8("a\xFF"
                                  "b"),
              "a" + fffd + "b");
    EXPECT_EQ(text::sanitize_utf8("\xC3"), fffd);                                   // truncated
    EXPECT_EQ(text::sanitize_utf8("\xC0\xAF"), fffd + fffd);                        // overlong '/'
    EXPECT_EQ(text::sanitize_utf8("\xE0\x80\xAF"), fffd + fffd + fffd);             // overlong
    EXPECT_EQ(text::sanitize_utf8("\xED\xA0\x80"), fffd + fffd + fffd);             // surrogate
    EXPECT_EQ(text::sanitize_utf8("\xF4\x90\x80\x80"), fffd + fffd + fffd + fffd);  // > U+10FFFF
}

TEST(Text, SanitizeForDisplayRemovesControls) {
    EXPECT_EQ(text::sanitize_for_display("a\x1b[2Jb"), "a?[2Jb");
    EXPECT_EQ(text::sanitize_for_display("tab\there"), "tab?here");
    EXPECT_EQ(text::sanitize_for_display("del\x7F"), "del?");
    EXPECT_EQ(text::sanitize_for_display("c1\xC2\x9B"), "c1?");     // U+009B CSI
    EXPECT_EQ(text::sanitize_for_display("\xC2\xA0"), "\xC2\xA0");  // NBSP is fine
}

TEST(Text, SanitizeForDisplayTruncatesOnCodePointBoundary) {
    EXPECT_EQ(text::sanitize_for_display("abcdef", 3), "abc...");
    // "ğ" is two bytes; with a 3-byte budget "aǧ" fits but "aǧǧ" does not.
    EXPECT_EQ(text::sanitize_for_display("a\xC4\x9F\xC4\x9F", 3), "a\xC4\x9F...");
    EXPECT_EQ(text::sanitize_for_display("abc", 3), "abc");
}
