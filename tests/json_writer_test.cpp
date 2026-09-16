#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

#include "core/json_writer.hpp"
#include "support/json_validator.hpp"

using sysdiag::JsonWriter;
using sysdiag::test::JsonValidator;

TEST(JsonValidatorSelfTest, AcceptsAndRejects) {
    EXPECT_EQ(JsonValidator::validate(R"({"a":[1,2.5,-3e2,true,false,null,"x\n"]})"), "");
    EXPECT_NE(JsonValidator::validate(R"({"a":1,})"), "");
    EXPECT_NE(JsonValidator::validate(R"({'a':1})"), "");
    EXPECT_NE(JsonValidator::validate("{\"a\":\"\x01\"}"), "");
    EXPECT_NE(JsonValidator::validate(R"([01])"), "");
    EXPECT_NE(JsonValidator::validate(R"({"a":1} x)"), "");
    EXPECT_NE(JsonValidator::validate(""), "");
    EXPECT_NE(JsonValidator::validate(R"(["\x"])"), "");
}

TEST(JsonWriter, EscapesSpecialCharacters) {
    EXPECT_EQ(JsonWriter::escape("a\"b"), "a\\\"b");
    EXPECT_EQ(JsonWriter::escape("C:\\Windows"), "C:\\\\Windows");
    EXPECT_EQ(JsonWriter::escape("line\nnext\ttab\r"), "line\\nnext\\ttab\\r");
    EXPECT_EQ(JsonWriter::escape(std::string("\x00\x01\x1f", 3)), "\\u0000\\u0001\\u001f");
    EXPECT_EQ(JsonWriter::escape("\b\f"), "\\b\\f");
    EXPECT_EQ(JsonWriter::escape("/"), "/");
}

TEST(JsonWriter, KeepsUtf8AndReplacesInvalidBytes) {
    EXPECT_EQ(JsonWriter::escape("\xC3\xBC"), "\xC3\xBC");
    EXPECT_EQ(JsonWriter::escape("bad\xFF"), "bad\xEF\xBF\xBD");
}

TEST(JsonWriter, CompactDocument) {
    JsonWriter w(false);
    w.begin_object();
    w.field("name", "sysdiag");
    w.field("count", std::uint64_t{3});
    w.field("missing", std::optional<std::string>{});
    w.bool_field("ok", true);
    w.string_array("list", {"a", "b"});
    w.key("empty");
    w.begin_array();
    w.end_array();
    w.key("nested");
    w.begin_object();
    w.end_object();
    w.end_object();
    EXPECT_EQ(
        w.finish(),
        R"({"name":"sysdiag","count":3,"missing":null,"ok":true,"list":["a","b"],"empty":[],"nested":{}})");
}

TEST(JsonWriter, PrettyDocumentIsValid) {
    JsonWriter w(true);
    w.begin_object();
    w.field("text", "quote \" backslash \\ newline \n");
    w.key("numbers");
    w.begin_array();
    w.number(std::uint64_t{0});
    w.number(std::numeric_limits<std::uint64_t>::max());
    w.number(std::numeric_limits<std::int64_t>::min());
    w.number(12.345, 1);
    w.end_array();
    w.end_object();
    const std::string doc = w.finish();
    EXPECT_EQ(JsonValidator::validate(doc), "") << doc;
    EXPECT_NE(doc.find("18446744073709551615"), std::string::npos);
    EXPECT_NE(doc.find("-9223372036854775808"), std::string::npos);
    EXPECT_NE(doc.find("12.3"), std::string::npos);
    EXPECT_EQ(doc.back(), '\n');
}

TEST(JsonWriter, NonFiniteNumbersBecomeNull) {
    JsonWriter w(false);
    w.begin_array();
    w.number(std::numeric_limits<double>::quiet_NaN(), 1);
    w.number(std::numeric_limits<double>::infinity(), 1);
    w.number(-std::numeric_limits<double>::infinity(), 1);
    w.end_array();
    EXPECT_EQ(w.finish(), "[null,null,null]");
}

TEST(JsonWriter, OptionalFields) {
    JsonWriter w(false);
    w.begin_object();
    w.field("a", std::optional<std::uint32_t>{7});
    w.field("b", std::optional<std::uint32_t>{});
    w.field("c", std::optional<double>{50.04}, 1);
    w.field("d", std::optional<double>{}, 1);
    w.end_object();
    EXPECT_EQ(w.finish(), R"({"a":7,"b":null,"c":50.0,"d":null})");
}

TEST(JsonWriter, MisuseThrows) {
    {
        JsonWriter w;
        w.begin_object();
        EXPECT_THROW(w.string("value without key"), std::logic_error);
    }
    {
        JsonWriter w;
        w.begin_array();
        EXPECT_THROW(w.key("key in array"), std::logic_error);
    }
    {
        JsonWriter w;
        w.begin_object();
        EXPECT_THROW(w.end_array(), std::logic_error);
    }
    {
        JsonWriter w;
        w.begin_object();
        w.key("dangling");
        EXPECT_THROW(w.end_object(), std::logic_error);
    }
    {
        JsonWriter w;
        w.begin_object();
        EXPECT_THROW((void)w.finish(), std::logic_error);
    }
    {
        JsonWriter w;
        EXPECT_THROW((void)w.finish(), std::logic_error);
    }
    {
        JsonWriter w;
        w.null();
        EXPECT_THROW(w.null(), std::logic_error);
    }
    {
        JsonWriter w;
        EXPECT_THROW(w.key("no object"), std::logic_error);
    }
}
