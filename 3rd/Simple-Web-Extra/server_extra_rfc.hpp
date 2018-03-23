#ifndef SERVER_EXTRA_RFC_HPP
#define SERVER_EXTRA_RFC_HPP

#include <sstream>
#include <string>
#include <vector>
#include <exception>
#include <unordered_map>
#include <boost/lexical_cast.hpp>

namespace SimpleWeb {
// code from served
// https://github.com/datasift/served/blob/master/src/served/uri.cpp
// by MIT license
static const char hex_table[] = "0123456789ABCDEF";

static const char dec_to_hex[256] = {
    /*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
    /* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,

    /* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

    /* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

    /* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};

static std::string
query_escape(const std::string& s) {
    const unsigned char* src_ptr = (const unsigned char*)s.c_str();
    const size_t         src_len = s.length();

    unsigned char        tmp[src_len * 3];
    unsigned char*       end = tmp;

    const unsigned char * const eol = src_ptr + src_len;

    for (; src_ptr < eol; ++src_ptr) {
        unsigned char c = *src_ptr;
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~')
        {
            *end++ = *src_ptr;
        } else {
            *end++ = '%';
            *end++ = hex_table[*src_ptr >> 4];
            *end++ = hex_table[*src_ptr & 0x0F];
        }
    }
    return std::string((char*)tmp, (char*)end);
}

static std::string
query_unescape(const std::string& s) {
    const unsigned char* src_ptr = (const unsigned char*)s.c_str();
    const size_t         src_len = s.length();

    const unsigned char* const eol = src_ptr + src_len;
    const unsigned char* const last_decodable = eol - 2;

    char  tmp[src_len];
    char* end = tmp;

    while (src_ptr < last_decodable) {
        if (*src_ptr == '%') {
            char dec1, dec2;
            if (-1 != (dec1 = dec_to_hex[*(src_ptr + 1)])
                && -1 != (dec2 = dec_to_hex[*(src_ptr + 2)]))
            {
                *end++ = (dec1 << 4) + dec2;
                src_ptr += 3;
                continue;
            }
        }
        *end++ = *src_ptr++;
    }

    while (src_ptr < eol) {
        *end++ = *src_ptr++;
    }

    return std::string((char*)tmp, (char*)end);
}

    class Exception: public std::exception {
    };

    class QueryDict: public std::unordered_map<std::string, std::string> {
    public:
        QueryDict () {}
        QueryDict (std::string const &query) {
            auto off = query.find('?');
            if (off == query.npos) return;
            char const *b = &query[off] + 1;
            char const *e = &query[0] + query.size();
            while (b < e) {
                char const *sep = b;
                while (sep < e && *sep != '&') ++sep;
                char const *eq = b;
                while (eq < sep && *eq != '=') ++eq;
                // b  <= eq <= sep <= e
                //       =      &
                if (!(b < eq)) throw Exception();
                if (!(eq + 1 < sep)) throw Exception();
                std::string key = query_unescape(std::string(b, eq));
                std::string value = query_unescape(std::string(eq + 1, sep));
                insert(std::make_pair(key, value));
                b = sep + 1;
            }
        }
        template <typename T>
        T get (std::string const &key, T def) {
            auto it = find(key);
            if (it == end()) return def;
            return boost::lexical_cast<T>(it->second);
        }

        std::string encode (bool amps = false) const {
            std::ostringstream ss;
            bool first = !amps;
            for (auto p: *this) {
                if (first) first = false;
                else ss << '&';
                ss << query_escape(p.first) << '=' << query_escape(p.second);
            }
            return ss.str();
        }
    };

    class Part: public std::unordered_map<std::string, std::string> {
        std::string name_;
        std::string body_;
    public:
        friend class MultiPart;
        std::string const &name () const {
            return name_;
        }
        std::string const &body () const {
            return body_;
        }
    };

    class MultiPart: public std::vector<Part> {
        static size_t spaceCRLF (std::string const &s, size_t off) {
            off = s.find("\r\n", off);
            if (off == std::string::npos) {
                throw Exception();
            }
            return off + 2;
        }
        static size_t getline (std::string const &s, size_t off, std::string *line) {
            auto e = s.find("\r\n", off);
            if (e == std::string::npos) {
                throw Exception();
            }
            *line = s.substr(off, e - off);
            return e + 2;
        }
    public:
        MultiPart (std::string const &content_type,
                   std::string const &body) {
            //std::cerr << "CT: " << content_type << std::endl;
            size_t off = content_type.find("multipart/");
            if (off != 0) {
                throw Exception();
            }
            off = content_type.find("boundary=");
            if (off == std::string::npos) {
                throw Exception();
            }
            off += 9; // len("boundary")
            if (off >= content_type.size()) {
                throw Exception();
            }
            bool quoted = false;
            if (content_type[off] == '"') {
                quoted = true;
                ++off;
            }
            std::string boundary = content_type.substr(off);
            for (;;) {
                if (boundary.empty()) break;
                if (!std::isspace(boundary.back())) break;
                boundary.pop_back();
            }
            if (quoted) {
                if (boundary.empty() || boundary.back() != '"') {
                    throw Exception();
                }
                boundary.pop_back();
            }
            if (boundary.empty()) {
                throw Exception();
            }

            boundary = "--" + boundary;
            //std::cerr << "B: " << boundary << std::endl;

            off = body.find(boundary);
            if (off != 0) throw Exception();
            off = spaceCRLF(body, off);
            boundary = "\r\n" + boundary;
            while (off < body.size()) {
                //std::cerr << "P" << std::endl;
                size_t end = body.find(boundary, off);
                if (end == std::string::npos) throw Exception();
                if (end <= off) throw Exception();
                // parse header
                std::string name;
                for (;;) {
                    std::string line;
                    size_t off2 = getline(body, off, &line);
                    if (off2 <= off) throw Exception();
                    off = off2;
                    if (line.empty()) break;
                    //std::cerr << "H: " << line << std::endl;
                    // try to parse std::string line and find name
                    //std::cerr << "Header: " << line << std::endl;
                    do {
                        auto off3 = line.find("Content-Disposition:");
                        if (off3 != 0) continue;
                        off3 = line.find("name=\"");
                        if (off3 == line.npos) break;
                        off3 += 6;
                        if (off3 >= line.size()) break;
                        auto off4 = line.find('"', off3);
                        if (off4 == line.npos) break;
                        name = std::string(&line[off3], &line[off4]);
                        //std::cerr << "name: " << name << std::endl;
                    } while (false);
                }
                Part part;
                part.name_ = name;
                part.body_ = body.substr(off, end - off);
                push_back(std::move(part));
                off = end + boundary.size();
                if (off + 2 > body.size()) throw Exception();
                if (body[off] == '-' && body[off+1] == '-') break;
                off = spaceCRLF(body, off);
                if (off == std::string::npos) break;
            }
        }
    };
}

#endif

