#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace cinerig::testing
{

struct GoldenTolerance
{
    double absolute = 1e-4;
    double relative = 0.0;
    double position = 1e-4;
    double rotation = 1e-4;
};

struct GoldenResult
{
    bool ok = false;
    bool updated = false;
    std::string message;
};

inline bool updateGoldenRequested() noexcept
{
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "CINERIG_UPDATE_GOLDEN") != 0 || value == nullptr)
        return false;
    const std::string_view view(value);
    const bool requested = view == "1" || view == "true" || view == "TRUE";
    std::free(value);
    return requested;
#else
    const char* value = std::getenv("CINERIG_UPDATE_GOLDEN");
    return value != nullptr
        && (std::string_view(value) == "1"
            || std::string_view(value) == "true"
            || std::string_view(value) == "TRUE");
#endif
}

inline std::string normalizeGoldenNewlines(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == '\r')
        {
            if (i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
            result.push_back('\n');
        }
        else
        {
            result.push_back(c);
        }
    }
    return result;
}

inline std::string formatGoldenNumber(double value)
{
    if (value == 0.0)
        value = 0.0;
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
}

namespace golden_detail
{

struct JsonValue
{
    enum class Kind
    {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object,
    };

    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;
};

class JsonParser
{
public:
    explicit JsonParser(std::string_view text)
        : text_(text)
    {
    }

    JsonValue parse()
    {
        skipWhitespace();
        JsonValue value = parseValue();
        skipWhitespace();
        if (position_ != text_.size())
            fail("trailing characters");
        return value;
    }

private:
    [[noreturn]] void fail(std::string_view message) const
    {
        throw std::runtime_error(
            "JSON parse error at byte " + std::to_string(position_) + ": " + std::string(message));
    }

    void skipWhitespace()
    {
        while (position_ < text_.size())
        {
            const char c = text_[position_];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
                break;
            ++position_;
        }
    }

    char peek() const
    {
        return position_ < text_.size() ? text_[position_] : '\0';
    }

    bool consume(char expected)
    {
        if (peek() != expected)
            return false;
        ++position_;
        return true;
    }

    void expect(char expected, std::string_view message)
    {
        if (!consume(expected))
            fail(message);
    }

    JsonValue parseValue()
    {
        skipWhitespace();
        if (position_ >= text_.size())
            fail("expected a value");

        switch (text_[position_])
        {
        case '{':
            return parseObject();
        case '[':
            return parseArray();
        case '"':
        {
            JsonValue value;
            value.kind = JsonValue::Kind::String;
            value.string = parseString();
            return value;
        }
        case 't':
            consumeLiteral("true");
            return makeBoolean(true);
        case 'f':
            consumeLiteral("false");
            return makeBoolean(false);
        case 'n':
            consumeLiteral("null");
            return {};
        default:
            if (text_[position_] == '-' || isDigit(text_[position_]))
                return parseNumber();
            fail("unexpected character");
        }
    }

    static JsonValue makeBoolean(bool value)
    {
        JsonValue result;
        result.kind = JsonValue::Kind::Boolean;
        result.boolean = value;
        return result;
    }

    JsonValue parseObject()
    {
        expect('{', "expected '{'");
        JsonValue result;
        result.kind = JsonValue::Kind::Object;
        skipWhitespace();
        if (consume('}'))
            return result;

        while (true)
        {
            skipWhitespace();
            if (peek() != '"')
                fail("expected an object key");
            const std::string key = parseString();
            skipWhitespace();
            expect(':', "expected ':' after object key");
            JsonValue value = parseValue();
            if (!result.object.emplace(key, std::move(value)).second)
                fail("duplicate object key");
            skipWhitespace();
            if (consume('}'))
                return result;
            expect(',', "expected ',' or '}'");
        }
    }

    JsonValue parseArray()
    {
        expect('[', "expected '['");
        JsonValue result;
        result.kind = JsonValue::Kind::Array;
        skipWhitespace();
        if (consume(']'))
            return result;

        while (true)
        {
            result.array.push_back(parseValue());
            skipWhitespace();
            if (consume(']'))
                return result;
            expect(',', "expected ',' or ']'");
        }
    }

    JsonValue parseNumber()
    {
        const std::size_t start = position_;
        consume('-');

        if (peek() == '0')
        {
            ++position_;
        }
        else if (isDigit(peek()) && peek() != '0')
        {
            while (isDigit(peek()))
                ++position_;
        }
        else
        {
            fail("invalid number");
        }

        if (consume('.'))
        {
            if (!isDigit(peek()))
                fail("expected digits after decimal point");
            while (isDigit(peek()))
                ++position_;
        }

        if (peek() == 'e' || peek() == 'E')
        {
            ++position_;
            if (peek() == '+' || peek() == '-')
                ++position_;
            if (!isDigit(peek()))
                fail("expected exponent digits");
            while (isDigit(peek()))
                ++position_;
        }

        const std::string token(text_.substr(start, position_ - start));
        char* end = nullptr;
        const double number = std::strtod(token.c_str(), &end);
        if (end != token.c_str() + token.size())
            fail("invalid number");

        JsonValue result;
        result.kind = JsonValue::Kind::Number;
        result.number = number;
        return result;
    }

    std::string parseString()
    {
        expect('"', "expected a string");
        std::string result;

        while (position_ < text_.size())
        {
            const char c = text_[position_++];
            if (c == '"')
                return result;
            if (c == '\\')
            {
                if (position_ >= text_.size())
                    fail("unterminated escape");
                const char escaped = text_[position_++];
                switch (escaped)
                {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u':
                {
                    std::uint32_t codePoint = parseHex4();
                    if (codePoint >= 0xD800u && codePoint <= 0xDBFFu)
                    {
                        if (position_ + 1 >= text_.size() || text_[position_] != '\\'
                            || text_[position_ + 1] != 'u')
                            fail("expected low surrogate");
                        position_ += 2;
                        const std::uint32_t low = parseHex4();
                        if (low < 0xDC00u || low > 0xDFFFu)
                            fail("invalid low surrogate");
                        codePoint = 0x10000u + ((codePoint - 0xD800u) << 10) + (low - 0xDC00u);
                    }
                    else if (codePoint >= 0xDC00u && codePoint <= 0xDFFFu)
                    {
                        fail("unexpected low surrogate");
                    }
                    appendUtf8(result, codePoint);
                    break;
                }
                default:
                    fail("invalid escape");
                }
            }
            else
            {
                if (static_cast<unsigned char>(c) < 0x20u)
                    fail("control character in string");
                result.push_back(c);
            }
        }

        fail("unterminated string");
    }

    void consumeLiteral(std::string_view literal)
    {
        if (text_.substr(position_, literal.size()) != literal)
            fail("invalid literal");
        position_ += literal.size();
    }

    std::uint32_t parseHex4()
    {
        if (position_ + 4 > text_.size())
            fail("truncated unicode escape");
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char c = text_[position_++];
            value <<= 4;
            if (c >= '0' && c <= '9')
                value |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f')
                value |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                value |= static_cast<std::uint32_t>(c - 'A' + 10);
            else
                fail("invalid unicode escape");
        }
        return value;
    }

    static void appendUtf8(std::string& output, std::uint32_t codePoint)
    {
        if (codePoint <= 0x7Fu)
        {
            output.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint <= 0x7FFu)
        {
            output.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
            output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
        }
        else if (codePoint <= 0xFFFFu)
        {
            output.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
            output.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
            output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
        }
        else if (codePoint <= 0x10FFFFu)
        {
            output.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
            output.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
            output.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
            output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
        }
    }

    static bool isDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

    std::string_view text_;
    std::size_t position_ = 0;
};

inline std::string lowerAscii(std::string_view value)
{
    std::string result(value);
    for (char& c : result)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

inline double toleranceForPath(std::string_view path, const GoldenTolerance& tolerance)
{
    const std::string lower = lowerAscii(path);
    if (lower.find("position") != std::string::npos)
        return tolerance.position;
    if (lower.find("rotation") != std::string::npos
        || lower.find("orientation") != std::string::npos)
        return tolerance.rotation;
    return tolerance.absolute;
}

inline std::string kindName(JsonValue::Kind kind)
{
    switch (kind)
    {
    case JsonValue::Kind::Null: return "null";
    case JsonValue::Kind::Boolean: return "boolean";
    case JsonValue::Kind::Number: return "number";
    case JsonValue::Kind::String: return "string";
    case JsonValue::Kind::Array: return "array";
    case JsonValue::Kind::Object: return "object";
    }
    return "unknown";
}

inline std::string childPath(std::string_view path, std::string_view key)
{
    return std::string(path) + "." + std::string(key);
}

inline bool compareValues(
    const JsonValue& expected,
    const JsonValue& actual,
    std::string_view path,
    const GoldenTolerance& tolerance,
    std::string& error)
{
    if (expected.kind != actual.kind)
    {
        error = "type mismatch at " + std::string(path) + ": expected "
            + kindName(expected.kind) + ", actual " + kindName(actual.kind);
        return false;
    }

    switch (expected.kind)
    {
    case JsonValue::Kind::Null:
        return true;

    case JsonValue::Kind::Boolean:
        if (expected.boolean != actual.boolean)
        {
            error = "boolean mismatch at " + std::string(path) + ": expected "
                + (expected.boolean ? "true" : "false") + ", actual "
                + (actual.boolean ? "true" : "false");
            return false;
        }
        return true;

    case JsonValue::Kind::Number:
    {
        const double allowed = toleranceForPath(path, tolerance)
            + tolerance.relative * std::max(std::abs(expected.number), std::abs(actual.number));
        if (!std::isfinite(expected.number) || !std::isfinite(actual.number)
            || std::abs(expected.number - actual.number) > allowed)
        {
            error = "number mismatch at " + std::string(path) + ": expected "
                + formatGoldenNumber(expected.number) + ", actual "
                + formatGoldenNumber(actual.number) + " (tolerance "
                + formatGoldenNumber(allowed) + ")";
            return false;
        }
        return true;
    }

    case JsonValue::Kind::String:
        if (expected.string != actual.string)
        {
            error = "string mismatch at " + std::string(path) + ": expected '"
                + expected.string + "', actual '" + actual.string + "'";
            return false;
        }
        return true;

    case JsonValue::Kind::Array:
        if (expected.array.size() != actual.array.size())
        {
            error = "array size mismatch at " + std::string(path) + ": expected "
                + std::to_string(expected.array.size()) + ", actual "
                + std::to_string(actual.array.size());
            return false;
        }
        for (std::size_t i = 0; i < expected.array.size(); ++i)
        {
            const std::string elementPath = std::string(path) + "[" + std::to_string(i) + "]";
            if (!compareValues(expected.array[i], actual.array[i], elementPath, tolerance, error))
                return false;
        }
        return true;

    case JsonValue::Kind::Object:
        for (const auto& [key, expectedValue] : expected.object)
        {
            const auto actualIt = actual.object.find(key);
            if (actualIt == actual.object.end())
            {
                error = "missing key at " + childPath(path, key);
                return false;
            }
            if (!compareValues(
                    expectedValue, actualIt->second, childPath(path, key), tolerance, error))
                return false;
        }
        for (const auto& [key, unused] : actual.object)
        {
            (void)unused;
            if (expected.object.find(key) == expected.object.end())
            {
                error = "unexpected key at " + childPath(path, key);
                return false;
            }
        }
        return true;
    }

    return false;
}

inline void appendIndent(std::string& output, int depth)
{
    output.append(static_cast<std::size_t>(depth) * 2u, ' ');
}

inline void appendJsonString(std::string& output, std::string_view value)
{
    static constexpr char hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const char c : value)
    {
        switch (c)
        {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20u)
            {
                output += "\\u00";
                output.push_back(hex[(static_cast<unsigned char>(c) >> 4) & 0x0Fu]);
                output.push_back(hex[static_cast<unsigned char>(c) & 0x0Fu]);
            }
            else
            {
                output.push_back(c);
            }
            break;
        }
    }
    output.push_back('"');
}

inline void appendJson(std::string& output, const JsonValue& value, int depth)
{
    switch (value.kind)
    {
    case JsonValue::Kind::Null:
        output += "null";
        break;

    case JsonValue::Kind::Boolean:
        output += value.boolean ? "true" : "false";
        break;

    case JsonValue::Kind::Number:
        output += formatGoldenNumber(value.number);
        break;

    case JsonValue::Kind::String:
        appendJsonString(output, value.string);
        break;

    case JsonValue::Kind::Array:
        if (value.array.empty())
        {
            output += "[]";
            break;
        }
        output += "[\n";
        for (std::size_t i = 0; i < value.array.size(); ++i)
        {
            appendIndent(output, depth + 1);
            appendJson(output, value.array[i], depth + 1);
            if (i + 1 < value.array.size())
                output.push_back(',');
            output.push_back('\n');
        }
        appendIndent(output, depth);
        output.push_back(']');
        break;

    case JsonValue::Kind::Object:
        if (value.object.empty())
        {
            output += "{}";
            break;
        }
        output += "{\n";
        {
            std::size_t index = 0;
            for (const auto& [key, child] : value.object)
            {
                appendIndent(output, depth + 1);
                appendJsonString(output, key);
                output += ": ";
                appendJson(output, child, depth + 1);
                if (++index < value.object.size())
                    output.push_back(',');
                output.push_back('\n');
            }
        }
        appendIndent(output, depth);
        output.push_back('}');
        break;
    }
}

inline bool readTextFile(const std::filesystem::path& path, std::string& text, std::string& error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "could not open file '" + path.string() + "'";
        return false;
    }

    std::ostringstream contents;
    contents << stream.rdbuf();
    if (stream.bad())
    {
        error = "could not read file '" + path.string() + "'";
        return false;
    }
    text = contents.str();
    return true;
}

} // namespace golden_detail

inline GoldenResult writeGoldenFile(
    const std::filesystem::path& path, std::string_view text)
{
    std::string normalized = normalizeGoldenNewlines(text);

    std::string formatted;
    try
    {
        golden_detail::JsonParser parser(normalized);
        const golden_detail::JsonValue value = parser.parse();
        golden_detail::appendJson(formatted, value, 0);
        formatted.push_back('\n');
    }
    catch (const std::exception& exception)
    {
        return {false, false, std::string("golden JSON parse failed: ") + exception.what()};
    }

    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty())
    {
        std::error_code errorCode;
        std::filesystem::create_directories(parent, errorCode);
        if (errorCode)
        {
            return {false, false, "could not create golden directory '" + parent.string()
                + "': " + errorCode.message()};
        }
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
        return {false, false, "could not write golden file '" + path.string() + "'"};
    stream.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
    if (!stream)
        return {false, false, "could not finish writing golden file '" + path.string() + "'"};
    return {true, true, "wrote golden file '" + path.string() + "'"};
}

inline GoldenResult compareGoldenJson(
    std::string_view expectedJson,
    std::string_view actualJson,
    const GoldenTolerance& tolerance = {})
{
    using golden_detail::JsonParser;
    using golden_detail::JsonValue;
    using golden_detail::compareValues;

    try
    {
        JsonParser expectedParser(expectedJson);
        JsonParser actualParser(actualJson);
        const JsonValue expected = expectedParser.parse();
        const JsonValue actual = actualParser.parse();

        std::string error;
        if (!compareValues(expected, actual, "$", tolerance, error))
            return {false, false, std::move(error)};
        return {true, false, "golden snapshot matches"};
    }
    catch (const std::exception& exception)
    {
        return {false, false, std::string("golden JSON parse failed: ") + exception.what()};
    }
}

inline GoldenResult compareGoldenFile(
    const std::filesystem::path& actualPath,
    const std::filesystem::path& goldenPath,
    const GoldenTolerance& tolerance = {},
    bool update = false)
{
    std::string actual;
    std::string error;
    if (!golden_detail::readTextFile(actualPath, actual, error))
        return {false, false, error};

    if (update || updateGoldenRequested())
        return writeGoldenFile(goldenPath, actual);

    std::string expected;
    if (!golden_detail::readTextFile(goldenPath, expected, error))
        return {false, false, "golden file missing or unreadable: " + error};

    return compareGoldenJson(expected, actual, tolerance);
}

}
