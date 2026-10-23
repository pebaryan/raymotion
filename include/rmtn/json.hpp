// rmtn/json.hpp -- a small JSON value for composition props (Remotion's inputProps).
//
//     Json p = Json::parse(R"({"title": "Hi", "accent": "#ff3366", "items": [1, 2]})");
//     std::string t = p.str("title", "untitled");
//     Color accent = p.color("accent", WHITE);
//     for (const Json& it : p["items"].asArray()) ...
#pragma once
#include <raylib.h>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace rmtn {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;   // keeps key order

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool b) : v_(b) {}
    Json(double d) : v_(d) {}
    Json(int i) : v_((double)i) {}
    Json(const char* s) : v_(std::string(s)) {}
    Json(std::string s) : v_(std::move(s)) {}
    Json(Array a) : v_(std::move(a)) {}
    Json(Object o) : v_(std::move(o)) {}

    // Strict JSON (RFC 8259). Throws std::runtime_error with the line and column.
    static Json parse(std::string_view text);

    Type type() const { return (Type)v_.index(); }
    bool isNull() const { return type() == Type::Null; }
    bool isBool() const { return type() == Type::Bool; }
    bool isNumber() const { return type() == Type::Number; }
    bool isString() const { return type() == Type::String; }
    bool isArray() const { return type() == Type::Array; }
    bool isObject() const { return type() == Type::Object; }

    // Typed access; throws std::runtime_error on a type mismatch.
    bool asBool() const;
    double asNumber() const;
    const std::string& asString() const;
    const Array& asArray() const;
    const Object& asObject() const;

    // Lookups return a null value when the key/index is absent or this is the wrong type.
    const Json& operator[](std::string_view key) const;
    const Json& operator[](size_t index) const;
    bool has(std::string_view key) const;
    size_t size() const;                     // array/object entries, else 0
    Json& set(std::string_view key, Json value);   // makes this an object if it is not

    // Field with a fallback when absent; throws when present with the wrong type.
    double num(std::string_view key, double fallback) const;
    int integer(std::string_view key, int fallback) const;
    bool flag(std::string_view key, bool fallback) const;
    std::string str(std::string_view key, std::string_view fallback) const;
    // "#rgb", "#rrggbb", "#rrggbbaa" or [r, g, b(, a)] with 0..255 channels.
    Color color(std::string_view key, Color fallback) const;

    // Objects merge key by key (recursively); anything else in `over` replaces this.
    Json merged(const Json& over) const;
    std::string dump() const;               // compact

    bool operator==(const Json& o) const { return v_ == o.v_; }
    bool operator!=(const Json& o) const { return !(*this == o); }

private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> v_;
};

// Parses a colour string (see Json::color); throws std::runtime_error when malformed.
Color parseColor(std::string_view s);

} // namespace rmtn
