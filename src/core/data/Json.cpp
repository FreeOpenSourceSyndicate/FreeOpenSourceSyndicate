/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/data/Json.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace foss::core
{
namespace
{

// A shared Null value so out-of-range / missing lookups can return a reference.
const Json& nullValue()
{
    static const Json value;
    return value;
}

} // namespace

const Json& Json::operator[](std::size_t index) const
{
    if (index >= array_.size())
    {
        return nullValue();
    }
    return array_[index];
}

const Json& Json::operator[](const char* key) const
{
    for (const auto& entry : object_)
    {
        if (entry.first == key)
        {
            return entry.second;
        }
    }
    return nullValue();
}

bool Json::has(const char* key) const
{
    for (const auto& entry : object_)
    {
        if (entry.first == key)
        {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Recursive-descent parser. Kept in this translation unit and out of the header.
// ---------------------------------------------------------------------------
class JsonParser
{
public:
    JsonParser(const std::string& text, std::string& error) : text_(text), error_(error) {}

    bool parse(Json& out)
    {
        skipWhitespace();
        if (!parseValue(out))
        {
            return false;
        }
        skipWhitespace();
        if (pos_ != text_.size())
        {
            return fail("unexpected trailing characters");
        }
        return true;
    }

private:
    bool fail(const std::string& message)
    {
        std::ostringstream oss;
        oss << "line " << line_ << ": " << message;
        error_ = oss.str();
        return false;
    }

    char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

    char advance()
    {
        const char c = text_[pos_++];
        if (c == '\n')
        {
            ++line_;
        }
        return c;
    }

    void skipWhitespace()
    {
        while (pos_ < text_.size())
        {
            const char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                advance();
            }
            else
            {
                break;
            }
        }
    }

    bool parseValue(Json& out)
    {
        switch (peek())
        {
        case '{':
            return parseObject(out);
        case '[':
            return parseArray(out);
        case '"':
            out.type_ = Json::Type::String;
            return parseString(out.string_);
        case 't':
        case 'f':
            return parseBool(out);
        case 'n':
            return parseNull(out);
        default:
            if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek())))
            {
                return parseNumber(out);
            }
            return fail("unexpected character while parsing a value");
        }
    }

    bool parseObject(Json& out)
    {
        advance(); // '{'
        out.type_ = Json::Type::Object;
        skipWhitespace();
        if (peek() == '}')
        {
            advance();
            return true;
        }
        while (true)
        {
            skipWhitespace();
            if (peek() != '"')
            {
                return fail("expected a string key in object");
            }
            std::string key;
            if (!parseString(key))
            {
                return false;
            }
            skipWhitespace();
            if (peek() != ':')
            {
                return fail("expected ':' after object key");
            }
            advance();
            skipWhitespace();
            Json value;
            if (!parseValue(value))
            {
                return false;
            }
            out.object_.emplace_back(std::move(key), std::move(value));
            skipWhitespace();
            const char c = peek();
            if (c == ',')
            {
                advance();
                continue;
            }
            if (c == '}')
            {
                advance();
                return true;
            }
            return fail("expected ',' or '}' in object");
        }
    }

    bool parseArray(Json& out)
    {
        advance(); // '['
        out.type_ = Json::Type::Array;
        skipWhitespace();
        if (peek() == ']')
        {
            advance();
            return true;
        }
        while (true)
        {
            skipWhitespace();
            Json value;
            if (!parseValue(value))
            {
                return false;
            }
            out.array_.push_back(std::move(value));
            skipWhitespace();
            const char c = peek();
            if (c == ',')
            {
                advance();
                continue;
            }
            if (c == ']')
            {
                advance();
                return true;
            }
            return fail("expected ',' or ']' in array");
        }
    }

    bool parseString(std::string& out)
    {
        advance(); // opening quote
        out.clear();
        while (true)
        {
            if (pos_ >= text_.size())
            {
                return fail("unterminated string");
            }
            const char c = advance();
            if (c == '"')
            {
                return true;
            }
            if (c == '\\')
            {
                if (pos_ >= text_.size())
                {
                    return fail("unterminated escape sequence");
                }
                const char e = advance();
                switch (e)
                {
                case '"':
                    out.push_back('"');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case '/':
                    out.push_back('/');
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u':
                    // Minimal support: read 4 hex digits and emit the code point
                    // as-is (BMP only). Our content is ASCII, so this is enough.
                    if (!parseUnicodeEscape(out))
                    {
                        return false;
                    }
                    break;
                default:
                    return fail("invalid escape sequence");
                }
            }
            else if (static_cast<unsigned char>(c) < 0x20)
            {
                return fail("control character in string");
            }
            else
            {
                out.push_back(c);
            }
        }
    }

    bool parseUnicodeEscape(std::string& out)
    {
        unsigned int code = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (pos_ >= text_.size())
            {
                return fail("truncated \\u escape");
            }
            const char h = advance();
            code <<= 4;
            if (h >= '0' && h <= '9')
            {
                code |= static_cast<unsigned int>(h - '0');
            }
            else if (h >= 'a' && h <= 'f')
            {
                code |= static_cast<unsigned int>(h - 'a' + 10);
            }
            else if (h >= 'A' && h <= 'F')
            {
                code |= static_cast<unsigned int>(h - 'A' + 10);
            }
            else
            {
                return fail("invalid hex digit in \\u escape");
            }
        }
        // UTF-8 encode the code point (1-3 bytes for the BMP).
        if (code < 0x80)
        {
            out.push_back(static_cast<char>(code));
        }
        else if (code < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        return true;
    }

    bool parseNumber(Json& out)
    {
        const std::size_t start = pos_;
        if (peek() == '-')
        {
            advance();
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])))
        {
            advance();
        }
        if (peek() == '.')
        {
            advance();
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])))
            {
                advance();
            }
        }
        if (peek() == 'e' || peek() == 'E')
        {
            advance();
            if (peek() == '+' || peek() == '-')
            {
                advance();
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])))
            {
                advance();
            }
        }
        const std::string token = text_.substr(start, pos_ - start);
        if (token.empty() || token == "-")
        {
            return fail("invalid number");
        }
        out.type_ = Json::Type::Number;
        out.number_ = std::strtod(token.c_str(), nullptr);
        return true;
    }

    bool parseBool(Json& out)
    {
        if (text_.compare(pos_, 4, "true") == 0)
        {
            pos_ += 4;
            out.type_ = Json::Type::Bool;
            out.bool_ = true;
            return true;
        }
        if (text_.compare(pos_, 5, "false") == 0)
        {
            pos_ += 5;
            out.type_ = Json::Type::Bool;
            out.bool_ = false;
            return true;
        }
        return fail("invalid literal (expected true/false/null)");
    }

    bool parseNull(Json& out)
    {
        if (text_.compare(pos_, 4, "null") == 0)
        {
            pos_ += 4;
            out.type_ = Json::Type::Null;
            return true;
        }
        return fail("invalid literal (expected true/false/null)");
    }

    const std::string& text_;
    std::string& error_;
    std::size_t pos_ = 0;
    int line_ = 1;
};

Json Json::parse(const std::string& text, std::string& error)
{
    error.clear();
    Json result;
    JsonParser parser(text, error);
    if (!parser.parse(result))
    {
        return Json{};
    }
    return result;
}

bool Json::parseFile(const std::string& path, Json& out, std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        error = "cannot open file: " + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = parse(buffer.str(), error);
    return error.empty();
}

} // namespace foss::core
