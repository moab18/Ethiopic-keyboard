#pragma once

#include <cctype>
#include <istream>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace ethio {

class Json {
public:
    struct Iterator {
        using inner = std::unordered_map<std::string, Json>::const_iterator;
        inner it;
        const std::string &key() const { return it->first; }
        const Json &value() const { return it->second; }
        bool operator!=(const Iterator &o) const { return it != o.it; }
        Iterator &operator++() { ++it; return *this; }
    };

    Iterator begin() const
    {
        if (!is_obj_) throw std::runtime_error("Json: not an object");
        return {obj_.cbegin()};
    }
    Iterator end() const
    {
        if (!is_obj_) throw std::runtime_error("Json: not an object");
        return {obj_.cend()};
    }

    Json() : is_obj_(false) {}

    const Json &operator[](const std::string &key) const
    {
        if (!is_obj_) throw std::runtime_error("Json: not an object");
        auto it = obj_.find(key);
        if (it == obj_.end()) throw std::runtime_error("Json: key not found: " + key);
        return it->second;
    }

    std::string value(const std::string &key, const std::string &dflt) const
    {
        if (!is_obj_) return dflt;
        auto it = obj_.find(key);
        if (it == obj_.end() || !it->second.is_str_) return dflt;
        return it->second.str_;
    }

    operator std::string() const { return is_str_ ? str_ : ""; }

    friend std::istream &operator>>(std::istream &is, Json &j);

private:
    friend class Parser;
    bool is_obj_ = false;
    bool is_str_ = false;
    std::string str_;
    std::unordered_map<std::string, Json> obj_;
};

class Parser {
public:
    explicit Parser(std::istream &is) : is_(is), ch_(0) { next(); }

    Json parse()
    {
        skip_ws();
        Json j;
        parse_value(j);
        return j;
    }

private:
    std::istream &is_;
    int ch_;

    void next() { ch_ = is_.get(); }
    void expect(int c)
    {
        if (ch_ != c)
            throw std::runtime_error(std::string("Json: expected '") + static_cast<char>(c) + "'");
        next();
    }

    void skip_ws()
    {
        while (ch_ > 0 && std::isspace(static_cast<unsigned char>(ch_)))
            next();
    }

    void parse_value(Json &j)
    {
        skip_ws();
        if (ch_ == '"') {
            j.is_str_ = true;
            j.str_ = parse_string();
        } else if (ch_ == '{') {
            j.is_obj_ = true;
            parse_object(j);
        } else {
            throw std::runtime_error(std::string("Json: unexpected char '") +
                                     static_cast<char>(ch_) + "'");
        }
    }

    void parse_object(Json &j)
    {
        expect('{');
        skip_ws();
        if (ch_ == '}') { next(); return; }
        for (;;) {
            skip_ws();
            std::string key = parse_string();
            skip_ws();
            expect(':');
            Json val;
            parse_value(val);
            j.obj_[std::move(key)] = std::move(val);
            skip_ws();
            if (ch_ == '}') { next(); return; }
            expect(',');
        }
    }

    std::string parse_string()
    {
        expect('"');
        std::string s;
        while (ch_ != '"') {
            if (ch_ < 0x20 && ch_ >= 0)
                throw std::runtime_error("Json: control char in string");
            if (ch_ == '\\') {
                next();
                switch (ch_) {
                case '"':  s += '"';  break;
                case '\\': s += '\\'; break;
                case '/':  s += '/';  break;
                case 'b':  s += '\b'; break;
                case 'f':  s += '\f'; break;
                case 'n':  s += '\n'; break;
                case 'r':  s += '\r'; break;
                case 't':  s += '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    for (int i = 0; i < 4; i++) {
                        next();
                        cp <<= 4;
                        if (ch_ >= '0' && ch_ <= '9')      cp |= ch_ - '0';
                        else if (ch_ >= 'a' && ch_ <= 'f') cp |= ch_ - 'a' + 10;
                        else if (ch_ >= 'A' && ch_ <= 'F') cp |= ch_ - 'A' + 10;
                        else throw std::runtime_error("Json: bad \\u escape");
                    }
                    if (cp < 0x80) {
                        s += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        s += static_cast<char>(0xC0 | (cp >> 6));
                        s += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        s += static_cast<char>(0xE0 | (cp >> 12));
                        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        s += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default:
                    throw std::runtime_error("Json: bad escape '\\" +
                                             std::string(1, static_cast<char>(ch_)) + "'");
                }
            } else {
                s += static_cast<char>(ch_);
            }
            next();
        }
        expect('"');
        return s;
    }
};

inline std::istream &operator>>(std::istream &is, Json &j)
{
    Parser p(is);
    j = p.parse();
    return is;
}

} // namespace ethio
