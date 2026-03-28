#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <sstream>

namespace json {

inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

// Find next unescaped quote starting from pos
inline size_t findUnescapedQuote(const std::string& s, size_t pos) {
    while (pos < s.size()) {
        size_t q = s.find('"', pos);
        if (q == std::string::npos) return std::string::npos;
        // Count preceding backslashes
        size_t backslashes = 0;
        size_t p = q;
        while (p > 0 && s[p - 1] == '\\') { --p; ++backslashes; }
        if (backslashes % 2 == 0) return q; // even backslashes = unescaped quote
        pos = q + 1;
    }
    return std::string::npos;
}

inline std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

// Parse [1, 2, 3] into a vector of ints
inline std::vector<int> toIntArray(const std::string& s) {
    std::vector<int> out;
    size_t a = s.find('['), b = s.find(']');
    if (a == std::string::npos || b == std::string::npos) return out;
    std::stringstream ss(s.substr(a + 1, b - a - 1));
    std::string tok;
    while (std::getline(ss, tok, ',')) out.push_back(std::stoi(trim(tok)));
    return out;
}

// Parse [1.0, 2.5, 3.0] into a vector of floats
inline std::vector<float> toFloatArray(const std::string& s) {
    std::vector<float> out;
    size_t a = s.find('['), b = s.find(']');
    if (a == std::string::npos || b == std::string::npos) return out;
    std::stringstream ss(s.substr(a + 1, b - a - 1));
    std::string tok;
    while (std::getline(ss, tok, ',')) out.push_back(std::stof(trim(tok)));
    return out;
}

// A parsed JSON object — flat key/value pairs (string values stored raw)
struct Object {
    std::unordered_map<std::string, std::string> kv;

    bool has(const std::string& key) const { return kv.count(key) > 0; }

    std::string str(const std::string& key, const std::string& fallback = "") const {
        auto it = kv.find(key);
        return it != kv.end() ? unquote(it->second) : fallback;
    }

    int num(const std::string& key, int fallback = 0) const {
        auto it = kv.find(key);
        if (it == kv.end()) return fallback;
        try { return std::stoi(unquote(it->second)); } catch (...) { return fallback; }
    }

    float numf(const std::string& key, float fallback = 0.0f) const {
        auto it = kv.find(key);
        if (it == kv.end()) return fallback;
        try { return std::stof(unquote(it->second)); } catch (...) { return fallback; }
    }

    bool boolean(const std::string& key, bool fallback = false) const {
        auto it = kv.find(key);
        if (it == kv.end()) return fallback;
        std::string v = unquote(it->second);
        return v == "true" || v == "1";
    }

    std::vector<int> intArray(const std::string& key) const {
        auto it = kv.find(key);
        return it != kv.end() ? toIntArray(it->second) : std::vector<int>{};
    }

    std::vector<float> floatArray(const std::string& key) const {
        auto it = kv.find(key);
        return it != kv.end() ? toFloatArray(it->second) : std::vector<float>{};
    }
};

// Parse a JSON string directly (same format as file)
inline Object parse(const std::string& content) {
    Object obj;
    size_t start = content.find('{');
    size_t end = content.rfind('}');
    if (start == std::string::npos || end == std::string::npos) return obj;

    std::string inner = content.substr(start + 1, end - start - 1);

    size_t pos = 0;
    while (pos < inner.size()) {
        size_t ks = findUnescapedQuote(inner, pos);
        if (ks == std::string::npos) break;
        size_t ke = findUnescapedQuote(inner, ks + 1);
        if (ke == std::string::npos) break;
        std::string key = inner.substr(ks + 1, ke - ks - 1);

        size_t colon = inner.find(':', ke + 1);
        if (colon == std::string::npos) break;

        size_t vs = inner.find_first_not_of(" \t\r\n", colon + 1);
        if (vs == std::string::npos) break;

        std::string value;
        if (inner[vs] == '"') {
            size_t ve = findUnescapedQuote(inner, vs + 1);
            if (ve == std::string::npos) break;
            value = inner.substr(vs, ve - vs + 1);
            pos = ve + 1;
        } else if (inner[vs] == '[') {
            size_t ve = inner.find(']', vs);
            if (ve == std::string::npos) break;
            value = inner.substr(vs, ve - vs + 1);
            pos = ve + 1;
        } else {
            size_t ve = inner.find_first_of(",}\r\n", vs);
            if (ve == std::string::npos) ve = inner.size();
            value = trim(inner.substr(vs, ve - vs));
            pos = ve;
        }

        obj.kv[key] = value;

        size_t comma = inner.find(',', pos);
        if (comma != std::string::npos) pos = comma + 1;
        else break;
    }

    return obj;
}

// Parse a flat JSON object from a file. Returns empty Object on failure.
inline Object parseFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return parse(content);
}

} // namespace json
