// Minimal streaming JSON writer.
//
// The project only needs to *emit* JSON, so a ~200 line writer is preferred
// over an external dependency. The writer tracks nesting state and throws
// std::logic_error on misuse (e.g. a value without a key inside an object),
// which turns formatter bugs into test failures instead of invalid output.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sysdiag {

class JsonWriter {
  public:
    explicit JsonWriter(bool pretty = true) : pretty_(pretty) {}

    void begin_object();
    void end_object();
    void begin_array();
    void end_array();

    void key(std::string_view name);

    void string(std::string_view value);
    void number(std::uint64_t value);
    void number(std::int64_t value);
    // Fixed precision; NaN/Inf are written as null (JSON has no representation).
    void number(double value, int precision);
    void boolean(bool value);
    void null();

    // Convenience: write `"name": value`, or `"name": null` when empty.
    void field(std::string_view name, std::string_view value);
    void field(std::string_view name, const char* value) { field(name, std::string_view(value)); }
    void field(std::string_view name, const std::string& value) {
        field(name, std::string_view(value));
    }
    void field(std::string_view name, const std::optional<std::string>& value);
    void field(std::string_view name, std::uint64_t value);
    void field(std::string_view name, const std::optional<std::uint64_t>& value);
    void field(std::string_view name, const std::optional<std::uint32_t>& value);
    void field(std::string_view name, const std::optional<double>& value, int precision);
    // Named differently so pointers/integers can never silently convert to bool.
    void bool_field(std::string_view name, bool value);
    void string_array(std::string_view name, const std::vector<std::string>& values);

    // Returns the document; throws if containers are still open.
    [[nodiscard]] std::string finish();

    [[nodiscard]] static std::string escape(std::string_view value);

  private:
    struct Frame {
        bool is_object = false;
        bool has_items = false;
        bool awaiting_value = false;  // a key has been written
    };

    void before_value();
    void close(bool is_object);
    void newline_indent();

    std::string out_;
    std::vector<Frame> stack_;
    bool pretty_;
    bool root_written_ = false;
};

}  // namespace sysdiag
