/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace foss::core
{

// A minimal, dependency-free JSON value used to load gameplay content
// (ADR-0003). It supports the subset our content files need: objects, arrays,
// strings, numbers, booleans, and null. It is deliberately NOT a general JSON
// implementation — malformed or unsupported input is reported as an error.
//
// The parser lives in core so it is unit-tested headlessly and stays SDL-free
// (ADR-0002). It is small and allocation-light to respect the low-spec target
// (AGENTS.md 1b).
class Json
{
public:
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    Json() = default;

    // --- parsing ----------------------------------------------------------

    // Parses `text`. On success `error` is left empty and the returned value is
    // usable. On failure the returned value is Null and `error` describes the
    // problem, including the line number.
    static Json parse(const std::string& text, std::string& error);

    // Reads and parses a file. Returns false and sets `error` on failure.
    static bool parseFile(const std::string& path, Json& out, std::string& error);

    // --- inspection -------------------------------------------------------

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool(bool fallback = false) const { return type_ == Type::Bool ? bool_ : fallback; }
    double asNumber(double fallback = 0.0) const { return type_ == Type::Number ? number_ : fallback; }
    float asFloat(float fallback = 0.0f) const
    {
        return type_ == Type::Number ? static_cast<float>(number_) : fallback;
    }
    int asInt(int fallback = 0) const
    {
        return type_ == Type::Number ? static_cast<int>(number_) : fallback;
    }
    const std::string& asString() const { return string_; }

    // Array access. `index` out of range yields a shared Null value. The `int`
    // overload exists so the natural `node[0]` is not ambiguous with the string
    // overload (0 is also a null pointer constant).
    std::size_t size() const { return array_.size(); }
    const Json& operator[](std::size_t index) const;
    const Json& operator[](int index) const { return (*this)[static_cast<std::size_t>(index)]; }

    // Object access. A missing key yields a shared Null value, so a chain like
    // `node["weapon"]["damage"].asFloat()` is safe.
    const Json& operator[](const char* key) const;
    bool has(const char* key) const;

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Json> array_;
    std::vector<std::pair<std::string, Json>> object_;

    friend class JsonParser;
};

} // namespace foss::core
