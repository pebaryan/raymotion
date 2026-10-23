// json.cpp -- JSON parsing, access and serialisation.
#include "rmtn/json.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace rmtn {
namespace {

const Json& nullJson() {
    static const Json n;
    return n;
}

class Parser {
public:
    explicit Parser(std::string_view s) : s_(s) {}

    Json document() {
        Json v = value(0);
        ws();
        if (i_ < s_.size()) fail("unexpected trailing characters");
        return v;
    }

private:
    [[noreturn]] void fail(const char* what) const {
        size_t line = 1, col = 1;
        for (size_t k = 0; k < i_ && k < s_.size(); k++) {
            if (s_[k] == '\n') { line++; col = 1; } else col++;
        }
        char buf[160];
        std::snprintf(buf, sizeof buf, "json: %s at line %zu, column %zu", what, line, col);
        throw std::runtime_error(buf);
    }
    void ws() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) i_++;
    }
    bool eat(char c) {
        ws();
        if (i_ < s_.size() && s_[i_] == c) { i_++; return true; }
        return false;
    }
    void expect(char c, const char* what) { if (!eat(c)) fail(what); }
    bool word(std::string_view w) {
        if (s_.substr(i_, w.size()) != w) return false;
        i_ += w.size();
        return true;
    }

    Json value(int depth) {
        if (depth > 256) fail("nesting too deep");
        ws();
        if (i_ >= s_.size()) fail("unexpected end of input");
        char c = s_[i_];
        if (c == '{') return object(depth);
        if (c == '[') return array(depth);
        if (c == '"') return Json(string());
        if (c == '-' || (c >= '0' && c <= '9')) return Json(number());
        if (word("true")) return Json(true);
        if (word("false")) return Json(false);
        if (word("null")) return Json();
        fail("unexpected character");
    }
    Json object(int depth) {
        i_++;
        Json::Object o;
        if (eat('}')) return Json(std::move(o));
        do {
            ws();
            if (i_ >= s_.size() || s_[i_] != '"') fail("expected a string key");
            std::string key = string();
            expect(':', "expected ':'");
            Json v = value(depth + 1);
            bool replaced = false;                       // duplicate keys: last one wins
            for (auto& kv : o) if (kv.first == key) { kv.second = std::move(v); replaced = true; break; }
            if (!replaced) o.emplace_back(std::move(key), std::move(v));
        } while (eat(','));
        expect('}', "expected ',' or '}'");
        return Json(std::move(o));
    }
    Json array(int depth) {
        i_++;
        Json::Array a;
        if (eat(']')) return Json(std::move(a));
        do a.push_back(value(depth + 1)); while (eat(','));
        expect(']', "expected ',' or ']'");
        return Json(std::move(a));
    }
    double number() {
        size_t start = i_;
        auto digits = [&] {
            size_t d = i_;
            while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') i_++;
            return i_ - d;
        };
        if (s_[i_] == '-') i_++;
        if (i_ < s_.size() && s_[i_] == '0') i_++;
        else if (!digits()) fail("invalid number");
        if (i_ < s_.size() && s_[i_] == '.') { i_++; if (!digits()) fail("invalid number"); }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            i_++;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) i_++;
            if (!digits()) fail("invalid number");
        }
        return std::strtod(std::string(s_.substr(start, i_ - start)).c_str(), nullptr);
    }
    unsigned hex4() {
        if (i_ + 4 > s_.size()) fail("truncated \\u escape");
        unsigned v = 0;
        for (int k = 0; k < 4; k++) {
            char h = s_[i_++];
            v <<= 4;
            if (h >= '0' && h <= '9') v |= (unsigned)(h - '0');
            else if (h >= 'a' && h <= 'f') v |= (unsigned)(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') v |= (unsigned)(h - 'A' + 10);
            else fail("invalid \\u escape");
        }
        return v;
    }
    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
        }
    }
    std::string string() {
        i_++;                                            // opening quote
        std::string out;
        while (true) {
            if (i_ >= s_.size()) fail("unterminated string");
            char c = s_[i_++];
            if (c == '"') return out;
            if ((unsigned char)c < 0x20) fail("control character in string");
            if (c != '\\') { out += c; continue; }
            if (i_ >= s_.size()) fail("unterminated string");
            char e = s_[i_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp < 0xDC00) {       // surrogate pair
                        if (!word("\\u")) fail("unpaired surrogate");
                        unsigned lo = hex4();
                        if (lo < 0xDC00 || lo >= 0xE000) fail("unpaired surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp < 0xE000) fail("unpaired surrogate");
                    utf8(out, cp);
                    break;
                }
                default: fail("invalid escape");
            }
        }
    }

    std::string_view s_;
    size_t i_ = 0;
};

[[noreturn]] void typeError(const char* want) { throw std::runtime_error(std::string("json: expected ") + want); }

void dumpString(std::string& out, const std::string& s) {
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", (unsigned)c); out += b; }
                else out += c;
        }
    }
    out += '"';
}

void dumpTo(std::string& out, const Json& j) {
    switch (j.type()) {
        case Json::Type::Null: out += "null"; break;
        case Json::Type::Bool: out += j.asBool() ? "true" : "false"; break;
        case Json::Type::Number: {
            double d = j.asNumber();
            if (!std::isfinite(d)) { out += "null"; break; }
            char b[32];
            if (d == std::floor(d) && std::fabs(d) < 1e15) std::snprintf(b, sizeof b, "%.0f", d);
            else std::snprintf(b, sizeof b, "%.17g", d);
            out += b;
            break;
        }
        case Json::Type::String: dumpString(out, j.asString()); break;
        case Json::Type::Array: {
            out += '[';
            bool first = true;
            for (const Json& v : j.asArray()) { if (!first) out += ','; first = false; dumpTo(out, v); }
            out += ']';
            break;
        }
        case Json::Type::Object: {
            out += '{';
            bool first = true;
            for (const auto& [k, v] : j.asObject()) {
                if (!first) out += ',';
                first = false;
                dumpString(out, k);
                out += ':';
                dumpTo(out, v);
            }
            out += '}';
            break;
        }
    }
}

int hexDigit(char h) {
    if (h >= '0' && h <= '9') return h - '0';
    if (h >= 'a' && h <= 'f') return h - 'a' + 10;
    if (h >= 'A' && h <= 'F') return h - 'A' + 10;
    return -1;
}

} // namespace

Json Json::parse(std::string_view text) { return Parser(text).document(); }

bool Json::asBool() const { if (!isBool()) typeError("a boolean"); return std::get<bool>(v_); }
double Json::asNumber() const { if (!isNumber()) typeError("a number"); return std::get<double>(v_); }
const std::string& Json::asString() const { if (!isString()) typeError("a string"); return std::get<std::string>(v_); }
const Json::Array& Json::asArray() const { if (!isArray()) typeError("an array"); return std::get<Array>(v_); }
const Json::Object& Json::asObject() const { if (!isObject()) typeError("an object"); return std::get<Object>(v_); }

const Json& Json::operator[](std::string_view key) const {
    if (!isObject()) return nullJson();
    for (const auto& kv : std::get<Object>(v_)) if (kv.first == key) return kv.second;
    return nullJson();
}
const Json& Json::operator[](size_t index) const {
    if (!isArray() || index >= std::get<Array>(v_).size()) return nullJson();
    return std::get<Array>(v_)[index];
}
bool Json::has(std::string_view key) const {
    if (!isObject()) return false;
    for (const auto& kv : std::get<Object>(v_)) if (kv.first == key) return true;
    return false;
}
size_t Json::size() const {
    if (isArray()) return std::get<Array>(v_).size();
    if (isObject()) return std::get<Object>(v_).size();
    return 0;
}
Json& Json::set(std::string_view key, Json value) {
    if (!isObject()) v_ = Object{};
    auto& o = std::get<Object>(v_);
    for (auto& kv : o) if (kv.first == key) { kv.second = std::move(value); return kv.second; }
    o.emplace_back(std::string(key), std::move(value));
    return o.back().second;
}

double Json::num(std::string_view key, double fallback) const {
    const Json& v = (*this)[key];
    return v.isNull() ? fallback : v.asNumber();
}
int Json::integer(std::string_view key, int fallback) const {
    const Json& v = (*this)[key];
    return v.isNull() ? fallback : (int)std::lround(v.asNumber());
}
bool Json::flag(std::string_view key, bool fallback) const {
    const Json& v = (*this)[key];
    return v.isNull() ? fallback : v.asBool();
}
std::string Json::str(std::string_view key, std::string_view fallback) const {
    const Json& v = (*this)[key];
    return v.isNull() ? std::string(fallback) : v.asString();
}
Color Json::color(std::string_view key, Color fallback) const {
    const Json& v = (*this)[key];
    if (v.isNull()) return fallback;
    if (v.isString()) return parseColor(v.asString());
    const Array& a = v.asArray();
    if (a.size() != 3 && a.size() != 4) typeError("a colour [r, g, b(, a)]");
    auto ch = [&](size_t i) {
        double d = a[i].asNumber();
        return (unsigned char)std::lround(d < 0 ? 0 : d > 255 ? 255 : d);
    };
    return { ch(0), ch(1), ch(2), a.size() == 4 ? ch(3) : (unsigned char)255 };
}

Json Json::merged(const Json& over) const {
    if (!isObject() || !over.isObject()) return over;
    Json out = *this;
    for (const auto& [k, v] : over.asObject()) out.set(k, (*this)[k].merged(v));
    return out;
}

std::string Json::dump() const {
    std::string out;
    dumpTo(out, *this);
    return out;
}

Color parseColor(std::string_view s) {
    auto bad = [&]() -> Color { throw std::runtime_error("json: invalid colour \"" + std::string(s) + "\""); };
    if (s.empty() || s[0] != '#') return bad();
    std::string_view h = s.substr(1);
    for (char c : h) if (hexDigit(c) < 0) return bad();
    if (h.size() == 3)
        return { (unsigned char)(hexDigit(h[0]) * 17), (unsigned char)(hexDigit(h[1]) * 17),
                 (unsigned char)(hexDigit(h[2]) * 17), 255 };
    if (h.size() != 6 && h.size() != 8) return bad();
    auto byte = [&](size_t i) { return (unsigned char)(hexDigit(h[i]) * 16 + hexDigit(h[i + 1])); };
    return { byte(0), byte(2), byte(4), h.size() == 8 ? byte(6) : (unsigned char)255 };
}

} // namespace rmtn
