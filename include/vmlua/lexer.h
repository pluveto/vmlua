#pragma once
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "types.h"
namespace lb::vmlua {
class lexer {
public:
    using token_yield = std::optional<std::tuple<token_t, location>>;

private:
    std::ifstream &_file;
    location _loc;
    token_yield _begin_token;
    location eat_whitespace() {
        auto c = _file.get();
        auto next_loc = _loc;
        auto flag = false;
        while (std::isspace(static_cast<u_char>(c))) {
            flag = true;
            next_loc = next_loc.step(c == '\n');
            if (_file.peek() == EOF) {
                break;
            }
            c = _file.get();
        }
        // if no space is eaten, return the original location
        if (!flag) {
            _file.seekg(_loc.offset);
        }
        return next_loc;
    }
    token_yield eat_number() {
        auto ident = std::string{};
        auto next_loc = _loc;
        auto c = _file.get();
        if (c == '-') {
            ident += c;
            c = _file.get();
            next_loc = next_loc.step(false);
        }
        if (c == '+') {
            c = _file.get();
            next_loc = next_loc.step(false);
        }
        while (std::isdigit(static_cast<unsigned char>(c))) {
            ident += c;
            next_loc = next_loc.step(false);
            c = _file.get();
        }
        if (!ident.empty()) {
            return std::make_tuple(token_t{token_kind::t_number, ident, _loc}, next_loc);
        }
        _file.seekg(_loc.offset);

        return std::nullopt;
    }
    token_yield eat_identifier() {
        // auto ident = std::string{};
        std::string ident;
        auto next_loc = _loc;
        auto c = _file.get();
        while (std::isalnum(c) || c == '_') {
            ident += c;
            next_loc = next_loc.step(false);
            c = _file.get();
        }
        if (ident.empty()) {
            std::cout << "[error] empty ident" << std::endl;
            _file.seekg(_loc.offset);
            return std::nullopt;
        }
        if (std::isdigit(static_cast<unsigned char>(ident[0]))) {
            std::cout << "[error] ident starts with digit " << ident[0] << std::endl;
            _file.seekg(_loc.offset);
            return std::nullopt;
        }

        return std::make_tuple(token_t{token_kind::t_identifier, ident, _loc}, next_loc);
    }

    token_yield eat_keyword() {
        static const std::vector<std::string> keywords = {
            "function", "end",  "if",    "elseif", "else", "while", "do",    "in",   "nil",   "repeat",
            "util",     "true", "false", "and",    "or",   "not",   "break", "then", "local", "return"};

        for (auto const &keyword : keywords) {
            _file.seekg(_loc.offset);
            auto next_loc = _loc;
            auto c = _file.get();
            auto miss = false;
            for (auto const &char_ : keyword) {
                if (c != char_) {
                    miss = true;
                    break;
                }
                // is this ok?
                next_loc = std::move(next_loc.step(false));
                c = _file.get();
            }
            // note partial match is not allowed
            if (!miss && next_loc.offset - _loc.offset == keyword.size()) {
                return std::make_tuple(token_t{token_kind::t_keyword, keyword, _loc}, next_loc);
            }
        }
        return std::nullopt;
    }

    token_yield eat_syntax() {
        static const std::vector<char> syntax = {';', '=', '(', ')', ','};
        for (auto const &char_ : syntax) {
            auto next_loc = _loc;
            auto c = _file.get();
            next_loc = std::move(next_loc.step(false));
            if (c == char_) {
                if (c == '=' && _file.get() == '=') {
                    return std::nullopt;
                }
                return std::make_tuple(token_t{token_kind::t_syntax, std::string{char_}, _loc}, next_loc);
            }
            _file.seekg(_loc.offset);
        }
        return std::nullopt;
    }

    token_yield eat_operator() {
        static const std::vector<std::string> operators = {
            "and ", "or ", "not ", "==", "!=", ">=", "<=", "+", "-", "*", "/", "^", "%", ">", "<",  //
        };

        for (auto const &op : operators) {
            auto next_loc = _loc;
            auto c = _file.get();
            auto miss = false;
            for (auto const &char_ : op) {
                if (c != char_) {
                    miss = true;
                    break;
                }
                // is this ok?
                next_loc = std::move(next_loc.step(false));
                c = _file.get();
            }
            // note partial match is not allowed
            if (!miss && next_loc.offset - _loc.offset == op.size()) {
                return std::make_tuple(token_t{token_kind::t_operator, op, _loc}, next_loc);
            }
            // if miss or parital match
            _file.seekg(_loc.offset);
        }
        return std::nullopt;
    }

    std::vector<std::function<token_yield()>> sub_lexers;

    void load_lexers() {
        if (!sub_lexers.empty()) {
            // std::cout << "[debug] skip load lexer" << sub_lexers.size() << std::endl;
            return;
        }
        // std::cout << "[debug] load lexers" << std::endl;
        sub_lexers.reserve(5);
        sub_lexers.push_back([this]() {
            std::cout << "[debug] call eat_keyword" << std::endl;
            return eat_keyword();
        });
        sub_lexers.push_back([this]() {
            std::cout << "[debug] call eat_identifier" << std::endl;
            return eat_identifier();
        });
        sub_lexers.push_back([this]() {
            std::cout << "[debug] call eat_number" << std::endl;
            return eat_number();
        });
        sub_lexers.push_back([this]() {
            std::cout << "[debug] call eat_syntax" << std::endl;
            return eat_syntax();
        });
        sub_lexers.push_back([this]() {
            std::cout << "[debug] call eat_operator" << std::endl;
            return eat_operator();
        });
    }

public:
    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = std::unique_ptr<token_yield>;
        using pointer = value_type *;
        using reference = value_type const &;
        size_t _file_off;
        lexer &_parent;
        value_type _token_yield;
        iterator(lexer &parent, const size_t file_off) : _parent(parent), _file_off(file_off) { load_token(); }
        iterator(const iterator &other) : _parent(other._parent), _file_off(other._file_off) { load_token(); }
        void load_token() {
            if (_file_off == EOF) {
                _token_yield = std::make_unique<token_yield>(
                    std::optional(std::tuple(token_t{token_kind::t_eof, "", _parent._loc}, _parent._loc)));
                return;
            }
            _parent._file.seekg(_file_off);
            _token_yield = std::make_unique<token_yield>(_parent.next());
        }
        reference operator*() const { return _token_yield; }
        pointer operator->() { return &_token_yield; }
        // ++it
        iterator &operator++() {
            if (_file_off == EOF) {
                _token_yield = std::make_unique<token_yield>(
                    std::optional(std::tuple(token_t{token_kind::t_eof, "", _parent._loc}, _parent._loc)));
                return *this;
            }
            _parent._file.seekg(_file_off);
            _token_yield = std::make_unique<token_yield>(_parent.next());
            if (_token_yield.get()->has_value()) {
                auto &t = _token_yield.get()->value();
                _file_off = std::get<1>(t).offset;
                _parent._loc = std::get<1>(t);
            } else {
                _file_off = EOF;
            }
            return *this;
        }

        // it++
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        friend bool operator==(const iterator &a, const iterator &b) { return a._file_off == b._file_off; };
        friend bool operator!=(const iterator &a, const iterator &b) { return a._file_off != b._file_off; };
    };

    explicit lexer(std::ifstream &file) : _file(file), _loc(location{}) {}

    iterator begin() { return iterator(*this, 0); }
    iterator end() { return iterator(*this, EOF); }

    void reset() {
        _file.seekg(0);
        _loc = location{};
    }
    token_yield next() {
        _file.seekg(_loc.offset);
        std::cout << "[debug] current file offset: " << _file.tellg() << std::endl;
        load_lexers();
        if (_file.peek() == EOF) {
            return std::nullopt;
        }
        _loc = eat_whitespace();

        std::cout << "[debug] get space. current file offset: " << _file.tellg() << std::endl;

        if (_file.peek() == EOF) {
            return std::nullopt;
        }
        for (auto &sub_lexer : sub_lexers) {
            if (nullptr == sub_lexer) {
                std::cerr << "[error] sub_lexer is nullptr" << std::endl;
                exit(1);
            }
            auto lex = sub_lexer();
            if (lex) {
                _loc = std::get<1>(lex.value());
                _file.seekg(_loc.offset);
                std::cout << "[debug] get token. current file offset: " << _file.tellg() << std::endl;
                std::cout << "[debug] lex: " << std::get<0>(lex.value()).to_string() << std::endl;
                return lex.value();
            }
            _file.seekg(_loc.offset);
            std::cout << "[debug] no token. current file offset: " << _file.tellg() << std::endl;
        }
        if (_file.peek() == EOF) {
            return std::nullopt;
        } else {
            throw std::runtime_error("unexpected character: " + std::string{static_cast<char>(_file.get())} + " at " +
                                     std::to_string(_loc.line) + ":" + std::to_string(_loc.column));
        }
    }
};

}  // namespace lb::vmlua
