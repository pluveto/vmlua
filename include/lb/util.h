#pragma once
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

namespace lb {
namespace file_util {
bool is_readable(const std::string &path) {
    std::ifstream file(path);
    return file.good();
}
}  // namespace file_util

namespace string_util {
bool is_number(const std::string &s) {
    auto beg = s.begin();
    if (*beg == '-' || *beg == '+') {
        ++beg;
        if(beg == s.end()){
            return false;
        }
    }
    return !s.empty() && std::all_of(beg, s.end(), ::isdigit);
}
inline bool start_with(const std::string &str, const std::string &prefix) { return str.find(prefix) == 0; }
auto split(const std::string &str, char delim, bool remove_empty = true, int max_parts = -1) {
    std::vector<std::string> parts;
    std::stringstream ss(str);
    std::string part;
    while (std::getline(ss, part, delim)) {
        if (!remove_empty || !part.empty()) {
            parts.push_back(part);
        }
        if (max_parts > 0 && parts.size() >= max_parts) {
            break;
        }
    }
    return parts;
}
template <typename... Args>
std::string const_concat(Args const &...args) {
    size_t len = 0;
    for (auto s : {args...})
        len += strlen(s);

    std::string result;
    result.reserve(len);
    for (auto s : {args...})
        result += s;
    return result;
}

// limit max 8 arguments otherwise slow and consumptive
template <typename... Args>
typename std::enable_if<sizeof...(Args) <= 8, std::string>::type concat(Args const &...args) {
    std::stringstream ss;
    (ss << ... << args);
    return ss.str();
}
}  // namespace string_util

}  // namespace lb
